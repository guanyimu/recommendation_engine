#include "config_manager.h"
#include "config_snapshot.h"

#include "logger.h"

#include <stdexcept>
#include <string>

namespace recommendation {

ConfigManager::ConfigManager(const std::string &path) : path_(path) {
  ReloadFromFile(path_);
  ReloadFromRAM();
}

bool ConfigManager::RequireString(const std::string &key,
                                  std::string &out) const {
  if (!snapshot_->Get(key, out)) {
    LOG(ERROR) << "[config] 配置项 " << key << " 不存在";
    return false;
  }
  return !out.empty();
}

bool ConfigManager::RequireInt(const std::string &key, int &out) const {
  std::string value;
  if (!snapshot_->Get(key, value)) {
    LOG(ERROR) << "[config] 配置项 " << key << " 不存在";
    return false;
  }
  try {
    std::size_t pos = 0;
    const int n = std::stoi(value, &pos);
    if (pos != value.size()) {
      LOG(ERROR) << "[config] 配置项 " << key << " 的值非整数: " << value;
      return false;
    }
    out = n;
    return true;
  } catch (const std::exception &) {
    LOG(ERROR) << "[config] 配置项 " << key << " 的值非整数: " << value;
    return false;
  }
}

bool ConfigManager::ReloadFromFile(const std::string &new_path) {
  new_path_ = new_path;
  new_snapshot_ = std::move(ConfigSnapshot::LoadFromFile(new_path_));
  if (!new_snapshot_) {
    LOG(ERROR) << "[config] Reload 失败，无法加载: " << new_path_;
    return false;
  }
  return true;
}

bool ConfigManager::ReloadFromRAM() {
  if (!new_snapshot_) {
    LOG(ERROR) << "[config] ReloadFromFile 前就调用 ReloadFromRAM: ";
    return false;
  }
  snapshot_ = std::move(new_snapshot_);
  path_ = new_path_;
  new_snapshot_ = nullptr;
  new_path_ = "";
  return true;
}

const std::string &ConfigManager::path() const { return path_; }

bool ConfigManager::HasSnapshot() const { return snapshot_ != nullptr; }

} // namespace recommendation
