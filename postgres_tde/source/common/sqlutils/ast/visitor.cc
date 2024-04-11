#include "postgres_tde/source/common/sqlutils/ast/visitor.h"
#include "absl/cleanup/cleanup.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace SQLUtils {

Result Visitor::visitQuery(hsql::SQLParserResult& query) {
  table_aliases_.clear();
  column_aliases_.clear();
  from_tables_.clear();

  for (hsql::SQLStatement *stmt : query.getStatements()) {
    CHECK_RESULT(visitStatement(stmt));
  }

  return Result::ok;
}

Result Visitor::visitStatement(hsql::SQLStatement* stmt) {
  switch (stmt->type()) {
  case hsql::kStmtSelect:
    return visitSelectStatement(dynamic_cast<hsql::SelectStatement*>(stmt));
  case hsql::kStmtInsert:
    return visitInsertStatement(dynamic_cast<hsql::InsertStatement*>(stmt));
  case hsql::kStmtUpdate:
    return visitUpdateStatement(dynamic_cast<hsql::UpdateStatement*>(stmt));
  case hsql::kStmtDelete:
    return visitDeleteStatement(dynamic_cast<hsql::DeleteStatement*>(stmt));
  default:
    // TODO: DDL statements
    assert(false);
  }
}

Result Visitor::visitExpression(hsql::Expr* expr) {
  switch (expr->type) {
  case hsql::kExprOperator:
    return visitOperatorExpression(expr);
  case hsql::kExprArray:
    return visitArrayExpression(expr);
  case hsql::kExprLiteralFloat:
  case hsql::kExprLiteralString:
  case hsql::kExprLiteralInt:
  case hsql::kExprLiteralNull:
  case hsql::kExprStar:
    return Result::ok;

  case hsql::kExprColumnRef: {
    if (in_select_body_) {
      if (expr->table == nullptr) {
        return Result::makeError(
            fmt::format("postgres_tde: unable to determine the source of column {}. Please specify an explicit table/alias reference", expr->name));
      }

      // Save possible aliases to this column

      auto& table_name = getTableNameByAlias(expr->table);
      auto column = ColumnRef(table_name, expr->name);

      column_aliases_[fmt::format("{}.{}", expr->table, expr->name)] = column;
      ENVOY_LOG(error, "possible column alias: {}.{} -> ({}, {})", expr->table, expr->name, column.first, column.second);

      if (expr->alias != nullptr) {
        column_aliases_[expr->alias] = column;
        ENVOY_LOG(error, "possible column alias: {} -> ({}, {})", expr->alias, column.first, column.second);
      }
    }

    return Result::ok;
  }
  default:
    assert(false);
  }
}

Result Visitor::visitOperatorExpression(hsql::Expr* expr) {
  switch (expr->opType) {
  case hsql::kOpPlus:
  case hsql::kOpMinus:
  case hsql::kOpAsterisk:
  case hsql::kOpSlash:
  case hsql::kOpPercentage:
  case hsql::kOpCaret:
  case hsql::kOpEquals:
  case hsql::kOpNotEquals:
  case hsql::kOpLess:
  case hsql::kOpLessEq:
  case hsql::kOpGreater:
  case hsql::kOpGreaterEq:
  case hsql::kOpLike:
  case hsql::kOpNotLike:
  case hsql::kOpILike:
  case hsql::kOpAnd:
  case hsql::kOpOr:
  case hsql::kOpConcat:
    CHECK_RESULT(visitExpression(expr->expr));
    CHECK_RESULT(visitExpression(expr->expr2));
    return Result::ok;

  case hsql::kOpNot:
  case hsql::kOpUnaryMinus:
  case hsql::kOpIsNull:
  case hsql::kOpExists:
    return visitExpression(expr->expr);

  case hsql::kOpIn:
    CHECK_RESULT(visitExpression(expr->expr));
    for (hsql::Expr *exp: *expr->exprList) {
      CHECK_RESULT(visitExpression(exp));
    }

    return Result::ok;

  default:
    assert(false);
  }
}

Result Visitor::visitArrayExpression(hsql::Expr*) {
  return Result::makeError("postgres_tde: unsupported");
}

Result Visitor::visitSelectStatement(hsql::SelectStatement* stmt) {
  // Remember old state flags and restore it at the end
  // (necessary for nested selects support)
  bool in_select_body_old = in_select_body_;
  bool in_join_condition_old = in_join_condition_;
  bool in_group_by_old = in_group_by_;
  in_select_body_ = in_join_condition_ = in_group_by_ = false;
  absl::Cleanup state_flag_restorer = [&, this](){
    in_select_body_ = in_select_body_old;
    in_join_condition_ = in_join_condition_old;
    in_group_by_ = in_group_by_old;
  };

  if (stmt->unionSelect != nullptr) {
    return Result::makeError("postgres_tde: unsupported");
  }

  if (stmt->fromTable != nullptr) {
    CHECK_RESULT(visitTableRef(stmt->fromTable));
  }

  assert(stmt->selectList != nullptr);
  in_select_body_ = true;
  for (hsql::Expr *expr : *stmt->selectList) {
    CHECK_RESULT(visitExpression(expr));
  }
  in_select_body_ = false;

  if (stmt->whereClause != nullptr) {
    CHECK_RESULT(visitExpression(stmt->whereClause));
  }

  if (stmt->groupBy != nullptr) {
    in_group_by_ = true;
    hsql::GroupByDescription* desc = stmt->groupBy;
    for (hsql::Expr* column_expr : *desc->columns) {
      CHECK_RESULT(visitExpression(column_expr));
    }

    if (desc->having != nullptr) {
      CHECK_RESULT(visitExpression(desc->having));
    }
    in_group_by_ = false;
  }

  if (stmt->order != nullptr) {
    for (hsql::OrderDescription* desc : *stmt->order) {
      CHECK_RESULT(visitExpression(desc->expr));
    }
  }

  return Result::ok;
}

Result Visitor::visitTableRef(hsql::TableRef* table_ref) {
  switch (table_ref->type) {
  case hsql::kTableName:
    if (table_ref->alias != nullptr) {
      ENVOY_LOG(error, "table alias: {} -> {}", table_ref->alias->name, table_ref->name);
      table_aliases_[std::string(table_ref->alias->name)] = std::string(table_ref->name);
    }

    from_tables_.push_back(table_ref->name);
    return Result::ok;
  case hsql::kTableSelect:
    return visitSelectStatement(table_ref->select);
  case hsql::kTableJoin: {
    hsql::JoinDefinition *def = table_ref->join;

    CHECK_RESULT(visitTableRef(def->left));
    CHECK_RESULT(visitTableRef(def->right));

    in_join_condition_ = true;
    CHECK_RESULT(visitExpression(def->condition));
    in_join_condition_ = false;

    return Result::ok;
  }

  case hsql::kTableCrossProduct:
    for (hsql::TableRef* ref : *table_ref->list) {
      CHECK_RESULT(visitTableRef(ref));
    }
    return Result::ok;
  default:
    assert(false);
  }
}

Result Visitor::visitInsertStatement(hsql::InsertStatement* stmt) {
  switch (stmt->type) {
  case hsql::kInsertValues: {
    for (hsql::Expr* expr : *stmt->values) {
      CHECK_RESULT(visitExpression(expr));
    }
    return Result::ok;
  }
  case hsql::kInsertSelect:
    return visitSelectStatement(stmt->select);
  default:
    assert(false);
  }
}

Result Visitor::visitUpdateStatement(hsql::UpdateStatement* stmt) {
  assert(stmt->table->type == hsql::kTableName);

  if (stmt->table->alias != nullptr) {
    ENVOY_LOG(error, "table alias: {} -> {}", stmt->table->alias->name, stmt->table->name);
    table_aliases_[std::string(stmt->table->alias->name)] = std::string(stmt->table->name);
  }

  for (hsql::UpdateClause* update : *stmt->updates) {
    CHECK_RESULT(visitExpression(update->value));
  }

  if (stmt->where != nullptr) {
    CHECK_RESULT(visitExpression(stmt->where));
  }

  return Result::ok;
}

Result Visitor::visitDeleteStatement(hsql::DeleteStatement*) {
  return Result::makeError("postgres_tde: unsupported");
  // TODO:
}

const std::string& Visitor::getTableNameByAlias(const std::string& alias) const {
  if (table_aliases_.find(alias) == table_aliases_.end()) {
    return alias;
  }

  return table_aliases_.at(alias);
}

const ColumnRef* Visitor::getColumnByAlias(const std::string& alias) {
  if (column_aliases_.find(alias) == column_aliases_.end()) {
    if (from_tables_.empty()) {
      return nullptr;
    }

    // Allow direct access to the primary table from the FROM clause
    column_aliases_[alias] = ColumnRef(from_tables_[0], alias);
    return getColumnByAlias(alias);
  }

  return &column_aliases_.at(alias);
}

} // namespace SQLUtils
} // namespace Common
} // namespace Extensions
} // namespace Envoy
