#include "postgres_tde/source/common/sqlutils/ast/dump_visitor.h"
#include "source/common/common/assert.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace SQLUtils {

Result DumpVisitor::visitQuery(hsql::SQLParserResult& query) {
  query_str_ = std::stringstream();

  for (hsql::SQLStatement* stmt : query.getStatements()) {
    CHECK_RESULT(visitStatement(stmt));
    query_str_ << "; ";
  }

  return Result::ok;
}

std::string DumpVisitor::getResult() const { return query_str_.str(); }

Result DumpVisitor::visitExpression(hsql::Expr* expr) {
  switch (expr->type) {
  case hsql::kExprStar:
    query_str_ << "*";
    return Result::ok;
  case hsql::kExprColumnRef:
    if (expr->table != nullptr) {
      query_str_ << expr->table << ".";
    }
    query_str_ << expr->name;
    if (expr->alias != nullptr) {
      query_str_ << " AS " << expr->alias;
    }
    return Result::ok;
  case hsql::kExprLiteralFloat:
    query_str_ << expr->fval;
    return Result::ok;
  case hsql::kExprLiteralInt:
    query_str_ << expr->ival;
    return Result::ok;
  case hsql::kExprLiteralString:
    query_str_ << "'" << expr->name << "'";
    return Result::ok;
  case hsql::kExprLiteralNull:
    query_str_ << "NULL";
    return Result::ok;
  case hsql::kExprOperator:
    return visitOperatorExpression(expr);
  case hsql::kExprSelect: {
    query_str_ << "(";
    CHECK_RESULT(visitSelectStatement(expr->select));
    query_str_ << ")";
    return Result::ok;
  }
  case hsql::kExprArray:
    return visitArrayExpression(expr);
  default:
    return Result::makeError("postgres_tde: unable to dump query");
  }
}

Result DumpVisitor::visitOperatorExpression(hsql::Expr* expr) {
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
    query_str_ << "(";
    CHECK_RESULT(visitExpression(expr->expr));
    query_str_ << ") " << operatorToString(expr->opType) << " (";
    CHECK_RESULT(visitExpression(expr->expr2));
    query_str_ << ")";
    return Result::ok;

  case hsql::kOpNot:
  case hsql::kOpUnaryMinus:
  case hsql::kOpExists:
    query_str_ << operatorToString(expr->opType) << " ";
    query_str_ << "(";
    CHECK_RESULT(visitExpression(expr->expr));
    query_str_ << ")";
    return Result::ok;

  case hsql::kOpIsNull:
    query_str_ << "(";
    CHECK_RESULT(visitExpression(expr->expr));
    query_str_ << ") " << operatorToString(expr->opType);
    return Result::ok;

  case hsql::kOpIn:
    query_str_ << "(";
    CHECK_RESULT(visitExpression(expr->expr));
    query_str_ << ") " << operatorToString(expr->opType) << " (";
    for (size_t i = 0; i < expr->exprList->size(); i++) {
      CHECK_RESULT(visitExpression((*expr->exprList)[i]));
      if (i < expr->exprList->size() - 1) {
        query_str_ << ", ";
      }
    }
    query_str_ << ")";
    return Result::ok;

  default:
    PANIC("not implemented");;
  }
}

Result DumpVisitor::visitSelectStatement(hsql::SelectStatement* stmt) {
  if (stmt->unionSelect != nullptr) {
    return Result::makeError("postgres_tde: unable to dump query");
  }

  query_str_ << "SELECT ";

  ASSERT(stmt->selectList != nullptr);
  in_select_body_ = true;
  for (size_t i = 0; i < stmt->selectList->size(); i++) {
    CHECK_RESULT(visitExpression((*stmt->selectList)[i]));
    if (i < stmt->selectList->size() - 1) {
      query_str_ << ", ";
    }
  }

  if (stmt->fromTable != nullptr) {
    query_str_ << " FROM ";
    CHECK_RESULT(visitTableRef(stmt->fromTable));
  }

  if (stmt->whereClause != nullptr) {
    query_str_ << " WHERE ";
    CHECK_RESULT(visitExpression(stmt->whereClause));
  }

  if (stmt->groupBy != nullptr) {
    query_str_ << " GROUP BY ";

    hsql::GroupByDescription* desc = stmt->groupBy;
    for (size_t i = 0; i < desc->columns->size(); i++) {
      CHECK_RESULT(visitExpression((*desc->columns)[i]));
      if (i < desc->columns->size() - 1) {
        query_str_ << ", ";
      }
    }

    if (desc->having != nullptr) {
      query_str_ << " HAVING ";
      CHECK_RESULT(visitExpression(desc->having));
    }
  }

  if (stmt->order != nullptr) {
    query_str_ << " ORDER BY ";
    for (size_t i = 0; i < stmt->order->size(); i++) {
      hsql::OrderDescription* desc = (*stmt->order)[i];
      CHECK_RESULT(visitExpression(desc->expr));
      query_str_ << (desc->type == hsql::kOrderAsc ? " ASC" : " DESC");
      if (i < stmt->order->size() - 1) {
        query_str_ << ", ";
      }
    }
  }

  if (stmt->limit != nullptr) {
    if (stmt->limit->limit != hsql::kNoLimit) {
      query_str_ << " LIMIT " << stmt->limit->limit;
    }

    if (stmt->limit->offset != hsql::kNoOffset) {
      query_str_ << " OFFSET " << stmt->limit->offset;
    }
  }

  return Result::ok;
}

Result DumpVisitor::visitTableRef(hsql::TableRef* table_ref) {
  switch (table_ref->type) {
  case hsql::kTableName:
    query_str_ << table_ref->name;
    if (table_ref->alias != nullptr) {
      query_str_ << " " << table_ref->alias->name;
    }
    return Result::ok;
  case hsql::kTableSelect: {
    query_str_ << "(";
    CHECK_RESULT(visitSelectStatement(table_ref->select));
    query_str_ << ")";
    return Result::ok;
  }
  case hsql::kTableJoin: {
    hsql::JoinDefinition* def = table_ref->join;
    CHECK_RESULT(visitTableRef(def->left));
    query_str_ << " JOIN ";
    CHECK_RESULT(visitTableRef(def->right));

    if (def->condition != nullptr) {
      query_str_ << " ON (";
      CHECK_RESULT(visitExpression(def->condition));
      query_str_ << ")";
    }
    return Result::ok;
  }

  case hsql::kTableCrossProduct:
    for (size_t i = 0; i < table_ref->list->size(); i++) {
      CHECK_RESULT(visitTableRef((*table_ref->list)[i]));
      if (i < table_ref->list->size() - 1) {
        query_str_ << ", ";
      }
    }
    return Result::ok;
  default:
    PANIC("not implemented");;
  }
}

Result DumpVisitor::visitInsertStatement(hsql::InsertStatement* stmt) {
  query_str_ << "INSERT INTO " << stmt->tableName;
  switch (stmt->type) {
  case hsql::kInsertValues: {
    query_str_ << " (";
    for (size_t i = 0; i < stmt->columns->size(); i++) {
      query_str_ << (*stmt->columns)[i];
      if (i < stmt->columns->size() - 1) {
        query_str_ << ", ";
      }
    }
    query_str_ << ") VALUES (";

    for (size_t i = 0; i < stmt->values->size(); i++) {
      CHECK_RESULT(visitExpression((*stmt->values)[i]));
      if (i < stmt->values->size() - 1) {
        query_str_ << ", ";
      }
    }

    query_str_ << ")";
    return Result::ok;
  }
  case hsql::kInsertSelect:
    query_str_ << " ";
    return visitSelectStatement(stmt->select);
  default:
    PANIC("not implemented");;
  }
}

Result DumpVisitor::visitUpdateStatement(hsql::UpdateStatement* stmt) {
  ASSERT(stmt->table->type == hsql::kTableName);
  ASSERT(!stmt->updates->empty());

  query_str_ << "UPDATE " << stmt->table->name << " SET ";

  for (size_t i = 0; i < stmt->updates->size(); i++) {
    hsql::UpdateClause *update = (*stmt->updates)[i];
    query_str_ << update->column << " = ";
    CHECK_RESULT(visitExpression(update->value));
    if (i < stmt->updates->size() - 1) {
      query_str_ << ", ";
    }
  }

  if (stmt->where != nullptr) {
    query_str_ << " WHERE ";
    CHECK_RESULT(visitExpression(stmt->where));
  }

  return Result::ok;
}

const char* DumpVisitor::operatorToString(hsql::OperatorType type) {
  switch (type) {
  case hsql::kOpNone:
    PANIC("not implemented");;
    return "";
  // Ternary operator
  case hsql::kOpBetween:
    PANIC("not implemented");
    return "";
  // n-nary special case
  case hsql::kOpCase:
    PANIC("not implemented");
    return "";
  case hsql::kOpCaseListElement: // `WHEN expr THEN expr`
    PANIC("not implemented");
    return "";
  // Binary operators.
  case hsql::kOpPlus:
    return "+";
  case hsql::kOpMinus:
    return "-";
  case hsql::kOpAsterisk:
    return "*";
  case hsql::kOpSlash:
    return "/";
  case hsql::kOpPercentage:
    return "%";
  case hsql::kOpCaret:
    return "";
  case hsql::kOpEquals:
    return "=";
  case hsql::kOpNotEquals:
    return "!=";
  case hsql::kOpLess:
    return "<";
  case hsql::kOpLessEq:
    return "<=";
  case hsql::kOpGreater:
    return ">";
  case hsql::kOpGreaterEq:
    return ">=";
  case hsql::kOpLike:
    return "LIKE";
  case hsql::kOpNotLike:
    return "NOT LIKE";
  case hsql::kOpILike:
    return "ILIKE";
  case hsql::kOpAnd:
    return "AND";
  case hsql::kOpOr:
    return "OR";
  case hsql::kOpIn:
    return "IN";
  case hsql::kOpConcat:
    return "||";

  // Unary operators.
  case hsql::kOpNot:
    return "NOT";
  case hsql::kOpUnaryMinus:
    return "-";
  case hsql::kOpIsNull:
    return "IS NULL";
  case hsql::kOpExists:
    return "EXISTS";
  default:
    PANIC("not implemented");;
  }
}

} // namespace SQLUtils
} // namespace Common
} // namespace Extensions
} // namespace Envoy
