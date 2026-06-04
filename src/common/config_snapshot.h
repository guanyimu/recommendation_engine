#pragma once

#include <memory>
#include <string>
#include <unordered_map>

namespace recommendation {

// 不可变 KV 快照，Reload 时整表替换
class ConfigSnapshot {
 public:
  using Map = std::unordered_map<std::string, std::string>;

  static std::unique_ptr<ConfigSnapshot> LoadFromFile(const std::string& path);

  bool Has(const std::string& key) const;
  std::string Get(const std::string& key) const;

 private:
  explicit ConfigSnapshot(Map kv);

  Map kv_;
};

}  // namespace recommendation
