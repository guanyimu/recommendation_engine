#pragma once

#include <functional>
#include <memory>

#include <grpcpp/grpcpp.h>

namespace recommendation {

constexpr int kGrpcBindMaxAttempts = 10;

// gRPC 监听 127.0.0.1:0，失败时最多重试 max_attempts 次。
bool StartGrpcWithRetry(
    const std::function<void(grpc::ServerBuilder &)> &configure_builder,
    std::unique_ptr<grpc::Server> *server_out, int *bound_port_out,
    int max_attempts = kGrpcBindMaxAttempts);

}  // namespace recommendation
