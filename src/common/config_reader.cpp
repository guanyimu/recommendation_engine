#include "config_reader.h"

namespace recommendation {
namespace {

bool ParseSizeT(const std::string& value, std::size_t* out) {
  if (value.empty()) {
    return false;
  }
  std::size_t n = 0;
  for (char c : value) {
    if (c < '0' || c > '9') {
      return false;
    }
    n = n * 10 + static_cast<std::size_t>(c - '0');
  }
  *out = n;
  return true;
}

}  // namespace

bool ConfigReader::RequireString(const std::string& key,
                                 std::string* out) const {
  if (!Has(key)) {
    return false;
  }
  *out = Get(key);
  return !out->empty();
}

bool ConfigReader::RequireSizeT(const std::string& key, std::size_t* out) const {
  if (!Has(key)) {
    return false;
  }
  return ParseSizeT(Get(key), out);
}

}  // namespace recommendation
