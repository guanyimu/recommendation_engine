#include "load_options.h"
#include "worker.h"
#include "worker_stats.h"

#include "config.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

struct ChildSlot {
  pid_t pid{-1};
  int read_fd{-1};
};

bool ReadWorkerStats(int fd, recommendation::load_test::WorkerStats *out) {
  std::uint8_t buffer[sizeof(recommendation::load_test::WorkerStats)];
  std::size_t got = 0;
  while (got < sizeof(buffer)) {
    const ssize_t n = read(fd, buffer + got, sizeof(buffer) - got);
    if (n <= 0) {
      return false;
    }
    got += static_cast<std::size_t>(n);
  }
  *out = *reinterpret_cast<const recommendation::load_test::WorkerStats *>(
      buffer);
  return true;
}

int ExpectedVideoCount(const recommendation::load_test::LoadOptions &opts) {
  if (!recommendation::Config::Reload(opts.config_path.c_str())) {
    return -1;
  }
  const std::shared_ptr<const recommendation::ConfigSnapshot> snap =
      recommendation::Config::GetSnapshot();
  if (!snap) {
    return -1;
  }
  int count = 0;
  if (!snap->RequireNonNegativeInt("recommend_count", count)) {
    return -1;
  }
  return count;
}

}  // namespace

int main(int argc, char **argv) {
  recommendation::load_test::LoadOptions options;
  if (!recommendation::load_test::ParseArgs(argc, argv, &options)) {
    return 0;
  }

  const int expected_videos = ExpectedVideoCount(options);
  if (expected_videos < 0) {
    std::cerr << "无法读取配置: " << options.config_path << "\n";
    return 1;
  }

  std::vector<ChildSlot> children;
  children.reserve(static_cast<std::size_t>(options.workers));

  const auto started = std::chrono::steady_clock::now();

  for (int i = 0; i < options.workers; ++i) {
    int pipefd[2];
    if (pipe(pipefd) != 0) {
      std::cerr << "pipe() 失败\n";
      return 1;
    }

    const pid_t pid = fork();
    if (pid < 0) {
      std::cerr << "fork() 失败\n";
      return 1;
    }

    if (pid == 0) {
      close(pipefd[0]);
      const recommendation::load_test::WorkerStats stats =
          recommendation::load_test::RunWorker(options, i, expected_videos);
      const ssize_t n =
          write(pipefd[1], &stats, sizeof(stats));
      close(pipefd[1]);
      _exit(n == static_cast<ssize_t>(sizeof(stats)) &&
                    stats.fail_count == 0
                ? 0
                : 1);
    }

    close(pipefd[1]);
    children.push_back(ChildSlot{pid, pipefd[0]});

    if (options.stagger_batch > 0 && options.stagger_ms > 0 &&
        (i + 1) % options.stagger_batch == 0) {
      std::this_thread::sleep_for(
          std::chrono::milliseconds(options.stagger_ms));
    }
  }

  recommendation::load_test::WorkerStats total;
  int crashed = 0;

  for (ChildSlot &child : children) {
    recommendation::load_test::WorkerStats stats;
    const bool got_stats = ReadWorkerStats(child.read_fd, &stats);
    close(child.read_fd);

    int status = 0;
    if (waitpid(child.pid, &status, 0) < 0) {
      ++crashed;
      continue;
    }

    if (!got_stats) {
      ++crashed;
      continue;
    }
    if (!WIFEXITED(status)) {
      ++crashed;
      continue;
    }

    recommendation::load_test::Merge(&total, stats);
  }

  const double elapsed_sec =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - started)
          .count();

  const recommendation::load_test::LoadReport report =
      recommendation::load_test::BuildReport(options.workers, crashed,
                                               options.duration_sec, elapsed_sec,
                                               total);
  recommendation::load_test::PrintReport(std::cout, report, expected_videos);

  if (crashed > 0 || total.fail_count > 0) {
    return 1;
  }
  return 0;
}
