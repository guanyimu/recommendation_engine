#include "ranking_grpc_server.h"

#include <iostream>

namespace recommendation {
namespace {

bool ValidateRankingConfig(GatedConfig& config, std::string* err) {
  std::string ranking_address;
  std::size_t recommend_count = 0;
  if (!config.RequireString("ranking_address", &ranking_address)) {
    if (err) {
      *err = "conf 缺少 ranking_address 或值为空";
    }
    return false;
  }
  if (!config.RequireSizeT("recommend_count", &recommend_count)) {
    if (err) {
      *err = "conf 缺少 recommend_count 或值非法";
    }
    return false;
  }
  return true;
}

}  // namespace

RankingGrpcServer::RankingGrpcServer(GatedConfig& config,
                                     std::string address)
    : config_(config), service_(config), address_(std::move(address)) {}

RankingGrpcServer::~RankingGrpcServer() { Shutdown(); }

void RankingGrpcServer::Run() {
  grpc::ServerBuilder builder;
  builder.AddListeningPort(address_, grpc::InsecureServerCredentials());
  builder.RegisterService(&service_);
  server_ = builder.BuildAndStart();
  if (!server_) {
    std::cerr << "[ranking] 无法监听地址: " << address_ << '\n';
    return;
  }
  server_->Wait();
}

void RankingGrpcServer::Shutdown() {
  if (server_) {
    server_->Shutdown();
    server_.reset();
  }
}

bool RankingGrpcServer::Reload() {
  if (!config_.Reload(ValidateRankingConfig)) {
    return false;
  }

  std::string ranking_address;
  std::size_t recommend_count = 0;
  config_.RequireString("ranking_address", &ranking_address);
  config_.RequireSizeT("recommend_count", &recommend_count);
  std::cout << "[ranking] ranking_address=" << ranking_address
            << " recommend_count=" << recommend_count << '\n';
  return true;
}

}  // namespace recommendation
