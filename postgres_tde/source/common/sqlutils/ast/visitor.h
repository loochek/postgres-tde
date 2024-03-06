#pragma once

#include "include/sqlparser/SQLParser.h"
#include "envoy/common/platform.h"
#include "source/common/common/logger.h"
#include "postgres_tde/source/common/utils/utils.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace SQLUtils {

using Common::Utils::Result;

// Helper class that provides base traversal routines for SQL query's AST
class Visitor : public Logger::Loggable<Logger::Id::filter> {
protected:
  virtual ~Visitor() = default;

  virtual Result visitQuery(hsql::SQLParserResult& query);

  virtual Result visitStatement(hsql::SQLStatement* stmt);
  virtual Result visitExpression(hsql::Expr* expr);

  virtual Result visitOperatorExpression(hsql::Expr* expr);
  virtual Result visitArrayExpression(hsql::Expr* expr);

  virtual Result visitTableRef(hsql::TableRef* table_ref);

  virtual Result visitSelectStatement(hsql::SelectStatement* stmt);
  virtual Result visitInsertStatement(hsql::InsertStatement* stmt);
  virtual Result visitUpdateStatement(hsql::UpdateStatement* stmt);
  virtual Result visitDeleteStatement(hsql::DeleteStatement* stmt);

//  static std::string dumpCreateStatement(const hsql::SQLStatement& stmt);
//  static std::string dumpAlterStatement(const hsql::SQLStatement& stmt);
//  static std::string dumpDropStatement(const hsql::SQLStatement& stmt);

  const std::string& getTableNameByAlias(const std::string& alias);

protected:
  bool in_select_body_{false};
  bool in_join_condition_{false};
  bool in_group_by_{false};

  std::unordered_map<std::string, std::string> alias2table_;
};

} // namespace SQLUtils
} // namespace Common
} // namespace Extensions
} // namespace Envoy
