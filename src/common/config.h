#pragma once

#include <memory>
#include <string>
#include <unordered_map>

namespace recommendation {

// 不可变 KV 快照；对外仅 Require* 读配置。
class ConfigSnapshot {
public:
  bool RequireString(const std::string &key, std::string &value) const;
  bool RequirePositiveInt(const std::string &key, int &out) const;
  bool RequireNonNegativeInt(const std::string &key, int &out) const;

private:
  friend class Config;

  using Map = std::unordered_map<std::string, std::string>;

  explicit ConfigSnapshot(Map kv);

  static std::unique_ptr<ConfigSnapshot> LoadFromFile(const std::string &path);

  bool Get(const std::string &key, std::string &value) const;

  Map kv_;
};

// 进程内配置入口：Reload 原子替换快照，GetSnapshot 返回一份 shared_ptr 拷贝。
class Config {
public:
  static bool Reload(const std::string &path);
  static std::shared_ptr<const ConfigSnapshot> GetSnapshot();

private:
  Config() = delete;
};

} // namespace recommendation
