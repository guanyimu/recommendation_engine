#pragma once

#include "ranking_service_impl.h"

#include <memory>
#include <mutex>
#include <string>

#include <grpcpp/grpcpp.h>

namespace recommendation {

class RankingServer {
 public:
  RankingServer();
  ~RankingServer();

  RankingServer(const RankingServer &) = delete;
  RankingServer &operator=(const RankingServer &) = delete;

  bool Run();
  void Shutdown();
  bool Reload(const std::string &config_path);

  const std::string &address() const { return address_; }

 private:
  bool StartListening();
  void StopListeningLocked();

  RankingServiceImpl service_;
  std::string address_;
  std::unique_ptr<grpc::Server> server_;
  std::mutex mu_;
  bool shutdown_requested_{false};
  bool restart_for_reload_{false};
};

}  // namespace recommendation
