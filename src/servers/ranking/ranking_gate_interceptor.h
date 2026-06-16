#pragma once

#include <grpcpp/support/server_interceptor.h>

#include <memory>

namespace recommendation {

// 每个 RPC 自动构造 RequestGuard，保证 Reload drain 覆盖整个 handler 耗时。
class GatedConfigInterceptorFactory
    : public grpc::experimental::ServerInterceptorFactoryInterface {
public:
  grpc::experimental::Interceptor *
  CreateServerInterceptor(grpc::experimental::ServerRpcInfo *info) override;
};

} // namespace recommendation
