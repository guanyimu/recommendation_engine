#pragma once

#include "service_gate.h"

#include <memory>
#include <string>

namespace recommendation {

class ConfigManager;

class GatedConfig {
public:
  // RPC 入口由 Interceptor 构造；整个请求生命周期内 active+1，供 Reload drain。
  class RequestGuard {
  public:
    RequestGuard();
    ~RequestGuard() = default;

    RequestGuard(const RequestGuard &) = delete;
    RequestGuard &operator=(const RequestGuard &) = delete;

    bool ok() const { return guard_.ok(); }

  private:
    ServiceGate::Guard guard_;
  };

  static GatedConfig &Instance();

  bool Init(const std::string &path);

  bool HasSnapshot() const;

  bool RequireString(const std::string &key, std::string &out) const;
  bool RequireInt(const std::string &key, int &out) const;
  const std::string &config_path() const;

  bool Reload(const std::string &new_path);

private:
  friend class RequestGuard;

  int wait_drained_timeout_ms_{5000};

  GatedConfig();
  ~GatedConfig();
  GatedConfig(const GatedConfig &) = delete;
  GatedConfig &operator=(const GatedConfig &) = delete;

  std::unique_ptr<ConfigManager> config_manager_;
  std::unique_ptr<ServiceGate> gate_;
};

} // namespace recommendation
