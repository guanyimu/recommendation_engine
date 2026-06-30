#pragma once

#include "registry_store.h"

#include <atomic>
#include <string>

namespace recommendation {
namespace registry {

class RegistryHttpServer {
 public:
  explicit RegistryHttpServer(RegistryStore *store);

  // bind + listen 成功后进入 accept 循环；失败返回 false。
  bool Run(const std::string &listen_address,
           std::atomic<bool> *ready = nullptr);

 private:
  std::string HandleRequest(const std::string &request) const;

  RegistryStore *store_;
};

}  // namespace registry
}  // namespace recommendation
