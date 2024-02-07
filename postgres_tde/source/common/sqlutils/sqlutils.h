#pragma once

#include "source/common/protobuf/utility.h"

#include "include/sqlparser/SQLParser.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace SQLUtils {

class SQLUtils {
public:
  using DecoderAttributes = std::map<std::string, std::string>;
  /**
   * Method parses SQL query string and writes output to metadata.
   * @param query supplies SQL statement.
   * @param attr supplies attributes which cannot be extracted from SQL query but are
   *    required to create proper metadata. For example database name may be sent
   *    by a client when it initially connects to the server, not along each SQL query.
   * @param metadata supplies placeholder where metadata should be written.
   * @return True if parsing was successful and False if parsing failed.
   *         If True was returned the metadata contains result of parsing. The results are
   *         stored in metadata.mutable_fields.
   **/
  static bool setMetadata(const std::string& query, const DecoderAttributes& attr,
                          ProtobufWkt::Struct& metadata);

  static bool dumpQuery(const hsql::SQLParserResult& query, std::string& result);

private:
  static bool dumpStatement(std::stringstream &out, const hsql::SQLStatement* stmt);
  static bool dumpExpression(std::stringstream &out, const hsql::Expr* expr);

  static bool dumpOperatorExpression(std::stringstream &out, const hsql::Expr* expr);
  static bool dumpArrayExpression(std::stringstream &out, const hsql::Expr* expr);

  static bool dumpSelectStatement(std::stringstream &out, const hsql::SelectStatement* stmt);
  static bool dumpInsertStatement(std::stringstream &out, const hsql::InsertStatement* stmt);
  static bool dumpUpdateStatement(std::stringstream &out, const hsql::UpdateStatement* stmt);
  static bool dumpDeleteStatement(std::stringstream &out, const hsql::DeleteStatement* stmt);

//  static std::string dumpCreateStatement(const hsql::SQLStatement& stmt);
//  static std::string dumpAlterStatement(const hsql::SQLStatement& stmt);
//  static std::string dumpDropStatement(const hsql::SQLStatement& stmt);

  static std::string operatorToString(hsql::OperatorType type);
};

} // namespace SQLUtils
} // namespace Common
} // namespace Extensions
} // namespace Envoy
