#include "grpc_listen_util.h"

#include "logger.h"
#include "net_util.h"

namespace recommendation {

bool StartGrpcWithRetry(
    const std::function<void(grpc::ServerBuilder &)> &configure_builder,
    std::unique_ptr<grpc::Server> *server_out, int *bound_port_out,
    int max_attempts) {
  if (server_out == nullptr || bound_port_out == nullptr || max_attempts <= 0) {
    return false;
  }

  const std::string listen = FormatHostPort(kLoopbackHost, 0);

  for (int attempt = 0; attempt < max_attempts; ++attempt) {
    grpc::ServerBuilder builder;
    builder.AddChannelArgument(GRPC_ARG_ALLOW_REUSEPORT, 0);
    configure_builder(builder);

    int bound_port = 0;
    builder.AddListeningPort(listen, grpc::InsecureServerCredentials(),
                           &bound_port);
    std::unique_ptr<grpc::Server> server = builder.BuildAndStart();
    if (server && bound_port > 0) {
      *server_out = std::move(server);
      *bound_port_out = bound_port;
      return true;
    }

    LOG(WARNING) << "[grpc] 监听失败 attempt=" << (attempt + 1) << '/'
                 << max_attempts;
    server.reset();
  }

  LOG(ERROR) << "[grpc] " << max_attempts
             << " 次尝试后仍无法绑定 " << listen;
  return false;
}

}  // namespace recommendation
