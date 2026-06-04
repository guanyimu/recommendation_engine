#include "config_manager.h"
#include "config_snapshot.h"

#include <mutex>

namespace recommendation {

ConfigManager& ConfigManager::Instance() {
  static ConfigManager instance;
  return instance;
}

bool ConfigManager::Init(const std::string& path) {
  path_ = path;
  return Reload();
}

bool ConfigManager::Reload() {
  std::unique_ptr<ConfigSnapshot> new_snapshot =
      ConfigSnapshot::LoadFromFile(path_);
  if (!new_snapshot) {
    return false;
  }

  std::unique_lock<std::shared_mutex> lock(mutex_);
  snapshot_ = std::shared_ptr<const ConfigSnapshot>(new_snapshot.release());
  return true;
}

std::string ConfigManager::Get(const std::string& key) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  if (!snapshot_) {
    return "";
  }
  return snapshot_->Get(key);
}

bool ConfigManager::Has(const std::string& key) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  if (!snapshot_) {
    return false;
  }
  return snapshot_->Has(key);
}

}  // namespace recommendation
