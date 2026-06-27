#pragma once

#include "load_options.h"
#include "worker_stats.h"

namespace recommendation {
namespace load_test {

// 在子进程中运行：独立 Agent，Browse 循环，返回统计。
WorkerStats RunWorker(const LoadOptions &options, int worker_id,
                      int expected_video_count);

}  // namespace load_test
}  // namespace recommendation
