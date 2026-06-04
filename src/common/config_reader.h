#pragma once

#include <cstddef>
#include <string>

namespace recommendation {

// 配置读取抽象接口；生产用 ConfigManager，测试可换 MockConfig
class ConfigReader {
 public:
  virtual ~ConfigReader() = default;

  virtual bool Has(const std::string& key) const = 0;
  virtual std::string Get(const std::string& key) const = 0;

  // 缺少 key、值为空或非法时返回 false
  bool RequireString(const std::string& key, std::string* out) const;
  bool RequireSizeT(const std::string& key, std::size_t* out) const;
};

}  // namespace recommendation
