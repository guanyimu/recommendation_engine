#include "gated_config_reloader.h"

#include "config_manager.h"
#include "service_gate.h"

#include <iostream>

namespace recommendation {

struct GatedConfig::RequestGuard::Impl {
  ServiceGate::Guard guard;

  explicit Impl(GatedConfig& config) : guard(*config.gate_) {}
};

GatedConfig::RequestGuard::RequestGuard(GatedConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

GatedConfig::RequestGuard::~RequestGuard() = default;

bool GatedConfig::RequestGuard::ok() const {
  return impl_->guard.ok();
}

bool GatedConfig::Init(const std::string& path) {
  return ConfigManager::Instance().Init(path);
}

GatedConfig::GatedConfig()
    : config_(&ConfigManager::Instance()),
      gate_(std::make_unique<ServiceGate>()) {}

GatedConfig::GatedConfig(ConfigManager& config)
    : config_(&config), gate_(std::make_unique<ServiceGate>()) {}

GatedConfig::~GatedConfig() = default;

bool GatedConfig::Has(const std::string& key) const {
  return config_->Has(key);
}

bool GatedConfig::RequireString(const std::string& key,
                                        std::string* out) const {
  return config_->RequireString(key, out);
}

bool GatedConfig::RequireSizeT(const std::string& key,
                                       std::size_t* out) const {
  return config_->RequireSizeT(key, out);
}

const std::string& GatedConfig::config_path() const {
  return config_->path();
}

bool GatedConfig::Reload(Validator validate) {
  std::cout << "[config] Reload 开始，排空在途请求...\n";
  gate_->StopAccepting();
  gate_->WaitDrained();

  if (!config_->Reload()) {
    std::cerr << "[config] Reload 失败: " << config_->path() << '\n';
    gate_->StartAccepting();
    return false;
  }

  if (validate) {
    std::string err;
    if (!validate(*this, &err)) {
      std::cerr << "[config] Reload 后配置无效: " << err << '\n';
      gate_->StartAccepting();
      return false;
    }
  }

  gate_->StartAccepting();
  std::cout << "[config] Reload 完成: " << config_->path() << '\n';
  return true;
}

}  // namespace recommendation
