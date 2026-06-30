#include "config.h"
#include "registry_client.h"
#include "service_names.h"

#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <set>
#include <thread>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

namespace {

constexpr char kConfigPath[] = "configs/services.conf";
constexpr int kLbPortPickMaxAttempts = 10;

bool PickLoopbackPort(int *port_out) {
  if (port_out == nullptr) {
    return false;
  }
  const int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return false;
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;

  if (bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
    close(fd);
    return false;
  }

  socklen_t len = sizeof(addr);
  if (getsockname(fd, reinterpret_cast<sockaddr *>(&addr), &len) != 0) {
    close(fd);
    return false;
  }

  *port_out = ntohs(addr.sin_port);
  close(fd);
  return true;
}

// 为 nginx 配置挑选 count 个互不相同的临时端口（仅 setup-lb 使用）。
bool PickDistinctLoopbackPorts(std::vector<int> *ports_out, int count) {
  if (ports_out == nullptr || count <= 0) {
    return false;
  }

  ports_out->clear();
  std::set<int> chosen;

  for (int i = 0; i < count; ++i) {
    bool got = false;
    for (int attempt = 0; attempt < kLbPortPickMaxAttempts; ++attempt) {
      int port = 0;
      if (!PickLoopbackPort(&port) || chosen.count(port) > 0) {
        continue;
      }
      chosen.insert(port);
      ports_out->push_back(port);
      got = true;
      break;
    }
    if (!got) {
      ports_out->clear();
      return false;
    }
  }

  return true;
}

bool LoadRegistryClient(recommendation::RegistryClient *client) {
  if (!recommendation::Config::Reload(kConfigPath)) {
    return false;
  }
  const std::shared_ptr<const recommendation::ConfigSnapshot> snap =
      recommendation::Config::GetSnapshot();
  return recommendation::RegistryClient::FromConfig(snap, client);
}

bool RegisterLb(recommendation::RegistryClient *client, const char *service,
                int port) {
  recommendation::RegistryEndpoint endpoint;
  endpoint.service = service;
  endpoint.instance_id = recommendation::kRegistryLbInstanceId;
  endpoint.kind = recommendation::kRegistryKindEntry;
  endpoint.host = "127.0.0.1";
  endpoint.port = port;
  endpoint.pid = 0;
  return client->Register(endpoint);
}

int WriteNginxConf(recommendation::RegistryClient *client,
                   const std::string &output_path) {
  auto load_lb_port = [&](const char *service) -> int {
    std::vector<recommendation::RegistryEndpoint> all;
    if (!client->ListAll(&all)) {
      return 0;
    }
    for (const auto &item : all) {
      if (item.service == service &&
          item.kind == recommendation::kRegistryKindEntry &&
          item.instance_id == recommendation::kRegistryLbInstanceId) {
        return item.port;
      }
    }
    return 0;
  };

  const int ranking_lb = load_lb_port(recommendation::kServiceRanking);
  const int video_lb = load_lb_port(recommendation::kServiceVideo);
  const int browse_lb = load_lb_port(recommendation::kServiceBrowse);
  if (ranking_lb <= 0 || video_lb <= 0 || browse_lb <= 0) {
    std::cerr << "LB 入口未注册\n";
    return 1;
  }

  std::vector<recommendation::RegistryEndpoint> ranking_instances;
  std::vector<recommendation::RegistryEndpoint> video_instances;
  std::vector<recommendation::RegistryEndpoint> browse_instances;
  if (!client->ListInstances(recommendation::kServiceRanking,
                             &ranking_instances) ||
      !client->ListInstances(recommendation::kServiceVideo, &video_instances) ||
      !client->ListInstances(recommendation::kServiceBrowse,
                             &browse_instances) ||
      ranking_instances.empty() || video_instances.empty() ||
      browse_instances.empty()) {
    std::cerr << "后端实例未就绪\n";
    return 1;
  }

  std::ofstream out(output_path);
  if (!out) {
    std::cerr << "无法写入 " << output_path << '\n';
    return 1;
  }

  std::string prefix = output_path;
  const std::size_t slash = output_path.rfind('/');
  if (slash != std::string::npos) {
    prefix = output_path.substr(0, slash);
  }

  out << "load_module /usr/lib/nginx/modules/ngx_stream_module.so;\n\n";
  out << "worker_processes 1;\n";
  out << "error_log " << prefix << "/logs/error.log info;\n";
  out << "pid " << prefix << "/nginx.pid;\n\n";
  out << "events { worker_connections 8192; }\n\n";
  out << "stream {\n";

  auto write_upstream =
      [&](const char *name,
          const std::vector<recommendation::RegistryEndpoint> &instances) {
        out << "  upstream " << name << "_backend {\n";
        for (const auto &instance : instances) {
          out << "    server " << instance.Address() << ";\n";
        }
        out << "  }\n\n";
      };

  write_upstream("ranking", ranking_instances);
  write_upstream("video", video_instances);
  write_upstream("browse", browse_instances);

  out << "  server { listen 127.0.0.1:" << ranking_lb
      << "; proxy_pass ranking_backend; }\n";
  out << "  server { listen 127.0.0.1:" << video_lb
      << "; proxy_pass video_backend; }\n";
  out << "  server { listen 127.0.0.1:" << browse_lb
      << "; proxy_pass browse_backend; }\n";
  out << "}\n";

  std::cout << "已生成 nginx 配置: " << output_path << '\n';
  return 0;
}

int CmdSetupLb(const std::string &output_path) {
  recommendation::RegistryClient client("");
  if (!LoadRegistryClient(&client)) {
    std::cerr << "无法连接名字服务配置\n";
    return 1;
  }

  std::vector<int> ports;
  if (!PickDistinctLoopbackPorts(&ports, 3)) {
    std::cerr << "挑选 LB 端口失败\n";
    return 1;
  }

  const char *services[] = {recommendation::kServiceRanking,
                            recommendation::kServiceVideo,
                            recommendation::kServiceBrowse};
  for (int i = 0; i < 3; ++i) {
    if (!RegisterLb(&client, services[i], ports[i])) {
      std::cerr << "注册 LB 失败 service=" << services[i] << '\n';
      return 1;
    }
  }

  std::cout << "已注册 LB: ranking=" << ports[0] << " video=" << ports[1]
            << " browse=" << ports[2] << '\n';
  return WriteNginxConf(&client, output_path);
}

int CmdStopAll() {
  recommendation::RegistryClient client("");
  if (!LoadRegistryClient(&client)) {
    return 1;
  }
  std::vector<recommendation::RegistryEndpoint> all;
  if (!client.ListAll(&all)) {
    return 1;
  }

  for (const auto &item : all) {
    if (item.pid > 0) {
      kill(item.pid, SIGTERM);
    }
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  for (const auto &item : all) {
    if (item.pid > 0 && kill(item.pid, 0) == 0) {
      kill(item.pid, SIGKILL);
    }
    client.Deregister(item.service, item.instance_id);
  }

  return 0;
}

int CmdPing() {
  recommendation::RegistryClient client("");
  if (!LoadRegistryClient(&client)) {
    return 1;
  }
  std::vector<recommendation::RegistryEndpoint> all;
  return client.ListAll(&all) ? 0 : 1;
}

void PrintUsage(const char *prog) {
  std::cout << "用法:\n"
            << "  " << prog << " ping\n"
            << "  " << prog << " setup-lb <output.conf>\n"
            << "  " << prog << " stop-all\n";
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    PrintUsage(argv[0]);
    return 2;
  }

  const std::string cmd = argv[1];
  if (cmd == "ping") {
    return CmdPing();
  }
  if (cmd == "setup-lb" && argc == 3) {
    return CmdSetupLb(argv[2]);
  }
  if (cmd == "stop-all") {
    return CmdStopAll();
  }

  PrintUsage(argv[0]);
  return 2;
}
