#include "server_registration.h"

#include "config.h"
#include "logger.h"
#include "net_util.h"
#include "registry_client.h"
#include "service_names.h"

#include <unistd.h>

namespace recommendation {

std::string DefaultInstanceId(const std::string &service_name) {
  const char *env = std::getenv("RE_INSTANCE_NAME");
  if (env != nullptr && *env != '\0') {
    return env;
  }
  return service_name + '-' + std::to_string(static_cast<int>(::getpid()));
}

ServerRegistration::ServerRegistration(std::string service_name)
    : service_name_(std::move(service_name)),
      instance_id_(DefaultInstanceId(service_name_)) {}

bool ServerRegistration::RegisterBoundPort(int port) {
  if (registered_) {
    return true;
  }

  const std::shared_ptr<const ConfigSnapshot> snap = Config::GetSnapshot();
  RegistryClient client("");
  if (!RegistryClient::FromConfig(snap, &client)) {
    LOG(ERROR) << "[registry] 未配置 " << kRegistryAddressKey;
    return false;
  }

  bound_address_ = FormatHostPort("127.0.0.1", port);
  RegistryEndpoint endpoint;
  endpoint.service = service_name_;
  endpoint.instance_id = instance_id_;
  endpoint.kind = kRegistryKindInstance;
  endpoint.host = "127.0.0.1";
  endpoint.port = port;
  endpoint.pid = static_cast<int>(::getpid());

  if (!client.Register(endpoint)) {
    LOG(ERROR) << "[registry] 注册失败 service=" << service_name_
               << " instance=" << instance_id_
               << " address=" << bound_address_;
    return false;
  }

  registered_ = true;
  LOG(INFO) << "[registry] 已注册 service=" << service_name_
            << " instance=" << instance_id_
            << " address=" << bound_address_;
  return true;
}

void ServerRegistration::Deregister() {
  if (!registered_) {
    return;
  }

  const std::shared_ptr<const ConfigSnapshot> snap = Config::GetSnapshot();
  RegistryClient client("");
  if (RegistryClient::FromConfig(snap, &client)) {
    if (client.Deregister(service_name_, instance_id_)) {
      LOG(INFO) << "[registry] 已注销 service=" << service_name_
                << " instance=" << instance_id_;
    } else {
      LOG(WARNING) << "[registry] 注销失败 service=" << service_name_
                   << " instance=" << instance_id_;
    }
  }
  registered_ = false;
}

bool ResolveServiceAddress(const std::shared_ptr<const ConfigSnapshot> &snap,
                           const char *service_name, std::string *address_out) {
  if (!address_out) {
    return false;
  }
  RegistryClient client("");
  if (!RegistryClient::FromConfig(snap, &client)) {
    LOG(ERROR) << "[registry] 客户端未配置 " << kRegistryAddressKey;
    return false;
  }
  if (!client.Resolve(service_name, address_out)) {
    LOG(ERROR) << "[registry] 解析失败 service=" << service_name;
    return false;
  }
  return true;
}

}  // namespace recommendation
