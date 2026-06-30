#pragma once

#include "config.h"

#include <memory>
#include <string>

namespace recommendation {

// 服务端：bind 成功后注册，Shutdown 时注销。
class ServerRegistration {
 public:
  explicit ServerRegistration(std::string service_name);

  bool RegisterBoundPort(int port);
  void Deregister();

  const std::string &instance_id() const { return instance_id_; }
  const std::string &bound_address() const { return bound_address_; }

 private:
  std::string service_name_;
  std::string instance_id_;
  std::string bound_address_;
  bool registered_{false};
};

std::string DefaultInstanceId(const std::string &service_name);

// 客户端 / 下游：从名字服务解析 LB 入口。
bool ResolveServiceAddress(const std::shared_ptr<const ConfigSnapshot> &snap,
                           const char *service_name, std::string *address_out);

}  // namespace recommendation
