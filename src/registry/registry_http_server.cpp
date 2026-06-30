#include "registry_http_server.h"

#include "logger.h"
#include "net_util.h"
#include "service_names.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <sstream>

namespace recommendation {
namespace registry {
namespace {

std::string JsonEscape(const std::string &text) {
  std::string out;
  for (char c : text) {
    if (c == '"' || c == '\\') {
      out.push_back('\\');
    }
    out.push_back(c);
  }
  return out;
}

std::string ExtractJsonString(const std::string &json, const std::string &key) {
  const std::string needle = '"' + key + '"';
  const std::size_t key_pos = json.find(needle);
  if (key_pos == std::string::npos) {
    return {};
  }
  const std::size_t colon = json.find(':', key_pos + needle.size());
  const std::size_t quote1 = json.find('"', colon + 1);
  const std::size_t quote2 = json.find('"', quote1 + 1);
  if (colon == std::string::npos || quote1 == std::string::npos ||
      quote2 == std::string::npos) {
    return {};
  }
  return json.substr(quote1 + 1, quote2 - quote1 - 1);
}

int ExtractJsonInt(const std::string &json, const std::string &key) {
  const std::string needle = '"' + key + '"';
  const std::size_t key_pos = json.find(needle);
  if (key_pos == std::string::npos) {
    return 0;
  }
  const std::size_t colon = json.find(':', key_pos + needle.size());
  if (colon == std::string::npos) {
    return 0;
  }
  std::size_t i = colon + 1;
  while (i < json.size() && (json[i] == ' ' || json[i] == '\t')) {
    ++i;
  }
  int sign = 1;
  if (i < json.size() && json[i] == '-') {
    sign = -1;
    ++i;
  }
  int value = 0;
  bool found = false;
  while (i < json.size() && json[i] >= '0' && json[i] <= '9') {
    found = true;
    value = value * 10 + (json[i] - '0');
    ++i;
  }
  return found ? value * sign : 0;
}

std::string HttpResponseText(int status, const std::string &body) {
  std::ostringstream out;
  out << "HTTP/1.1 " << status << ' '
      << (status == 200 ? "OK" : status == 404 ? "Not Found" : "Bad Request")
      << "\r\n"
      << "Content-Type: application/json\r\n"
      << "Connection: close\r\n"
      << "Content-Length: " << body.size() << "\r\n"
      << "\r\n"
      << body;
  return out.str();
}

std::string RecordToJson(const Record &record) {
  std::ostringstream out;
  out << '{'
      << "\"service\":\"" << JsonEscape(record.service) << "\","
      << "\"instance_id\":\"" << JsonEscape(record.instance_id) << "\","
      << "\"kind\":\"" << JsonEscape(record.kind) << "\","
      << "\"host\":\"" << JsonEscape(record.host) << "\","
      << "\"port\":" << record.port << ','
      << "\"pid\":" << record.pid << '}';
  return out.str();
}

}  // namespace

RegistryHttpServer::RegistryHttpServer(RegistryStore *store) : store_(store) {}

std::string RegistryHttpServer::HandleRequest(const std::string &request) const {
  const std::size_t line_end = request.find("\r\n");
  if (line_end == std::string::npos) {
    return HttpResponseText(400, "{\"error\":\"bad request\"}");
  }

  const std::string line = request.substr(0, line_end);
  const std::size_t sp1 = line.find(' ');
  const std::size_t sp2 = line.find(' ', sp1 + 1);
  if (sp1 == std::string::npos || sp2 == std::string::npos) {
    return HttpResponseText(400, "{\"error\":\"bad request\"}");
  }

  const std::string method = line.substr(0, sp1);
  const std::string path = line.substr(sp1 + 1, sp2 - sp1 - 1);

  const std::size_t body_start = request.find("\r\n\r\n");
  const std::string body =
      body_start == std::string::npos ? "" : request.substr(body_start + 4);

  if (method == "PUT" && path == "/v1/register") {
    Record record;
    record.service = ExtractJsonString(body, "service");
    record.instance_id = ExtractJsonString(body, "instance_id");
    record.kind = ExtractJsonString(body, "kind");
    record.host = ExtractJsonString(body, "host");
    record.port = ExtractJsonInt(body, "port");
    record.pid = ExtractJsonInt(body, "pid");
    if (record.service.empty() || record.instance_id.empty() ||
        record.host.empty() || record.port <= 0) {
      return HttpResponseText(400, "{\"error\":\"invalid register body\"}");
    }
    if (record.kind.empty()) {
      record.kind = kRegistryKindInstance;
    }
    store_->Upsert(std::move(record));
    return HttpResponseText(200, "{\"ok\":true}");
  }

  if (method == "DELETE" && path.rfind("/v1/register/", 0) == 0) {
    const std::string rest = path.substr(std::string("/v1/register/").size());
    const std::size_t slash = rest.find('/');
    if (slash == std::string::npos) {
      return HttpResponseText(400, "{\"error\":\"invalid path\"}");
    }
    const std::string service = rest.substr(0, slash);
    const std::string instance_id = rest.substr(slash + 1);
    if (!store_->Remove(service, instance_id)) {
      return HttpResponseText(404, "{\"error\":\"not found\"}");
    }
    return HttpResponseText(200, "{\"ok\":true}");
  }

  if (method == "GET" && path.rfind("/v1/resolve/", 0) == 0) {
    const std::string service = path.substr(std::string("/v1/resolve/").size());
    std::string address;
    if (!store_->Resolve(service, &address)) {
      return HttpResponseText(404, "{\"error\":\"not found\"}");
    }
    return HttpResponseText(200,
                            "{\"address\":\"" + JsonEscape(address) + "\"}");
  }

  if (method == "GET" && path.rfind("/v1/instances/", 0) == 0) {
    const std::string service =
        path.substr(std::string("/v1/instances/").size());
    const std::vector<Record> records = store_->ListInstances(service);
    std::ostringstream body_out;
    body_out << "{\"instances\":[";
    for (std::size_t i = 0; i < records.size(); ++i) {
      if (i > 0) {
        body_out << ',';
      }
      body_out << RecordToJson(records[i]);
    }
    body_out << "]}";
    return HttpResponseText(200, body_out.str());
  }

  if (method == "GET" && path == "/v1/all") {
    const std::vector<Record> records = store_->ListAll();
    std::ostringstream body_out;
    body_out << "{\"records\":[";
    for (std::size_t i = 0; i < records.size(); ++i) {
      if (i > 0) {
        body_out << ',';
      }
      body_out << RecordToJson(records[i]);
    }
    body_out << "]}";
    return HttpResponseText(200, body_out.str());
  }

  return HttpResponseText(404, "{\"error\":\"not found\"}");
}

bool RegistryHttpServer::Run(const std::string &listen_address,
                             std::atomic<bool> *ready) {
  std::string host;
  int port = 0;
  if (!SplitHostPort(listen_address, &host, &port)) {
    LOG(ERROR) << "[registry] 无效监听地址: " << listen_address;
    return false;
  }

  const int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    LOG(ERROR) << "[registry] socket 失败: " << std::strerror(errno);
    return false;
  }

  int yes = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  sockaddr_in addr {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(port));
  if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
    LOG(ERROR) << "[registry] 无效 host: " << host;
    close(server_fd);
    return false;
  }

  if (bind(server_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
    LOG(ERROR) << "[registry] bind 失败 " << listen_address << ": "
               << std::strerror(errno);
    close(server_fd);
    return false;
  }
  if (listen(server_fd, 128) != 0) {
    LOG(ERROR) << "[registry] listen 失败 " << listen_address << ": "
               << std::strerror(errno);
    close(server_fd);
    return false;
  }

  if (ready != nullptr) {
    ready->store(true);
  }

  while (true) {
    const int client_fd = accept(server_fd, nullptr, nullptr);
    if (client_fd < 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    }

    std::string request;
    char buffer[4096];
    while (true) {
      const ssize_t n = read(client_fd, buffer, sizeof(buffer));
      if (n <= 0) {
        break;
      }
      request.append(buffer, static_cast<std::size_t>(n));
      if (request.find("\r\n\r\n") != std::string::npos) {
        break;
      }
      if (request.size() > 1024 * 1024) {
        break;
      }
    }

    const std::string response = HandleRequest(request);
    std::size_t sent = 0;
    while (sent < response.size()) {
      const ssize_t n =
          write(client_fd, response.data() + sent, response.size() - sent);
      if (n < 0) {
        if (errno == EINTR) {
          continue;
        }
        break;
      }
      sent += static_cast<std::size_t>(n);
    }
    close(client_fd);
  }

  close(server_fd);
  return true;
}

}  // namespace registry
}  // namespace recommendation
