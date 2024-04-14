#include "postgres_tde/source/filters/network/postgres_tde/postgres_message.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

// String type methods.
bool String::read(const Buffer::Instance& data, uint64_t& pos, uint64_t& left) {
  // read method uses values set by validate method.
  // This avoids unnecessary repetition of scanning data looking for terminating zero.
  ASSERT(pos == start_);
  ASSERT(end_ >= start_);

  // Reserve that many bytes in the string.
  const uint64_t size = end_ - start_;
  value_.resize(size);
  // Now copy from buffer to string.
  data.copyOut(pos, size, value_.data());
  pos += (size + 1);
  left -= (size + 1);

  return true;
}

std::string String::toString() const { return absl::StrCat("[", value_, "]"); }

Message::ValidationResult String::validate(const Buffer::Instance& data,
                                           const uint64_t start_offset, uint64_t& pos,
                                           uint64_t& left) {
  // Try to find the terminating zero.
  // If found, all is good. If not found, we may need more data.
  const char zero = 0;
  const ssize_t index = data.search(&zero, 1, pos);
  if (index == -1) {
    if (left <= (data.length() - pos)) {
      // Message ended before finding terminating zero.
      return Message::ValidationFailed;
    } else {
      return Message::ValidationNeedMoreData;
    }
  }
  // Found, but after the message boundary.
  const uint64_t size = index - pos;
  if (size >= left) {
    return Message::ValidationFailed;
  }

  start_ = pos - start_offset;
  end_ = start_ + size;

  pos += (size + 1);
  left -= (size + 1);
  return Message::ValidationOK;
}

// ByteN type methods.
bool ByteN::read(const Buffer::Instance& data, uint64_t& pos, uint64_t& left) {
  if (left > (data.length() - pos)) {
    return false;
  }
  value_.resize(left);
  data.copyOut(pos, left, value_.data());
  pos += left;
  left = 0;
  return true;
}
// Since ByteN does not have a length field, it is not possible to verify
// its correctness.
Message::ValidationResult ByteN::validate(const Buffer::Instance& data, const uint64_t,
                                          uint64_t& pos, uint64_t& left) {
  if (left > (data.length() - pos)) {
    return Message::ValidationNeedMoreData;
  }

  pos += left;
  left = 0;

  return Message::ValidationOK;
}

std::string ByteN::toString() const {
  std::string out = "[";
  absl::StrAppend(&out, absl::StrJoin(value_, " "));
  absl::StrAppend(&out, "]");
  return out;
}

// VarByteN type methods.
bool VarByteN::read(const Buffer::Instance& data, uint64_t& pos, uint64_t& left) {
  // len_ was set by validator, skip it.
  int32_t len = data.peekBEInt<int32_t>(pos);

  pos += sizeof(int32_t);
  left -= sizeof(int32_t);
  if (len < 1) {
    // There is no payload if length is not positive.
    value_ = std::nullopt;
    return true;
  }

  value_ = std::vector<uint8_t>();
  value_->resize(len);
  data.copyOut(pos, len, value_->data());
  pos += len;
  left -= len;
  return true;
}

std::string VarByteN::toString() const {
  std::string out;
  if (value_.has_value()) {
    out = fmt::format("[({} bytes):", value_->size());
    absl::StrAppend(&out, absl::StrJoin(*value_, " "));
    absl::StrAppend(&out, "]");
  } else {
    out = "[null]";
  }
  return out;
}

Message::ValidationResult VarByteN::validate(const Buffer::Instance& data, const uint64_t,
                                             uint64_t& pos, uint64_t& left) {
  if (left < sizeof(int32_t)) {
    // Malformed message.
    return Message::ValidationFailed;
  }

  if ((data.length() - pos) < sizeof(int32_t)) {
    return Message::ValidationNeedMoreData;
  }

  // Read length of the VarByteN structure.
  int32_t len = data.peekBEInt<int32_t>(pos);
  if (static_cast<int64_t>(len) > static_cast<int64_t>(left)) {
    // VarByteN would extend past the current message boundaries.
    // Lengths of message and individual fields do not match.
    return Message::ValidationFailed;
  }

  if (len < 1) {
    // There is no payload if length is not positive.
    pos += sizeof(int32_t);
    left -= sizeof(int32_t);
    return Message::ValidationOK;
  }

  if ((data.length() - pos) < (len + sizeof(int32_t))) {
    return Message::ValidationNeedMoreData;
  }

  pos += (len + sizeof(int32_t));
  left -= (len + sizeof(int32_t));

  return Message::ValidationOK;
}

bool VarByteN::operator==(const VarByteN& other) const {
  if (this == &other) {
    return true;
  }

  return value_ == other.value_;
}

bool VarByteN::operator!=(const VarByteN& other) const {
  return !(*this == other);
}

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
