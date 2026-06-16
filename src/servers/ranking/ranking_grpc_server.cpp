#include "ranking_grpc_server.h"

#include "gated_config_reloader.h"
#include "logger.h"
#include "ranking_gate_interceptor.h"

namespace recommendation {

RankingGrpcServer::RankingGrpcServer(std::string address)
    : address_(std::move(address)) {}

RankingGrpcServer::~RankingGrpcServer() { Shutdown(); }

bool RankingGrpcServer::Run() {
  grpc::ServerBuilder builder;
  builder.AddChannelArgument(GRPC_ARG_ALLOW_REUSEPORT, 0);

  int bound_port = 0;
  builder.AddListeningPort(address_, grpc::InsecureServerCredentials(),
                           &bound_port);
  std::vector<
      std::unique_ptr<grpc::experimental::ServerInterceptorFactoryInterface>>
      interceptor_factories;
  interceptor_factories.push_back(
      std::make_unique<GatedConfigInterceptorFactory>());
  builder.experimental().SetInterceptorCreators(
      std::move(interceptor_factories));
  builder.RegisterService(&service_);
  server_ = builder.BuildAndStart();
  if (!server_ || bound_port == 0) {
    LOG(ERROR) << "[ranking] 无法监听地址（端口可能被占用）: " << address_;
    return false;
  }

  LOG(INFO) << "[ranking] gRPC 已绑定端口 " << bound_port;
  server_->Wait();
  return true;
}

void RankingGrpcServer::Shutdown() {
  if (server_) {
    server_->Shutdown();
    server_.reset();
  }
}
// TODO:这里reload完全没用阿
bool RankingGrpcServer::Reload() {
  GatedConfig &config = GatedConfig::Instance();
  if (!config.Reload("configs/ranking.conf")) {
    return false;
  }

  std::string ranking_address;
  int recommend_count = 0;
  config.RequireString("ranking_address", ranking_address);
  config.RequireInt("recommend_count", recommend_count);
  LOG(INFO) << "[ranking] ranking_address=" << ranking_address
            << " recommend_count=" << recommend_count;
  return true;
}

} // namespace recommendation
