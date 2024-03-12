#pragma once

#include <sstream>
#include "include/sqlparser/SQLParser.h"
#include "postgres_tde/source/common/sqlutils/ast/visitor.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace SQLUtils {

using Common::Utils::Result;

// AST dumper
class DumpVisitor : public Visitor {
public:
  DumpVisitor() = default;

  Result visitQuery(hsql::SQLParserResult& query) override;
  std::string getResult() const;

protected:
  Result visitExpression(hsql::Expr* expr) override;

  Result visitOperatorExpression(hsql::Expr* expr) override;

  Result visitTableRef(hsql::TableRef* table_ref) override;

  Result visitSelectStatement(hsql::SelectStatement* stmt) override;
  Result visitInsertStatement(hsql::InsertStatement* stmt) override;
  Result visitUpdateStatement(hsql::UpdateStatement* stmt) override;

  static const char* operatorToString(hsql::OperatorType type);

protected:
  std::stringstream query_str_;
};

} // namespace SQLUtils
} // namespace Common
} // namespace Extensions
} // namespace Envoy
