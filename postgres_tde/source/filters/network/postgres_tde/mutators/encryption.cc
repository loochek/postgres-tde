#include "postgres_tde/source/filters/network/postgres_tde/mutators/encryption.h"
#include "postgres_tde/source/filters/network/postgres_tde/postgres_mutation_manager.h"
#include "postgres_tde/source/common/crypto/utility_ext.h"
#include "source/common/common/fmt.h"
#include "absl/strings/escaping.h"
#include "absl/cleanup/cleanup.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

EncryptionMutator::EncryptionMutator(MutationManager* manager) : BaseMutator(manager) {}

Result EncryptionMutator::mutateQuery(hsql::SQLParserResult& query) {
  insert_mutation_candidates_.clear();
  update_mutation_candidates_.clear();

  CHECK_RESULT(Visitor::visitQuery(query));
  CHECK_RESULT(mutateInsertStatement());
  CHECK_RESULT(mutateUpdateStatement());

  return Result::ok;
}

Result EncryptionMutator::mutateRowDescription(RowDescriptionMessage& message) {
  data_row_config_.clear();

  std::unordered_set<std::string> column_names;
  for (auto& column : message.column_descriptions()) {
    if (column_names.find(column->name()) != column_names.end()) {
      return Result::makeError(fmt::format("postgres_tde: detected ambiguous column name {}. "
                                           "Please specify a different alias for each column",
                                           column->name()));
    }

    column_names.insert(column->name());

    auto column_ref = getSelectColumnByAlias(column->name());
    if (column_ref == nullptr) {
      data_row_config_.push_back(nullptr);
      continue;
    }

    auto column_config =
        mgr_->getEncryptionConfig()->getColumnConfig(column_ref->table(), column_ref->column());
    data_row_config_.push_back(
        mgr_->getEncryptionConfig()->getColumnConfig(column_ref->table(), column_ref->column()));

    if (column_config != nullptr && column_config->isEncrypted()) {
      ENVOY_LOG(error, "matched encrypted column: {} -> ({}, {})", column->name(),
                column_ref->table(), column_ref->column());

      // Ensure that encrypted data is returned as hex string
      ASSERT(column->formatCode() == 0);

      // Replace data type OID and size
      column->dataType() = column_config->origDataType();
      column->dataSize() = column_config->origDataSize();
    }
  }

  return Result::ok;
}

Result EncryptionMutator::mutateDataRow(std::unique_ptr<DataRowMessage>& message) {
  ASSERT(message->columns().size() == data_row_config_.size());
  size_t columns_count = message->columns().size();

  auto& crypto_util_ext = Common::Crypto::UtilityExtSingleton::get();
  for (size_t i = 0; i < columns_count; i++) {
    auto& column = message->columns()[i];
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
    std::vector<uint8_t> decrypted_data;
    Result result = crypto_util_ext.AESDecrypt(
        config->encryptionKey(),
        absl::string_view(encrypted_raw_data.data(), encrypted_raw_data.size()), decrypted_data);
    if (!result.isOk) {
      return result;
    }

    column = std::make_unique<VarByteN>(std::move(decrypted_data));
  }

  return Result::ok;
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
    auto column_config = mgr_->getEncryptionConfig()->getColumnConfig(table_name, expr->name);
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

Result EncryptionMutator::mutateInsertStatement() {
  for (hsql::InsertStatement* stmt : insert_mutation_candidates_) {
    if (stmt->columns->size() != stmt->values->size()) {
      return Result::makeError("postgres_tde: bad INSERT statement");
    }

    for (size_t i = 0; i < stmt->columns->size(); i++) {
      char* column = (*stmt->columns)[i];
      hsql::Expr* value = (*stmt->values)[i];

      auto column_config = mgr_->getEncryptionConfig()->getColumnConfig(stmt->tableName, column);
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
  for (hsql::UpdateStatement* stmt : update_mutation_candidates_) {
    for (hsql::UpdateClause* update : *stmt->updates) {
      auto column_config =
          mgr_->getEncryptionConfig()->getColumnConfig(stmt->table->name, update->column);
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
                                                      const ColumnConfig* column_config) {
  ASSERT(orig_literal->isLiteral());

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
    ASSERT(false);
  }
}

std::string EncryptionMutator::generateCryptoString(absl::string_view data,
                                                    const ColumnConfig* column_config) {
  auto& crypto_util_ext = Common::Crypto::UtilityExtSingleton::get();

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
