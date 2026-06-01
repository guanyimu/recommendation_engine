#include "config_reader.h"

namespace recommendation {

std::size_t ConfigReader::GetSizeT(const std::string& key,
                                    std::size_t default_value) const {
  const std::string value = Get(key);
  if (value.empty()) {
    return default_value;
  }

  std::size_t n = 0;
  for (char c : value) {
    if (c < '0' || c > '9') {
      return default_value;
    }
    n = n * 10 + static_cast<std::size_t>(c - '0');
  }
  return n;
}

}  // namespace recommendation
