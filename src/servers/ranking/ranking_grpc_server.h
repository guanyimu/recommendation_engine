#pragma once

#include "gated_config_reloader.h"
#include "ranking_service_impl.h"

#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

namespace recommendation {

// gRPC Ranking 服务：Run() 阻塞直到 Shutdown()
class RankingGrpcServer {
 public:
  RankingGrpcServer(GatedConfig& config, std::string address);
  ~RankingGrpcServer();

  void Run();
  void Shutdown();
  bool Reload();

  const std::string& address() const { return address_; }

 private:
  GatedConfig& config_;
  RankingServiceImpl service_;
  std::string address_;
  std::unique_ptr<grpc::Server> server_;
};

}  // namespace recommendation
