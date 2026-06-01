#pragma once

#include <cstddef>
#include <string>

namespace recommendation {

// 配置读取抽象接口；生产用 ConfigManager，测试可换 MockConfig
class ConfigReader {
 public:
  virtual ~ConfigReader() = default;

  virtual std::string Get(const std::string& key,
                          const std::string& default_value = "") const = 0;

  std::size_t GetSizeT(const std::string& key, std::size_t default_value) const;
};

}  // namespace recommendation
