#pragma once

#include <atomic>
#include <chrono>
#include <thread>

namespace recommendation {

// 节点级流量控制：排空在途请求后再做 Reload
class ServiceGate {
public:
  class Guard {
  public:
    explicit Guard(ServiceGate &gate);
    ~Guard();

    Guard(const Guard &) = delete;
    Guard &operator=(const Guard &) = delete;

    bool ok() const { return entered_; }

  private:
    ServiceGate &gate_;
    bool entered_;
  };

  bool accepting() const;

  // 等待 active_requests 归零，最多 max_wait_ms 毫秒
  bool WaitDrained(int max_wait_ms) const;

private:
  friend class Guard;
  friend class GatedConfig;
  void StopAccepting();
  void StartAccepting();

  bool TryEnter();
  void Leave();

  std::atomic<bool> accepting_{true};
  std::atomic<int> active_requests_{0};
};

} // namespace recommendation
