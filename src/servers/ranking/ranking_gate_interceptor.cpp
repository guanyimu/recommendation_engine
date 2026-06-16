#include "ranking_gate_interceptor.h"

#include "gated_config_reloader.h"

namespace recommendation {
namespace {

class GatedConfigServerInterceptor : public grpc::experimental::Interceptor {
public:
  explicit GatedConfigServerInterceptor(
      grpc::experimental::ServerRpcInfo * /*info*/)
      : request_guard_() {}

  void Intercept(grpc::experimental::InterceptorBatchMethods *methods) override {
    methods->Proceed();
  }

private:
  GatedConfig::RequestGuard request_guard_;
};

} // namespace

grpc::experimental::Interceptor *GatedConfigInterceptorFactory::CreateServerInterceptor(
    grpc::experimental::ServerRpcInfo *info) {
  return new GatedConfigServerInterceptor(info);
}

} // namespace recommendation
