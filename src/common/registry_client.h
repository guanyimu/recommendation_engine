#pragma once

#include "config.h"

#include <string>
#include <vector>

namespace recommendation {

struct RegistryEndpoint {
  std::string service;
  std::string instance_id;
  std::string kind;
  std::string host;
  int port{0};
  int pid{0};

  std::string Address() const;
};

class RegistryClient {
 public:
  explicit RegistryClient(std::string registry_address);

  static bool FromConfig(const std::shared_ptr<const ConfigSnapshot> &snap,
                         RegistryClient *out);

  bool Register(const RegistryEndpoint &endpoint);
  bool Deregister(const std::string &service, const std::string &instance_id);
  bool Resolve(const std::string &service, std::string *address_out);
  bool ListInstances(const std::string &service,
                     std::vector<RegistryEndpoint> *out);
  bool ListAll(std::vector<RegistryEndpoint> *out);

  const std::string &registry_address() const { return registry_address_; }

 private:
  std::string registry_address_;
};

}  // namespace recommendation
