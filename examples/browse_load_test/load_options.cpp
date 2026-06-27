#include "load_options.h"

#include <cstring>
#include <iostream>

namespace recommendation {
namespace load_test {

void PrintUsage(const char *prog) {
  std::cout
      << "用法: " << prog << " [选项]\n"
      << "\n"
      << "多进程压测 Browse gRPC（每个 worker 独立进程 + 独立 TCP 连接）。\n"
      << "需先启动服务: ./tools/run_servers.sh\n"
      << "\n"
      << "选项:\n"
      << "  --workers N        worker 进程数（默认 1000）\n"
      << "  --duration SEC     每个 worker 运行秒数（默认 30；0 表示只刷 1 次）\n"
      << "  --min-sleep SEC    两次 Browse 之间最短间隔（默认 1）\n"
      << "  --max-sleep SEC    两次 Browse 之间最长间隔（默认 3）\n"
      << "  --stagger-batch N  每 fork N 个 worker 暂停一次（默认 100）\n"
      << "  --stagger-ms MS    分批暂停毫秒数（默认 100）\n"
      << "  --config PATH      配置文件（默认 configs/services.conf）\n"
      << "  --verbose          打开 worker 详细日志\n"
      << "  -h, --help         显示帮助\n";
}

namespace {

bool ParseInt(const char *text, int *out) {
  if (text == nullptr || *text == '\0') {
    return false;
  }
  char *end = nullptr;
  const long v = std::strtol(text, &end, 10);
  if (*end != '\0' || v < 0) {
    return false;
  }
  *out = static_cast<int>(v);
  return true;
}

}  // namespace

bool ParseArgs(int argc, char **argv, LoadOptions *out) {
  for (int i = 1; i < argc; ++i) {
    const char *arg = argv[i];
    if (std::strcmp(arg, "-h") == 0 || std::strcmp(arg, "--help") == 0) {
      PrintUsage(argv[0]);
      return false;
    }
    if (std::strcmp(arg, "--verbose") == 0) {
      out->quiet = false;
      continue;
    }

    const auto need_value = [&](const char *name) -> const char * {
      if (i + 1 >= argc) {
        std::cerr << "缺少 " << name << " 的参数\n";
        std::exit(2);
      }
      return argv[++i];
    };

    if (std::strcmp(arg, "--workers") == 0) {
      if (!ParseInt(need_value(arg), &out->workers)) {
        std::cerr << "--workers 需要非负整数\n";
        std::exit(2);
      }
    } else if (std::strcmp(arg, "--duration") == 0) {
      if (!ParseInt(need_value(arg), &out->duration_sec)) {
        std::cerr << "--duration 需要非负整数\n";
        std::exit(2);
      }
    } else if (std::strcmp(arg, "--min-sleep") == 0) {
      if (!ParseInt(need_value(arg), &out->min_sleep_sec)) {
        std::cerr << "--min-sleep 需要非负整数\n";
        std::exit(2);
      }
    } else if (std::strcmp(arg, "--max-sleep") == 0) {
      if (!ParseInt(need_value(arg), &out->max_sleep_sec)) {
        std::cerr << "--max-sleep 需要非负整数\n";
        std::exit(2);
      }
    } else if (std::strcmp(arg, "--stagger-batch") == 0) {
      if (!ParseInt(need_value(arg), &out->stagger_batch) ||
          out->stagger_batch <= 0) {
        std::cerr << "--stagger-batch 需要正整数\n";
        std::exit(2);
      }
    } else if (std::strcmp(arg, "--stagger-ms") == 0) {
      if (!ParseInt(need_value(arg), &out->stagger_ms)) {
        std::cerr << "--stagger-ms 需要非负整数\n";
        std::exit(2);
      }
    } else if (std::strcmp(arg, "--config") == 0) {
      out->config_path = need_value(arg);
    } else {
      std::cerr << "未知参数: " << arg << "\n";
      PrintUsage(argv[0]);
      std::exit(2);
    }
  }

  if (out->workers <= 0) {
    std::cerr << "--workers 必须 > 0\n";
    std::exit(2);
  }
  if (out->max_sleep_sec < out->min_sleep_sec) {
    std::cerr << "--max-sleep 不能小于 --min-sleep\n";
    std::exit(2);
  }
  return true;
}

}  // namespace load_test
}  // namespace recommendation
