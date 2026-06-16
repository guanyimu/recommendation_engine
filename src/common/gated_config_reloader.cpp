#include "gated_config_reloader.h"

#include "config_manager.h"
#include "logger.h"
#include "service_gate.h"

namespace recommendation {
namespace {

void LogNoSnapshot(const char *op, const std::string &key) {
  LOG(ERROR) << "[config] 未加载配置快照（请先 GatedConfig::Init），" << op
             << "(\"" << key << "\") 失败";
}

void LogNoSnapshotOp(const char *op) {
  LOG(ERROR) << "[config] 未加载配置快照（请先 GatedConfig::Init），" << op
             << " 失败";
}

void LogGateDenied(const char *op, const std::string *key) {
  if (key) {
    LOG(WARNING) << "[config] " << op << "(\"" << *key
                 << "\") 拒绝：服务暂不可用（Init/Reload 中？）";
  } else {
    LOG(WARNING) << "[config] " << op
                 << " 拒绝：服务暂不可用（Init/Reload 中？）";
  }
}

} // namespace

GatedConfig::RequestGuard::RequestGuard()
    : guard_(*GatedConfig::Instance().gate_) {
  // LOG(INFO) << "[config] RequestGuard 构造\t"
  //           << std::to_string(
  //                  GatedConfig::Instance().gate_->active_requests_.load());
}

GatedConfig &GatedConfig::Instance() {
  static GatedConfig instance;
  return instance;
}

bool GatedConfig::Init(const std::string &path) {
  gate_->StopAccepting();
  gate_->WaitDrained(wait_drained_timeout_ms_);

  auto manager = std::make_unique<ConfigManager>(path);
  if (!manager->HasSnapshot()) {
    LOG(ERROR) << "[config] Init 失败，无有效快照: " << path;
    gate_->StartAccepting();
    return false;
  }

  config_manager_ = std::move(manager);
  gate_->StartAccepting();
  return true;
}

GatedConfig::GatedConfig() : gate_(std::make_unique<ServiceGate>()) {}

GatedConfig::~GatedConfig() = default;

bool GatedConfig::HasSnapshot() const {
  ServiceGate::Guard guard(*gate_);
  if (!guard.ok()) {
    LogGateDenied("HasSnapshot", nullptr);
    return false;
  }
  return config_manager_ != nullptr && config_manager_->HasSnapshot();
}

bool GatedConfig::RequireString(const std::string &key,
                                std::string &out) const {
  ServiceGate::Guard guard(*gate_);
  if (!guard.ok()) {
    LogGateDenied("RequireString", &key);
    return false;
  }
  if (!config_manager_ || !config_manager_->HasSnapshot()) {
    LogNoSnapshot("RequireString", key);
    return false;
  }
  return config_manager_->RequireString(key, out);
}

bool GatedConfig::RequireInt(const std::string &key, int &out) const {
  ServiceGate::Guard guard(*gate_);
  if (!guard.ok()) {
    LogGateDenied("RequireInt", &key);
    return false;
  }
  if (!config_manager_ || !config_manager_->HasSnapshot()) {
    LogNoSnapshot("RequireInt", key);
    return false;
  }
  return config_manager_->RequireInt(key, out);
}

const std::string &GatedConfig::config_path() const {
  ServiceGate::Guard guard(*gate_);
  if (!guard.ok()) {
    LogGateDenied("config_path", nullptr);
    static const std::string kEmpty;
    return kEmpty;
  }
  if (!config_manager_) {
    LogNoSnapshotOp("config_path");
    static const std::string kEmpty;
    return kEmpty;
  }
  return config_manager_->path();
}

bool GatedConfig::Reload(const std::string &new_path) {
  if (!config_manager_ || !config_manager_->HasSnapshot()) {
    LogNoSnapshotOp("Reload");
    return false;
  }

  LOG(INFO) << "[config] Reload 开始，排空在途请求...";
  gate_->StopAccepting();

  if (!config_manager_->ReloadFromFile(new_path)) {
    LOG(ERROR) << "[config] Reload 失败，无法从 " << new_path << " 加载配置";
    gate_->StartAccepting();
    return false;
  }
  LOG(INFO) << "[config] Reload 从 " << new_path << " 加载配置成功";

  gate_->WaitDrained(wait_drained_timeout_ms_);

  if (!config_manager_->ReloadFromRAM()) {
    LOG(ERROR) << "[config] Reload 失败，无法切换config快照";
    gate_->StartAccepting();
    return false;
  }

  gate_->StartAccepting();

  int wait_drained_timeout_ms_new = 0;
  if (RequireInt("wait_drained_timeout_ms", wait_drained_timeout_ms_new)) {
    wait_drained_timeout_ms_ = wait_drained_timeout_ms_new;
    LOG(INFO) << "[config] Reload 完成: " << new_path;
  } else {
    LOG(ERROR) << "[config] Reload 完成: " << new_path;
  }

  LOG(INFO) << "[config] Reload 完成: " << new_path;
  return true;
}

} // namespace recommendation
