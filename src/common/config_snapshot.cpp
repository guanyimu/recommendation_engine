#include "config_snapshot.h"

#include <fstream>
#include <utility>

namespace recommendation {

ConfigSnapshot::ConfigSnapshot(Map kv) : kv_(std::move(kv)) {}

std::unique_ptr<ConfigSnapshot> ConfigSnapshot::LoadFromFile(
    const std::string& path) {
  std::ifstream in(path);
  if (!in) {
    return nullptr;
  }

  Map kv;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }

    const std::size_t pos = line.find('=');
    if (pos == std::string::npos) {
      continue;
    }

    kv[line.substr(0, pos)] = line.substr(pos + 1);
  }

  return std::unique_ptr<ConfigSnapshot>(new ConfigSnapshot(std::move(kv)));
}

std::string ConfigSnapshot::Get(const std::string& key) const {
  const auto it = kv_.find(key);
  return it != kv_.end() ? it->second : "";
}

bool ConfigSnapshot::Has(const std::string& key) const {
  return kv_.find(key) != kv_.end();
}

}  // namespace recommendation
