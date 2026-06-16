#include "service_gate.h"

#include "logger.h"

namespace recommendation {

ServiceGate::Guard::Guard(ServiceGate &gate) : gate_(gate), entered_(false) {
  entered_ = gate_.TryEnter();
}

ServiceGate::Guard::~Guard() {
  if (entered_) {
    gate_.Leave();
  }
}

void ServiceGate::StopAccepting() {
  accepting_.store(false, std::memory_order_release);
}

void ServiceGate::StartAccepting() {
  accepting_.store(true, std::memory_order_release);
}

bool ServiceGate::accepting() const { return accepting_.load(); }

bool ServiceGate::WaitDrained(int max_wait_ms) const {
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(max_wait_ms);

  while (active_requests_.load() > 0 &&
         std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  const int remaining = active_requests_.load();
  if (remaining > 0) {
    LOG(ERROR) << "[gate] WaitDrained 超时，仍有 " << remaining
               << " 个在途请求未结束（max_wait_ms=" << max_wait_ms << "）";
    return false;
  }
  return true;
}

bool ServiceGate::TryEnter() {
  // 先乐观 +1，再确认 accepting；避免 load(true) 与 ++ 之间被 StopAccepting/WaitDrained 穿过
  if (!accepting_.load(std::memory_order_acquire)) {
    return false;
  }
  active_requests_.fetch_add(1, std::memory_order_acq_rel);
  if (accepting_.load(std::memory_order_acquire)) {
    return true;
  }
  active_requests_.fetch_sub(1, std::memory_order_acq_rel);
  return false;
}

void ServiceGate::Leave() { --active_requests_; }

} // namespace recommendation
