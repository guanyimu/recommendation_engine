#include "service_gate.h"

namespace recommendation {

ServiceGate::Guard::Guard(ServiceGate& gate) : gate_(gate), entered_(false) {
  entered_ = gate_.TryEnter();
}

ServiceGate::Guard::~Guard() {
  if (entered_) {
    gate_.Leave();
  }
}

void ServiceGate::StopAccepting() { accepting_.store(false); }

void ServiceGate::StartAccepting() { accepting_.store(true); }

bool ServiceGate::accepting() const { return accepting_.load(); }

void ServiceGate::WaitDrained(int max_wait_ms) const {
  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(max_wait_ms);
  
  // TODO: 这里应该有个日志记录或者别的什么，这里现在如果超时了，没有直接掐断正在运行的僵尸请求的功能
  // 我不知道这里是不是我理解不正确，实际上超时是有服务器掐断而不是这里掐断
  // 但如果是这样的话这里还超时了那就不正常，是不是应该ERROR日志？
  while (active_requests_.load() > 0 &&
         std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

bool ServiceGate::TryEnter() {
  if (!accepting_.load()) {
    return false;
  }
  ++active_requests_;
  return true;
}

void ServiceGate::Leave() { --active_requests_; }

}  // namespace recommendation
