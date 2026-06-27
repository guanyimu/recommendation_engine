#include "listen_util.h"

#include <cstdlib>

namespace recommendation {
namespace {

constexpr char kListenEnv[] = "RE_LISTEN_ADDRESS";
constexpr char kInstanceEnv[] = "RE_INSTANCE_NAME";

}  // namespace

bool ListenAddressFromEnv() {
  const char *env = std::getenv(kListenEnv);
  return env != nullptr && *env != '\0';
}

std::string ResolveListenAddress(
    const std::shared_ptr<const ConfigSnapshot> &snap,
    const char *config_key) {
  if (ListenAddressFromEnv()) {
    return std::getenv(kListenEnv);
  }
  std::string address;
  if (!snap || !snap->RequireString(config_key, address)) {
    return {};
  }
  return address;
}

void ApplyInstanceLogging(LogOptions &opts, const char *default_server_name,
                          const char *default_log_dir) {
  static std::string name_storage;
  static std::string dir_storage;

  const char *instance = std::getenv(kInstanceEnv);
  if (instance != nullptr && *instance != '\0') {
    name_storage = instance;
    dir_storage = std::string("logs/") + instance;
    opts.server_name = name_storage.c_str();
    opts.log_dir = dir_storage.c_str();
    return;
  }
  opts.server_name = default_server_name;
  opts.log_dir = default_log_dir;
}

}  // namespace recommendation
