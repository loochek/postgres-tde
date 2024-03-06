#include "postgres_tde/source/common/sqlutils/ast/visitor.h"
#include "absl/cleanup/cleanup.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace SQLUtils {

Result Visitor::visitQuery(hsql::SQLParserResult& query) {
  alias2table_.clear();
  for (hsql::SQLStatement *stmt : query.getStatements()) {
    Result result = visitStatement(stmt);
    if (!result.isOk) {
      return result;
    }
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
  case hsql::kExprColumnRef:
    return Result::ok;
  default:
    assert(false);
  }
}

Result Visitor::visitOperatorExpression(hsql::Expr* expr) {
  Result result(true);

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
    result = visitExpression(expr->expr);
    if (!result.isOk) {
      return result;
    }

    result = visitExpression(expr->expr2);
    if (!result.isOk) {
      return result;
    }
    return Result::ok;

  case hsql::kOpNot:
  case hsql::kOpUnaryMinus:
  case hsql::kOpIsNull:
  case hsql::kOpExists:
    return visitExpression(expr->expr);

  case hsql::kOpIn:
    result = visitExpression(expr->expr);
    if (!result.isOk) {
      return result;
    }
    for (hsql::Expr *exp: *expr->exprList) {
      result = visitExpression(exp);
      if (!result.isOk) {
        return result;
      }
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
    Result result = visitTableRef(stmt->fromTable);
    if (!result.isOk) {
      return result;
    }
  }

  assert(stmt->selectList != nullptr);
  in_select_body_ = true;
  for (hsql::Expr *expr : *stmt->selectList) {
    Result result = visitExpression(expr);
    if (!result.isOk) {
      return result;
    }
  }
  in_select_body_ = false;

  if (stmt->whereClause != nullptr) {
    Result result = visitExpression(stmt->whereClause);
    if (!result.isOk) {
      return result;
    }
  }

  if (stmt->groupBy != nullptr) {
    in_group_by_ = true;
    hsql::GroupByDescription* desc = stmt->groupBy;
    for (hsql::Expr* column_expr : *desc->columns) {
      Result result = visitExpression(column_expr);
      if (!result.isOk) {
        return result;
      }
    }

    if (desc->having != nullptr) {
      Result result = visitExpression(desc->having);
      if (!result.isOk) {
        return result;
      }
    }
    in_group_by_ = false;
  }

  if (stmt->order != nullptr) {
    for (hsql::OrderDescription* desc : *stmt->order) {
      Result result = visitExpression(desc->expr);
      if (!result.isOk) {
        return result;
      }
    }
  }

  return Result::ok;
}

Result Visitor::visitTableRef(hsql::TableRef* table_ref) {
  switch (table_ref->type) {
  case hsql::kTableName:
    if (table_ref->alias != nullptr) {
      ENVOY_LOG(error, "column alias: {} -> {}", table_ref->alias->name, table_ref->name);
      alias2table_[std::string(table_ref->alias->name)] = std::string(table_ref->name);
    }
    return Result::ok;
  case hsql::kTableSelect:
    return visitSelectStatement(table_ref->select);
  case hsql::kTableJoin: {
    hsql::JoinDefinition *def = table_ref->join;
    Result result = visitTableRef(def->left);
    if (!result.isOk) {
      return result;
    }

    result = visitTableRef(def->right);
    if (!result.isOk) {
      return result;
    }

    in_join_condition_ = true;
    result = visitExpression(def->condition);
    in_join_condition_ = false;
    if (!result.isOk) {
      return result;
    }
    return Result::ok;
  }

  case hsql::kTableCrossProduct:
    for (hsql::TableRef* ref : *table_ref->list) {
      Result result = visitTableRef(ref);
      if (!result.isOk) {
        return result;
      }
    }
    return Result::ok;
  default:
    assert(false);
  }
}

Result Visitor::visitInsertStatement(hsql::InsertStatement*) {
  // TODO:
  return Result::ok;
}

Result Visitor::visitUpdateStatement(hsql::UpdateStatement*) {
  // TODO:
  return Result::ok;
}

Result Visitor::visitDeleteStatement(hsql::DeleteStatement*) {
  // TODO:
  return Result::ok;
}

const std::string& Visitor::getTableNameByAlias(const std::string& alias) {
  if (alias2table_.find(alias) == alias2table_.end()) {
    return alias;
  }

  return alias2table_[alias];
}

} // namespace SQLUtils
} // namespace Common
} // namespace Extensions
} // namespace Envoy
