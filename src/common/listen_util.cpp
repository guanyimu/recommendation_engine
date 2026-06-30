#include "listen_util.h"

#include <cstdlib>
#include <string>

namespace recommendation {

void ApplyInstanceLogging(LogOptions &opts, const char *default_server_name,
                          const char *default_log_dir) {
  static std::string name_storage;
  static std::string dir_storage;

  const char *instance = std::getenv("RE_INSTANCE_NAME");
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

} // namespace recommendation
