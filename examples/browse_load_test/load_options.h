#pragma once

#include <string>

namespace recommendation {
namespace load_test {

struct LoadOptions {
  std::string config_path{"configs/services.conf"};
  int workers{1000};
  int duration_sec{30};
  int min_sleep_sec{1};
  int max_sleep_sec{3};
  int stagger_batch{100};
  int stagger_ms{100};
  bool quiet{true};
};

// 返回 false 表示应打印帮助并退出 0。
bool ParseArgs(int argc, char **argv, LoadOptions *out);

void PrintUsage(const char *prog);

}  // namespace load_test
}  // namespace recommendation
