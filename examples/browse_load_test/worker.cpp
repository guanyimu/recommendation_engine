#include "worker.h"

#include "agent.h"
#include "config.h"
#include "logger.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <thread>
#include <unistd.h>

namespace recommendation {
namespace load_test {
namespace {

constexpr char kLogDir[] = "logs/load_test";

UserId RandomUserId(int worker_id) {
  std::random_device rd;
  const auto seed = static_cast<std::uint32_t>(
      std::chrono::steady_clock::now().time_since_epoch().count() ^
      static_cast<std::uint64_t>(::getpid()) ^
      static_cast<std::uint64_t>(worker_id) ^ rd());
  std::mt19937 rng(seed);
  std::uniform_int_distribution<UserId> dist(1, 1'000'000'000);
  return dist(rng);
}

int RandomSleepSec(int min_sec, int max_sec, std::mt19937 &rng) {
  if (max_sec <= min_sec) {
    return min_sec;
  }
  std::uniform_int_distribution<int> dist(min_sec, max_sec);
  return dist(rng);
}

void RecordRpc(WorkerStats *stats, bool ok, std::uint64_t latency_ms) {
  if (ok) {
    ++stats->success_count;
    stats->latency_sum_ms += latency_ms;
    stats->latency_min_ms =
        stats->latency_min_ms == 0
            ? latency_ms
            : std::min(stats->latency_min_ms, latency_ms);
    stats->latency_max_ms = std::max(stats->latency_max_ms, latency_ms);
  } else {
    ++stats->fail_count;
  }
}

}  // namespace

WorkerStats RunWorker(const LoadOptions &options, int worker_id,
                      int expected_video_count) {
  WorkerStats stats;

  LogOptions log_opts;
  log_opts.server_name = "load_worker";
  log_opts.log_dir = kLogDir;
  log_opts.also_log_to_stderr = false;
  log_opts.min_log_level = options.quiet ? 2 : 0;
  int argc = 0;
  char dummy[] = "worker";
  char *argv[] = {dummy, nullptr};
  if (!InitLogging(argc, argv, log_opts)) {
    ++stats.fail_count;
    return stats;
  }

  if (!Config::Reload(options.config_path.c_str())) {
    ShutdownLogging();
    ++stats.fail_count;
    return stats;
  }

  Agent agent(RandomUserId(worker_id));
  std::mt19937 rng(static_cast<std::uint32_t>(
      std::chrono::steady_clock::now().time_since_epoch().count() ^
      static_cast<std::uint64_t>(::getpid()) ^
      static_cast<std::uint64_t>(worker_id)));

  const auto run_once = [&]() {
    const auto t0 = std::chrono::steady_clock::now();
    const auto videos = agent.BrowseVideos();
    const auto ms = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0)
            .count());
    const bool ok = static_cast<int>(videos.size()) == expected_video_count;
    RecordRpc(&stats, ok, ms);
  };

  if (options.duration_sec <= 0) {
    run_once();
    ShutdownLogging();
    return stats;
  }

  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::seconds(options.duration_sec);
  while (std::chrono::steady_clock::now() < deadline) {
    run_once();

    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) {
      break;
    }

    const int sleep_sec =
        RandomSleepSec(options.min_sleep_sec, options.max_sleep_sec, rng);
    const auto wake_at = now + std::chrono::seconds(sleep_sec);
    if (wake_at >= deadline) {
      std::this_thread::sleep_until(deadline);
      break;
    }
    std::this_thread::sleep_for(std::chrono::seconds(sleep_sec));
  }

  ShutdownLogging();
  return stats;
}

}  // namespace load_test
}  // namespace recommendation
