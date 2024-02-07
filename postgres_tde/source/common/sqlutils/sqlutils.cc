#include "postgres_tde/source/common/sqlutils/sqlutils.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace SQLUtils {

bool SQLUtils::setMetadata(const std::string& query, const DecoderAttributes& attr,
                           ProtobufWkt::Struct& metadata) {
  hsql::SQLParserResult result;

  hsql::SQLParser::parse(query, &result);

  if (!result.isValid()) {
    return false;
  }

  std::string database;
  // Check if the attributes map contains database name.
  const auto it = attr.find("database");
  if (it != attr.end()) {
    database = absl::StrCat(".", it->second);
  }

  auto& fields = *metadata.mutable_fields();

  for (auto i = 0u; i < result.size(); ++i) {
    if (result.getStatement(i)->type() == hsql::StatementType::kStmtShow) {
      continue;
    }
    hsql::TableAccessMap table_access_map;
    // Get names of accessed tables.
    result.getStatement(i)->tablesAccessed(table_access_map);
    for (auto& it : table_access_map) {
      auto& operations = *fields[it.first + database].mutable_list_value();
      // For each table get names of operations performed on that table.
      for (const auto& ot : it.second) {
        operations.add_values()->set_string_value(ot);
      }
    }
  }

  return true;
}

bool SQLUtils::dumpQuery(const hsql::SQLParserResult& query, std::string& result) {
  std::stringstream out;
  for (const hsql::SQLStatement *stmt : query.getStatements()) {
    if (!dumpStatement(out, stmt)) {
      return false;
    }
    out << "; ";
  }

  result = out.str();
  return true;
}

bool SQLUtils::dumpStatement(std::stringstream &out, const hsql::SQLStatement* stmt) {
  switch (stmt->type()) {
  case hsql::kStmtSelect:
    return dumpSelectStatement(out, dynamic_cast<const hsql::SelectStatement*>(stmt));
  case hsql::kStmtInsert:
    return dumpInsertStatement(out, dynamic_cast<const hsql::InsertStatement*>(stmt));
  case hsql::kStmtUpdate:
    return dumpUpdateStatement(out, dynamic_cast<const hsql::UpdateStatement*>(stmt));
  case hsql::kStmtDelete:
    return dumpDeleteStatement(out, dynamic_cast<const hsql::DeleteStatement*>(stmt));
  default:
    return false;
  }
}

bool SQLUtils::dumpExpression(std::stringstream &out, const hsql::Expr* expr) {
  switch (expr->type) {
  case hsql::kExprStar:
    out << "*";
    return true;
  case hsql::kExprColumnRef:
    out << expr->name;
    return true;
  case hsql::kExprLiteralFloat:
    out << expr->fval;
    return true;
  case hsql::kExprLiteralInt:
    out << expr->ival;
    return true;
  case hsql::kExprLiteralString:
    out << "'" << expr->name << "'";
    return true;
  case hsql::kExprFunctionRef:
    // TODO:
//    inprint(expr->name, numIndent);
//    for (Expr* e : *expr->exprList) printExpression(e, numIndent + 1);
    return false;
  case hsql::kExprOperator:
    return dumpOperatorExpression(out, expr);
  case hsql::kExprSelect:
//    printSelectStatementInfo(expr->select, numIndent);
    return false;
  case hsql::kExprParameter:
//    inprint(expr->ival, numIndent);
    return false;
  case hsql::kExprArray:
    return dumpArrayExpression(out, expr);
  case hsql::kExprArrayIndex:
//    printExpression(expr->expr, numIndent + 1);
//    inprint(expr->ival, numIndent);
    return false;
  default:
    return false;
  }
}

bool SQLUtils::dumpSelectStatement(std::stringstream &out, const hsql::SelectStatement* stmt) {
  if ((stmt->unionSelect != nullptr) || (stmt->selectList == nullptr)) {
    return false;
  }

  out << "SELECT ";
  for (size_t i = 0; i < stmt->selectList->size(); i++) {
    if (!dumpExpression(out, (*stmt->selectList)[i])) return false;
    if (i < stmt->selectList->size() - 1) {
      out << ", ";
    }
  }

  if (stmt->fromTable != nullptr) {
    if (stmt->fromTable->type != hsql::kTableName) return false;
    out << " FROM " << stmt->fromTable->name;
  }

  if (stmt->whereClause != nullptr) {
    out << " WHERE ";
    if (!dumpExpression(out, stmt->whereClause)) return false;
  }

  if (stmt->order != nullptr) {
    out << " ORDER BY ";
    for (size_t i = 0; i < stmt->order->size(); i++) {
      hsql::OrderDescription* desc = (*stmt->order)[i];
      if (!dumpExpression(out, desc->expr)) return false;
      out << (desc->type == hsql::kOrderAsc ? "ASC" : "DESC");
      if (i < stmt->selectList->size() - 1) {
        out << ", ";
      }
    }
  }

  if (stmt->limit != nullptr) {
    if (stmt->limit->limit != hsql::kNoLimit) {
      out << " LIMIT " << stmt->limit->limit;
    }

    if (stmt->limit->offset != hsql::kNoOffset) {
      out << " OFFSET " << stmt->limit->offset;
    }
  }

  return true;
}

bool SQLUtils::dumpOperatorExpression(std::stringstream& out, const hsql::Expr* expr) {
  bool result = true;

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
    out << "(";
    result &= dumpExpression(out, expr->expr);
    out << ") " << operatorToString(expr->opType) << " (";
    result &= dumpExpression(out, expr->expr2);
    out << ")";
    return result;

  case hsql::kOpNot:
  case hsql::kOpUnaryMinus:
  case hsql::kOpIsNull:
  case hsql::kOpExists:
    out << operatorToString(expr->opType) << " ";
    out << "(";
    result &= dumpExpression(out, expr->expr);
    out << ")";
    return result;

  case hsql::kOpIn:
    out << "(";
    result &= dumpExpression(out, expr->expr);
    out << ") " << operatorToString(expr->opType) << " (";
    for (size_t i = 0; i < expr->exprList->size(); i++) {
      result &= dumpExpression(out, (*expr->exprList)[i]);
      if (i < expr->exprList->size() - 1) {
        out << ", ";
      }
    }
    out << ")";
    return result;

  default:
    return false;
  }
}

bool SQLUtils::dumpArrayExpression(std::stringstream& out, const hsql::Expr* expr) {
  bool result = true;

  out << "(";
  for (size_t i = 0; i < expr->exprList->size(); i++) {
    result &= dumpExpression(out, (*expr->exprList)[i]);
    if (i < expr->exprList->size() - 1) {
      out << ", ";
    }
  }
  out << ")";

  return result;
}

bool SQLUtils::dumpInsertStatement(std::stringstream&, const hsql::InsertStatement*) {
  return false;
}

bool SQLUtils::dumpUpdateStatement(std::stringstream&, const hsql::UpdateStatement*) {
  return false;
}

bool SQLUtils::dumpDeleteStatement(std::stringstream&, const hsql::DeleteStatement*) {
  return false;
}

std::string SQLUtils::operatorToString(hsql::OperatorType type) {
  switch (type) {
  case hsql::kOpNone:
    assert(false);
    return "";
  // Ternary operator
  case hsql::kOpBetween:
    assert(false);
    return "";
  // n-nary special case
  case hsql::kOpCase:
    assert(false);
    return "";
  case hsql::kOpCaseListElement:  // `WHEN expr THEN expr`
    assert(false);
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
    assert(false);
  }
}

} // namespace SQLUtils
} // namespace Common
} // namespace Extensions
} // namespace Envoy
