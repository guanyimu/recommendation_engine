#include "ranking_server.h"

#include "config.h"
#include "logger.h"

namespace recommendation {

RankingServer::RankingServer() {
  const std::shared_ptr<const ConfigSnapshot> snap = Config::GetSnapshot();
  if (!snap) {
    LOG(ERROR) << "[ranking] 配置未加载";
    return;
  }
  if (!snap->RequireString("ranking_address", address_)) {
    LOG(ERROR) << "[ranking] 构造失败，监听地址未配置";
  }
}

RankingServer::~RankingServer() { Shutdown(); }

bool RankingServer::StartListening() {
  std::lock_guard<std::mutex> lock(mu_);
  if (shutdown_requested_) {
    return false;
  }

  grpc::ServerBuilder builder;
  builder.AddChannelArgument(GRPC_ARG_ALLOW_REUSEPORT, 0);

  int bound_port = 0;
  builder.AddListeningPort(address_, grpc::InsecureServerCredentials(),
                           &bound_port);
  builder.RegisterService(&service_);
  server_ = builder.BuildAndStart();
  if (!server_ || bound_port == 0) {
    LOG(ERROR) << "[ranking] 无法监听地址（端口可能被占用）: " << address_;
    server_.reset();
    return false;
  }

  LOG(INFO) << "[ranking] gRPC 已绑定 " << address_ << " 端口 " << bound_port;
  return true;
}

void RankingServer::StopListeningLocked() { server_.reset(); }

bool RankingServer::Run() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    shutdown_requested_ = false;
  }

  while (true) {
    if (!StartListening()) {
      return false;
    }

    grpc::Server *server_ptr = nullptr;
    {
      std::lock_guard<std::mutex> lock(mu_);
      server_ptr = server_.get();
    }

    server_ptr->Wait();

    bool should_exit = false;
    bool should_restart = false;
    {
      std::lock_guard<std::mutex> lock(mu_);
      StopListeningLocked();
      should_exit = shutdown_requested_;
      should_restart = restart_for_reload_;
      restart_for_reload_ = false;
    }

    if (should_exit) {
      break;
    }
    if (should_restart) {
      continue;
    }
    break;
  }

  return true;
}

void RankingServer::Shutdown() {
  std::lock_guard<std::mutex> lock(mu_);
  shutdown_requested_ = true;
  if (server_) {
    server_->Shutdown();
  }
}

// TODO
// 这种reload应该也要修改，首先是conf的reload和这个reload没有必然的因果关系，不会关联调用
// 然后是这里读取的recommend_count没用，现在是再ranking_engine又读取了一遍，所以有一次conf修改engine和server不同步的风险
// 他应该做到比如这个是一个服务器，那所有conf中有关的函数都在这里解析，然后由他统一负责reload，这个reload也应该跟conf的reload是同步的。
// 还有就是现在reload了不管监听地址有没有变，都重新连接，是如果地址没变这个操作很轻，还是设计问题应该判断以下用不用重连？
// 我还有一个疑问，就是如果rpc调用在一个线程中在开着，然后这里给他server_->Shutdown()了，会怎么样，服务之间断掉还是什么

bool RankingServer::Reload(const std::string &config_path) {
  if (!Config::Reload(config_path)) {
    return false;
  }

  const std::shared_ptr<const ConfigSnapshot> snap = Config::GetSnapshot();
  if (!snap) {
    LOG(ERROR) << "[ranking] Reload 后配置快照为空";
    return false;
  }

  std::string new_address;
  if (!snap->RequireString("ranking_address", new_address)) {
    return false;
  }

  int recommend_count = 0;
  if (!snap->RequireNonNegativeInt("recommend_count", recommend_count)) {
    return false;
  }

  std::lock_guard<std::mutex> lock(mu_);
  if (new_address == address_) {
    LOG(INFO) << "[ranking] Reload 完成，监听地址未变 address=" << address_
              << " recommend_count=" << recommend_count;
    return true;
  }

  LOG(INFO) << "[ranking] Reload 变更监听地址 " << address_ << " -> "
            << new_address << " recommend_count=" << recommend_count;
  address_ = std::move(new_address);
  restart_for_reload_ = true;
  if (server_) {
    server_->Shutdown();
  }
  return true;
}

} // namespace recommendation
