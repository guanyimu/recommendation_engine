#include "config_snapshot.h"

#include "logger.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <utility>

namespace recommendation {
namespace {

std::string Trim(std::string s) {
  const auto not_space = [](unsigned char c) { return !std::isspace(c); };
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
  s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
  return s;
}

} // namespace

ConfigSnapshot::ConfigSnapshot(Map kv) : kv_(std::move(kv)) {}

std::unique_ptr<ConfigSnapshot>
ConfigSnapshot::LoadFromFile(const std::string &path) {
  std::ifstream in(path);
  if (!in) {
    LOG(ERROR) << "[config] 无法打开配置文件: " << path;
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
      LOG(WARNING) << "[config] 忽略非法行（缺少 '='）: " << line;
      continue;
    }

    const std::string key = Trim(line.substr(0, pos));
    const std::string value = Trim(line.substr(pos + 1));
    if (key.empty()) {
      LOG(WARNING) << "[config] 忽略非法行（key 为空）: " << line;
      continue;
    }
    kv[key] = value;
  }

  return std::unique_ptr<ConfigSnapshot>(new ConfigSnapshot(std::move(kv)));
}

bool ConfigSnapshot::Get(const std::string &key, std::string &value) const {
  const auto it = kv_.find(key);
  if (it == kv_.end()) {
    return false;
  }
  value = it->second;
  return true;
}

} // namespace recommendation
