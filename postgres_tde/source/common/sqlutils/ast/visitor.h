#pragma once

#include "include/sqlparser/SQLParser.h"
#include "envoy/common/platform.h"
#include "source/common/common/logger.h"
#include "postgres_tde/source/common/utils/utils.h"

#define CHECK_RESULT(EXPR) { \
  Result result = (EXPR); \
  if (!result.isOk) { \
    return result; \
  } \
}

namespace Envoy {
namespace Extensions {
namespace Common {
namespace SQLUtils {

using Common::Utils::Result;

class ColumnRef: public std::pair<std::string, std::string> {
public:
  // Inherit constructors
  using pair::pair;

  auto& table() const { return first; }
  auto& column() const { return second; }
};

// Helper class that provides base traversal routines for SQL query's AST
class Visitor : public Logger::Loggable<Logger::Id::filter> {
public:
  virtual ~Visitor() = default;

  virtual Result visitQuery(hsql::SQLParserResult& query);

protected:
  virtual Result visitStatement(hsql::SQLStatement* stmt);
  virtual Result visitExpression(hsql::Expr* expr);

  virtual Result visitOperatorExpression(hsql::Expr* expr);
  virtual Result visitArrayExpression(hsql::Expr* expr);

  virtual Result visitTableRef(hsql::TableRef* table_ref);

  virtual Result visitSelectStatement(hsql::SelectStatement* stmt);
  virtual Result visitInsertStatement(hsql::InsertStatement* stmt);
  virtual Result visitUpdateStatement(hsql::UpdateStatement* stmt);
  virtual Result visitDeleteStatement(hsql::DeleteStatement* stmt);

  std::string getTableNameByAlias(const std::string& alias) const;
  const ColumnRef* getSelectColumnByAlias(const std::string& alias) const;
  bool isColumnSelected(const ColumnRef& column) const;

protected:
  bool in_select_body_{false};
  bool in_join_condition_{false};
  bool in_group_by_{false};

  std::unordered_map<std::string, std::string> table_aliases_;
  std::set<ColumnRef> select_columns_;
  std::unordered_map<std::string, const ColumnRef*> select_column_aliases_;
};

using VisitorPtr = std::unique_ptr<Visitor>;

} // namespace SQLUtils
} // namespace Common
} // namespace Extensions
} // namespace Envoy
