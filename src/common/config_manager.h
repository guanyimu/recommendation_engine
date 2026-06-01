#pragma once

#include "config_reader.h"
#include "config_snapshot.h"

#include <memory>
#include <shared_mutex>
#include <string>

namespace recommendation {

// 进程内单例；从文件加载 KV，Reload 时 swap 新快照
class ConfigManager : public ConfigReader {
 public:
  static ConfigManager& Instance();

  bool Init(const std::string& path);
  bool Reload();

  std::string Get(const std::string& key,
                  const std::string& default_value = "") const override;

  const std::string& path() const { return path_; }

 private:
  ConfigManager() = default;

  std::string path_;
  std::shared_ptr<const ConfigSnapshot> snapshot_;
  mutable std::shared_mutex mutex_;
};

}  // namespace recommendation
