#pragma once

#include <cstdint>
#include <iosfwd>

namespace recommendation {
namespace load_test {

struct WorkerStats {
  std::uint32_t success_count{0};
  std::uint32_t fail_count{0};
  std::uint64_t latency_sum_ms{0};
  std::uint64_t latency_min_ms{0};
  std::uint64_t latency_max_ms{0};
};

struct LoadReport {
  int workers{0};
  int crashed_workers{0};
  int duration_sec{0};
  WorkerStats total{};
  double elapsed_sec{0.0};
};

void Merge(WorkerStats *into, const WorkerStats &add);
LoadReport BuildReport(int workers, int crashed, int duration_sec,
                       double elapsed_sec, const WorkerStats &total);
void PrintReport(std::ostream &out, const LoadReport &report,
                 int expected_video_count);

}  // namespace load_test
}  // namespace recommendation
