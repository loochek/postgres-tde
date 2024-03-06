#include "postgres_tde/source/filters/network/postgres_tde/mutators/blind_index.h"
#include "source/common/crypto/utility.h"
#include "source/common/common/fmt.h"
#include "absl/strings/escaping.h"
#include "absl/cleanup/cleanup.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

BlindIndexMutator::BlindIndexMutator(DatabaseEncryptionConfig *config) : config_(config) {}

Result BlindIndexMutator::mutateQuery(hsql::SQLParserResult& query) {
  Result result = Visitor::visitQuery(query);
  if (!result.isOk) {
    return result;
  }

  result = mutateComparisons();
  if (!result.isOk) {
    return result;
  }

  result = mutateGroupByExpressions();
  if (!result.isOk) {
    return result;
  }

  return result;
}

void BlindIndexMutator::mutateResult() {}

Result BlindIndexMutator::visitExpression(hsql::Expr* expr) {
  switch (expr->type) {
  case hsql::kExprColumnRef: {
    if (in_select_body_) {
      // SELECT of possibly indexed column is ok
      return Result::ok;
    } else if (in_group_by_) {
      // GROUP BY possibly indexed column is ok, but it's necessary to replace it to corresponding BI column
      group_by_mutation_candidates_.push_back(expr);
      return Result::ok;
    }

    // Other usages should be prohibited

    if (expr->table == nullptr) {
      return Result::makeError(
          fmt::format("postgres_tde: unable to determine the source of column {}. Please specify an explicit table/alias reference", expr->name));
    }

    const std::string& table_name = getTableNameByAlias(expr->table);
    ColumnConfig *column_config = config_->getColumnConfig(table_name, expr->name);
    if (column_config != nullptr && column_config->hasBlindIndex()) {
      return Result::makeError(
          fmt::format("postgres_tde: invalid use of blind indexed column {}.{}", table_name, expr->name));
    } else {
      return Result::ok;
    }
  }
  default:
    return Visitor::visitExpression(expr);
  }
}


Result BlindIndexMutator::visitOperatorExpression(hsql::Expr* expr) {
  switch (expr->opType) {
  case hsql::kOpEquals:
  case hsql::kOpNotEquals:
    if (expr->expr->isType(hsql::kExprColumnRef) && expr->expr2->isLiteral()) {
      ENVOY_LOG(error, "blind index candidate: {} {}", expr->expr->name, expr->expr2->name);
      comparison_mutation_candidates_.push_back(expr);
      return Result::ok;
    }
    return Visitor::visitOperatorExpression(expr);
  default:
    return Visitor::visitOperatorExpression(expr);
  }
}

Result BlindIndexMutator::mutateComparisons() {
  absl::Cleanup cleanup = [this]() {
    comparison_mutation_candidates_.clear();
  };

  for (hsql::Expr* expr : comparison_mutation_candidates_) {
    assert(expr->isType(hsql::kExprOperator)
           && (expr->opType == hsql::OperatorType::kOpEquals || expr->opType == hsql::OperatorType::kOpNotEquals)
           && expr->expr->isType(hsql::kExprColumnRef) && expr->expr2->isLiteral());

    hsql::Expr *column = expr->expr;
    hsql::Expr *literal = expr->expr2;

    if (column->table == nullptr) {
      return Result::makeError(
        fmt::format("postgres_tde: unable to determine the source of column {}. Please specify an explicit table/alias reference", column->name));
    }

    const std::string& table_name = getTableNameByAlias(column->table);
    ColumnConfig* column_config = config_->getColumnConfig(table_name, column->name);
    if (column_config == nullptr || !column_config->hasBlindIndex()) {
      ENVOY_LOG(error, "blind index is not configured for {}.{}", column->table, column->name);
      continue;
    }

    const std::string& bi_column_name = column_config->BIColumnName();
    replaceHyriseName(column, bi_column_name);

    replaceLiteralWithHash(literal, column_config);
  }

  return Result::ok;
}

Result BlindIndexMutator::mutateGroupByExpressions() {
  absl::Cleanup cleanup = [this]() {
    group_by_mutation_candidates_.clear();
  };

  for (hsql::Expr* column : group_by_mutation_candidates_) {
    assert(column->isType(hsql::kExprColumnRef));

    if (column->table == nullptr) {
      return Result::makeError(
          fmt::format("postgres_tde: unable to determine the source of column {}. Please specify an explicit table/alias reference", column->name));
    }

    const std::string& table_name = getTableNameByAlias(column->table);
    ColumnConfig* column_config = config_->getColumnConfig(table_name, column->name);
    if (column_config == nullptr || !column_config->hasBlindIndex()) {
      ENVOY_LOG(error, "blind index is not configured for the column {}.{}", column->table, column->name);
      continue;
    }

    const std::string& bi_column_name = column_config->BIColumnName();
    replaceHyriseName(column, bi_column_name);
  }

  return Result::ok;
}

void BlindIndexMutator::replaceLiteralWithHash(hsql::Expr* expr, ColumnConfig *column_config) {
  assert(expr->isLiteral());

  switch (expr->type) {
  case hsql::kExprLiteralNull:
    // do nothing with null values
    return;
  case hsql::kExprLiteralString: {
    const std::string& hmac_hex_str = generateHMACString(
        std::string_view(static_cast<const char*>(expr->name), strlen(expr->name) + 1),
        column_config
    );
    replaceHyriseName(expr, hmac_hex_str);
    return;
  }
  case hsql::kExprLiteralInt: {
    const std::string& hmac_hex_str = generateHMACString(
        std::string_view(reinterpret_cast<const char*>(&expr->ival), sizeof(expr->ival)),
        column_config
    );
    replaceHyriseName(expr, hmac_hex_str);
    expr->type = hsql::kExprLiteralString;
    return;
  }
  case hsql::kExprLiteralFloat: {
    const std::string& hmac_hex_str = generateHMACString(
        std::string_view(reinterpret_cast<const char*>(&expr->fval), sizeof(expr->fval)),
        column_config
    );
    replaceHyriseName(expr, hmac_hex_str);
    expr->type = hsql::kExprLiteralString;
    return;
  }
  default:
    assert(false);
  }
}

std::string BlindIndexMutator::generateHMACString(absl::string_view data, ColumnConfig *column_config) {
  auto& crypto_util = Envoy::Common::Crypto::UtilitySingleton::get();
  auto hmac = crypto_util.getSha256Hmac(column_config->BIKey(), data);

  std::string hmac_hex_str = std::string("\\x");
  hmac_hex_str.append(absl::BytesToHexString(absl::string_view(reinterpret_cast<const char*>(hmac.data()), hmac.size())));
  return hmac_hex_str;
}

void BlindIndexMutator::replaceHyriseName(hsql::Expr* expr, const std::string& str) {
  free(expr->name);
  expr->name = static_cast<char*>(calloc(str.size() + 1, sizeof(char)));
  memcpy(expr->name, str.data(), str.size());
}

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
