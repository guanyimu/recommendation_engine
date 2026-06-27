#include "worker_stats.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>

namespace recommendation {
namespace load_test {

void Merge(WorkerStats *into, const WorkerStats &add) {
  into->success_count += add.success_count;
  into->fail_count += add.fail_count;
  into->latency_sum_ms += add.latency_sum_ms;
  if (add.latency_min_ms > 0) {
    into->latency_min_ms =
        into->latency_min_ms == 0
            ? add.latency_min_ms
            : std::min(into->latency_min_ms, add.latency_min_ms);
  }
  into->latency_max_ms = std::max(into->latency_max_ms, add.latency_max_ms);
}

LoadReport BuildReport(int workers, int crashed, int duration_sec,
                       double elapsed_sec, const WorkerStats &total) {
  LoadReport report;
  report.workers = workers;
  report.crashed_workers = crashed;
  report.duration_sec = duration_sec;
  report.elapsed_sec = elapsed_sec;
  report.total = total;
  return report;
}

void PrintReport(std::ostream &out, const LoadReport &report,
                 int expected_video_count) {
  const std::uint32_t ok = report.total.success_count;
  const std::uint32_t fail = report.total.fail_count;
  const std::uint32_t total_rpc = ok + fail;
  const std::uint64_t avg_ms =
      ok > 0 ? report.total.latency_sum_ms / ok : 0;

  out << std::fixed << std::setprecision(2);
  out << "========== Browse 压测结果 ==========\n";
  out << "Worker 进程:     " << report.workers << "\n";
  out << "每 worker 时长:  " << report.duration_sec << " s\n";
  out << "总耗时:          " << report.elapsed_sec << " s\n";
  out << "期望视频数:      " << expected_video_count << "\n";
  out << "成功 RPC:        " << ok << "\n";
  out << "失败 RPC:        " << fail << "\n";
  out << "崩溃 worker:     " << report.crashed_workers << "\n";
  if (total_rpc > 0) {
    out << "成功率:          " << (100.0 * ok / total_rpc) << " %\n";
    out << "QPS:             " << (total_rpc / report.elapsed_sec) << "\n";
  } else {
    out << "成功率:          N/A\n";
  }
  if (ok > 0 && report.total.latency_min_ms > 0) {
    out << "延迟 ms (min/avg/max): "
        << report.total.latency_min_ms << " / " << avg_ms << " / "
        << report.total.latency_max_ms << "\n";
  }
  out << "=====================================\n";
}

}  // namespace load_test
}  // namespace recommendation
