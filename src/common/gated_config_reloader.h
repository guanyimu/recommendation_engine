#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>

namespace recommendation {

class ConfigManager;
class ServiceGate;

// 配置读 + Gate 排空 Reload 的统一入口；业务只依赖此类
class GatedConfig {
 public:
  using Validator =
      std::function<bool(GatedConfig& config, std::string* err)>;

  class RequestGuard {
   public:
    explicit RequestGuard(GatedConfig& config);
    ~RequestGuard();

    RequestGuard(const RequestGuard&) = delete;
    RequestGuard& operator=(const RequestGuard&) = delete;

    bool ok() const;

   private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
  };

  static bool Init(const std::string& path);

  GatedConfig();
  explicit GatedConfig(ConfigManager& config);
  ~GatedConfig();

  GatedConfig(const GatedConfig&) = delete;
  GatedConfig& operator=(const GatedConfig&) = delete;

  bool Has(const std::string& key) const;
  bool RequireString(const std::string& key, std::string* out) const;
  bool RequireSizeT(const std::string& key, std::size_t* out) const;
  const std::string& config_path() const;

  bool Reload(Validator validate = nullptr);

 private:
  ConfigManager* config_;
  std::unique_ptr<ServiceGate> gate_;
};

}  // namespace recommendation
