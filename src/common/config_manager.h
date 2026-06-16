#pragma once

#include "config_snapshot.h"

#include <memory>
#include <shared_mutex>
#include <string>

namespace recommendation {

// 配置存储：构造时从 path 加载，Reload 时 swap 快照（由 GatedConfig 独占持有）
class ConfigManager {
public:
  explicit ConfigManager(const std::string &path);

  bool RequireString(const std::string &key, std::string &out) const;
  bool RequireInt(const std::string &key, int &out) const;
  bool HasSnapshot() const;

  bool ReloadFromFile(const std::string &new_path);
  bool ReloadFromRAM();

  const std::string &path() const;

private:
  std::string path_;
  std::unique_ptr<const ConfigSnapshot> snapshot_{nullptr};
  std::string new_path_;
  std::unique_ptr<const ConfigSnapshot> new_snapshot_{nullptr};
};

} // namespace recommendation
