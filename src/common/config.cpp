#include "config.h"

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

std::shared_ptr<const ConfigSnapshot> g_current;

bool ParseIntStrict(const std::string &text, int &out) {
  try {
    std::size_t pos = 0;
    const int n = std::stoi(text, &pos);
    if (pos != text.size()) {
      return false;
    }
    out = n;
    return true;
  } catch (const std::exception &) {
    return false;
  }
}

} // namespace

ConfigSnapshot::ConfigSnapshot(Map kv) : kv_(std::move(kv)) {}

bool ConfigSnapshot::Get(const std::string &key, std::string &value) const {
  const auto it = kv_.find(key);
  if (it == kv_.end()) {
    return false;
  }
  value = Trim(it->second);
  return true;
}

bool ConfigSnapshot::RequireString(const std::string &key,
                                   std::string &value) const {
  if (!Get(key, value)) {
    LOG(ERROR) << "[config] 配置项 " << key << " 不存在";
    return false;
  }
  return true;
}

bool ConfigSnapshot::RequirePositiveInt(const std::string &key,
                                        int &out) const {
  std::string text;
  if (!Get(key, text)) {
    LOG(ERROR) << "[config] 配置项 " << key << " 不存在";
    return false;
  }
  if (!ParseIntStrict(text, out) || out <= 0) {
    LOG(ERROR) << "[config] 配置项 " << key << " 的值非正整数: " << text;
    return false;
  }
  return true;
}

bool ConfigSnapshot::RequireNonNegativeInt(const std::string &key,
                                           int &out) const {
  std::string text;
  if (!Get(key, text)) {
    LOG(ERROR) << "[config] 配置项 " << key << " 不存在";
    return false;
  }
  if (!ParseIntStrict(text, out) || out < 0) {
    LOG(ERROR) << "[config] 配置项 " << key << " 的值非非负整数: " << text;
    return false;
  }
  return true;
}

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

bool Config::Reload(const std::string &path) {
  std::unique_ptr<ConfigSnapshot> loaded = ConfigSnapshot::LoadFromFile(path);
  if (!loaded) {
    LOG(ERROR) << "[config] Reload 失败，无法加载: " << path;
    return false;
  }

  std::shared_ptr<const ConfigSnapshot> next(std::move(loaded));
  std::atomic_store(&g_current, next);
  LOG(INFO) << "[config] Reload 完成: " << path;
  return true;
}

std::shared_ptr<const ConfigSnapshot> Config::GetSnapshot() {
  return std::atomic_load(&g_current);
}

} // namespace recommendation
