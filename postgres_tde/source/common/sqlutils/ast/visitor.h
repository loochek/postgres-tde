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

  const std::string& getTableNameByAlias(const std::string& alias);

protected:
  bool in_select_body_{false};
  bool in_join_condition_{false};
  bool in_group_by_{false};

  std::unordered_map<std::string, std::string> alias2table_;
};

using VisitorPtr = std::unique_ptr<Visitor>;

} // namespace SQLUtils
} // namespace Common
} // namespace Extensions
} // namespace Envoy
