#include "registry_client.h"

#include "http_util.h"
#include "logger.h"
#include "net_util.h"
#include "service_names.h"

#include <sstream>

namespace recommendation {
namespace {

std::string JsonEscape(const std::string &text) {
  std::string out;
  out.reserve(text.size());
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
  if (colon == std::string::npos) {
    return {};
  }
  const std::size_t quote1 = json.find('"', colon + 1);
  if (quote1 == std::string::npos) {
    return {};
  }
  const std::size_t quote2 = json.find('"', quote1 + 1);
  if (quote2 == std::string::npos) {
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

bool ParseEndpointObject(const std::string &object, RegistryEndpoint *out) {
  if (out == nullptr) {
    return false;
  }
  out->service = ExtractJsonString(object, "service");
  out->instance_id = ExtractJsonString(object, "instance_id");
  out->kind = ExtractJsonString(object, "kind");
  out->host = ExtractJsonString(object, "host");
  out->port = ExtractJsonInt(object, "port");
  out->pid = ExtractJsonInt(object, "pid");
  return !out->service.empty() && !out->host.empty() && out->port > 0;
}

void ParseEndpointArray(const std::string &json,
                        std::vector<RegistryEndpoint> *out) {
  std::size_t pos = 0;
  while (true) {
    const std::size_t obj_start = json.find('{', pos);
    if (obj_start == std::string::npos) {
      break;
    }
    const std::size_t obj_end = json.find('}', obj_start);
    if (obj_end == std::string::npos) {
      break;
    }
    RegistryEndpoint endpoint;
    if (ParseEndpointObject(json.substr(obj_start, obj_end - obj_start + 1),
                            &endpoint)) {
      out->push_back(std::move(endpoint));
    }
    pos = obj_end + 1;
  }
}

void LogRegistryHttpFailure(const char *op, const std::string &registry_address,
                            const std::string &path,
                            const HttpResponse &response) {
  if (response.status_code > 0) {
    LOG(ERROR) << "[registry_client] " << op << " 失败 registry=" << registry_address
               << " path=" << path << " status=" << response.status_code
               << " body=" << response.body;
    return;
  }
  LOG(ERROR) << "[registry_client] " << op
             << " 失败 registry=" << registry_address << " path=" << path
             << "（无法连接名字服务或未收到 HTTP 响应）";
}

}  // namespace

std::string RegistryEndpoint::Address() const {
  return FormatHostPort(host, port);
}

RegistryClient::RegistryClient(std::string registry_address)
    : registry_address_(std::move(registry_address)) {}

bool RegistryClient::FromConfig(
    const std::shared_ptr<const ConfigSnapshot> &snap, RegistryClient *out) {
  if (!out || !snap) {
    return false;
  }
  std::string address;
  if (!snap->RequireString(kRegistryAddressKey, address)) {
    return false;
  }
  *out = RegistryClient(std::move(address));
  return true;
}

bool RegistryClient::Register(const RegistryEndpoint &endpoint) {
  std::ostringstream body;
  body << '{'
       << "\"service\":\"" << JsonEscape(endpoint.service) << "\","
       << "\"instance_id\":\"" << JsonEscape(endpoint.instance_id) << "\","
       << "\"kind\":\"" << JsonEscape(endpoint.kind) << "\","
       << "\"host\":\"" << JsonEscape(endpoint.host) << "\","
       << "\"port\":" << endpoint.port << ','
       << "\"pid\":" << endpoint.pid << '}';

  HttpResponse response;
  if (!HttpRequest(registry_address_, "PUT", "/v1/register", body.str(),
                   &response)) {
    LogRegistryHttpFailure("Register", registry_address_, "/v1/register",
                           response);
    return false;
  }
  return true;
}

bool RegistryClient::Deregister(const std::string &service,
                                const std::string &instance_id) {
  const std::string path =
      "/v1/register/" + service + '/' + instance_id;
  HttpResponse response;
  if (!HttpRequest(registry_address_, "DELETE", path, "", &response)) {
    LogRegistryHttpFailure("Deregister", registry_address_, path, response);
    return false;
  }
  return true;
}

bool RegistryClient::Resolve(const std::string &service,
                             std::string *address_out) {
  if (address_out == nullptr) {
    LOG(ERROR) << "[registry_client] Resolve 失败 service=" << service
               << "（address_out 为空）";
    return false;
  }
  HttpResponse response;
  const std::string path = "/v1/resolve/" + service;
  if (!HttpRequest(registry_address_, "GET", path, "", &response)) {
    LogRegistryHttpFailure("Resolve", registry_address_, path, response);
    return false;
  }
  const std::string address = ExtractJsonString(response.body, "address");
  if (address.empty()) {
    LOG(ERROR) << "[registry_client] Resolve 解析 address 失败 service="
               << service << " registry=" << registry_address_
               << " body=" << response.body;
    return false;
  }
  *address_out = address;
  return true;
}

bool RegistryClient::ListInstances(const std::string &service,
                                   std::vector<RegistryEndpoint> *out) {
  if (out == nullptr) {
    LOG(ERROR) << "[registry_client] ListInstances 失败 service=" << service
               << "（out 为空）";
    return false;
  }
  out->clear();
  HttpResponse response;
  const std::string path = "/v1/instances/" + service;
  if (!HttpRequest(registry_address_, "GET", path, "", &response)) {
    LogRegistryHttpFailure("ListInstances", registry_address_, path, response);
    return false;
  }
  ParseEndpointArray(response.body, out);
  return true;
}

bool RegistryClient::ListAll(std::vector<RegistryEndpoint> *out) {
  if (out == nullptr) {
    LOG(ERROR) << "[registry_client] ListAll 失败（out 为空）";
    return false;
  }
  out->clear();
  HttpResponse response;
  if (!HttpRequest(registry_address_, "GET", "/v1/all", "", &response)) {
    LogRegistryHttpFailure("ListAll", registry_address_, "/v1/all", response);
    return false;
  }
  ParseEndpointArray(response.body, out);
  return true;
}

}  // namespace recommendation
