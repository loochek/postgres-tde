#include "postgres_tde/source/filters/network/postgres_tde/mutators/encryption.h"
#include "postgres_tde/source/common/crypto/utility_ext.h"
#include "source/common/common/fmt.h"
#include "absl/strings/escaping.h"
#include "absl/cleanup/cleanup.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

EncryptionMutator::EncryptionMutator(DatabaseEncryptionConfig* config) : config_(config) {}

Result EncryptionMutator::mutateQuery(hsql::SQLParserResult& query) {
  CHECK_RESULT(Visitor::visitQuery(query));
  CHECK_RESULT(mutateInsertStatement());
  CHECK_RESULT(mutateUpdateStatement());

  //  auto& crypto_util_ext = Envoy::Common::Crypto::UtilityExtSingleton::get();
  //  auto key = std::vector<uint8_t>({'a', 'e', 'g', 'r', 'j', 'a', 'd', 'f', 'n', 'a', 'd', 'f',
  //  'i', 'a', 'd', 'f', 'g', 'i', 'd', 'f', 'a', 'i', 'd', 's', 'f', 'g', 's', 'd', 'h', 'n', 'g',
  //  'h'}); std::string plain("а"); auto ciphertext = crypto_util_ext.AESEncrypt(key,
  //  absl::string_view(plain.data(), plain.size() + 1)); ENVOY_LOG(error, "ciphertext: {}",
  //  reinterpret_cast<const char*>(ciphertext.data())); auto decrypted =
  //  crypto_util_ext.AESDecrypt(key, absl::string_view(reinterpret_cast<const
  //  char*>(ciphertext.data()), ciphertext.size())); ENVOY_LOG(error, "decrypted: {}",
  //  reinterpret_cast<const char*>(decrypted.data()));

  return Result::ok;
}

void EncryptionMutator::mutateRowDescription(RowDescriptionMessage& message) {
  data_row_config_.clear();
  for (auto& column : message.column_descriptions()) {
    auto column_ref = getColumnByAlias(column->name());
    if (column_ref == nullptr) {
      continue;
    }

    const ColumnConfig* config =
        config_->getColumnConfig(column_ref->table(), column_ref->column());
    data_row_config_.push_back(config_->getColumnConfig(column_ref->table(), column_ref->column()));

    if (config != nullptr && config->isEncrypted()) {
      ENVOY_LOG(error, "matched encrypted column: {} -> ({}, {})", column->name(),
                column_ref->table(), column_ref->column());

      // Ensure that encrypted data is returned as hex string
      ASSERT(column->formatCode() == 0);
    }
  }
}

void EncryptionMutator::mutateDataRow(DataRowMessage& message) {
  ASSERT(message.columns().size() == data_row_config_.size());
  size_t columns_count = message.columns().size();

  auto& crypto_util_ext = Envoy::Common::Crypto::UtilityExtSingleton::get();
  for (size_t i = 0; i < columns_count; i++) {
    auto& column = message.columns()[i];
    const ColumnConfig* config = data_row_config_[i];

    if (config == nullptr || !config->isEncrypted() || column->is_null()) {
      continue;
    }

    // Decrypt data

    auto& encrypted_column = column->value();
    absl::string_view encrypted_hex_string(reinterpret_cast<const char*>(encrypted_column.data()),
                                           encrypted_column.size());
    encrypted_hex_string.remove_prefix(2); // 0x
    auto encrypted_raw_data = absl::HexStringToBytes(encrypted_hex_string);
    auto decrypted_data = crypto_util_ext.AESDecrypt(
        config->encryptionKey(),
        absl::string_view(encrypted_raw_data.data(), encrypted_raw_data.size()));

    column = std::make_unique<VarByteN>(std::move(decrypted_data));
  }
}

Result EncryptionMutator::visitExpression(hsql::Expr* expr) {
  switch (expr->type) {
  case hsql::kExprColumnRef: {
    if (in_select_body_) {
      // SELECT of possibly encrypted column is ok
      return Visitor::visitExpression(expr);
    }

    // Other usages should be prohibited

    if (expr->table == nullptr) {
      return Result::makeError(fmt::format("postgres_tde: unable to determine the source of column "
                                           "{}. Please specify an explicit table/alias reference",
                                           expr->name));
    }

    const std::string& table_name = getTableNameByAlias(expr->table);
    ColumnConfig* column_config = config_->getColumnConfig(table_name, expr->name);
    if (column_config != nullptr && column_config->isEncrypted()) {
      return Result::makeError(fmt::format("postgres_tde: invalid use of encrypted column {}.{}",
                                           table_name, expr->name));
    }

    return Visitor::visitExpression(expr);
  }
  default:
    return Visitor::visitExpression(expr);
  }
}

Result EncryptionMutator::visitInsertStatement(hsql::InsertStatement* stmt) {
  switch (stmt->type) {
  case hsql::kInsertValues: {
    if (stmt->columns == nullptr && config_->hasTDEEnabled(stmt->tableName)) {
      return Result::makeError(
          "postgres_tde: columns definition for INSERT is required for TDE-enabled tables");
    }

    insert_mutation_candidates_.push_back(stmt);
    return Visitor::visitInsertStatement(stmt);
  }
  case hsql::kInsertSelect:
    if (config_->hasTDEEnabled(stmt->tableName)) {
      return Result::makeError(
          "postgres_tde: INSERT INTO SELECT is not supported for TDE-enabled tables");
    }

    return Visitor::visitInsertStatement(stmt);
  default:
    assert(false);
  }
}

Result EncryptionMutator::visitUpdateStatement(hsql::UpdateStatement* stmt) {
  CHECK_RESULT(Visitor::visitUpdateStatement(stmt));
  update_mutation_candidates_.push_back(stmt);
  return Result::ok;
}

Result EncryptionMutator::mutateInsertStatement() {
  absl::Cleanup cleanup = [this]() { insert_mutation_candidates_.clear(); };

  for (hsql::InsertStatement* stmt : insert_mutation_candidates_) {
    if (stmt->columns->size() != stmt->values->size()) {
      return Result::makeError("postgres_tde: bad INSERT statement");
    }

    for (size_t i = 0; i < stmt->columns->size(); i++) {
      char* column = (*stmt->columns)[i];
      hsql::Expr* value = (*stmt->values)[i];

      ColumnConfig* column_config = config_->getColumnConfig(stmt->tableName, column);
      if (column_config == nullptr || !column_config->isEncrypted()) {
        continue;
      }

      if (!value->isLiteral()) {
        return Result::makeError(
            "postgres_tde: only literals can be used as INSERT values for encrypted columns");
      }

      (*stmt->values)[i] = createEncryptedLiteral(value, column_config);
      delete value;
    }
  }

  return Result::ok;
}

Result EncryptionMutator::mutateUpdateStatement() {
  absl::Cleanup cleanup = [this]() { update_mutation_candidates_.clear(); };

  for (hsql::UpdateStatement* stmt : update_mutation_candidates_) {
    for (hsql::UpdateClause* update : *stmt->updates) {
      ColumnConfig* column_config = config_->getColumnConfig(stmt->table->name, update->column);
      if (column_config == nullptr || !column_config->isEncrypted()) {
        continue;
      }

      if (!update->value->isLiteral()) {
        return Result::makeError(fmt::format(
            "postgres_tde: only literals can be used as UPDATE values for blind-indexed columns"));
      }

      hsql::Expr* orig_literal = update->value;
      update->value = createEncryptedLiteral(orig_literal, column_config);
      delete orig_literal;
    }
  }

  return Result::ok;
}

hsql::Expr* EncryptionMutator::createEncryptedLiteral(hsql::Expr* orig_literal,
                                                      ColumnConfig* column_config) {
  assert(orig_literal->isLiteral());

  switch (orig_literal->type) {
  case hsql::kExprLiteralNull:
    // do nothing with null values
    return hsql::Expr::makeNullLiteral();
  case hsql::kExprLiteralString: {
    const std::string& encrypted_hex_str =
        generateCryptoString(absl::string_view(static_cast<const char*>(orig_literal->name),
                                               strlen(orig_literal->name) + 1),
                             column_config);
    return hsql::Expr::makeLiteral(Common::Utils::makeOwnedCString(encrypted_hex_str));
  }
  case hsql::kExprLiteralInt: {
    const std::string& encrypted_hex_str =
        generateCryptoString(absl::string_view(reinterpret_cast<const char*>(&orig_literal->ival),
                                               sizeof(orig_literal->ival)),
                             column_config);
    return hsql::Expr::makeLiteral(Common::Utils::makeOwnedCString(encrypted_hex_str));
  }
  case hsql::kExprLiteralFloat: {
    const std::string& encrypted_hex_str =
        generateCryptoString(absl::string_view(reinterpret_cast<const char*>(&orig_literal->fval),
                                               sizeof(orig_literal->fval)),
                             column_config);
    return hsql::Expr::makeLiteral(Common::Utils::makeOwnedCString(encrypted_hex_str));
  }
  default:
    assert(false);
  }
}

std::string EncryptionMutator::generateCryptoString(absl::string_view data,
                                                    ColumnConfig* column_config) {
  auto& crypto_util_ext = Envoy::Common::Crypto::UtilityExtSingleton::get();
  auto encrypted_data = crypto_util_ext.AESEncrypt(column_config->encryptionKey(), data);

  std::string encrypted_data_hex_str = std::string("\\x");
  encrypted_data_hex_str.append(absl::BytesToHexString(absl::string_view(
      reinterpret_cast<const char*>(encrypted_data.data()), encrypted_data.size())));
  return encrypted_data_hex_str;
}

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
