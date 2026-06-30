#include "http_util.h"

#include "net_util.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <sstream>

namespace recommendation {
namespace {

bool ConnectHostPort(const std::string &host, int port, int *fd_out) {
  const int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return false;
  }

  sockaddr_in addr {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(port));
  if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
    close(fd);
    return false;
  }

  if (connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
    close(fd);
    return false;
  }

  *fd_out = fd;
  return true;
}

bool ReadHttpResponse(int fd, HttpResponse *response) {
  std::string data;
  char buffer[4096];
  while (true) {
    const ssize_t n = read(fd, buffer, sizeof(buffer));
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    if (n == 0) {
      break;
    }
    data.append(buffer, static_cast<std::size_t>(n));
    if (data.size() > 1024 * 1024) {
      return false;
    }
  }

  const std::size_t header_end = data.find("\r\n\r\n");
  if (header_end == std::string::npos) {
    return false;
  }

  const std::string header = data.substr(0, header_end);
  response->body = data.substr(header_end + 4);

  const std::size_t space1 = header.find(' ');
  const std::size_t space2 = header.find(' ', space1 + 1);
  if (space1 == std::string::npos || space2 == std::string::npos) {
    return false;
  }
  try {
    response->status_code = std::stoi(header.substr(space1 + 1, space2 - space1 - 1));
  } catch (...) {
    return false;
  }
  return true;
}

}  // namespace

bool HttpRequest(const std::string &registry_address, const std::string &method,
                 const std::string &path, const std::string &body,
                 HttpResponse *response) {
  if (response == nullptr) {
    return false;
  }

  std::string host;
  int port = 0;
  if (!SplitHostPort(registry_address, &host, &port)) {
    return false;
  }

  int fd = -1;
  if (!ConnectHostPort(host, port, &fd)) {
    return false;
  }

  std::ostringstream request;
  request << method << ' ' << path << " HTTP/1.1\r\n"
          << "Host: " << host << "\r\n"
          << "Connection: close\r\n"
          << "Content-Type: application/json\r\n"
          << "Content-Length: " << body.size() << "\r\n"
          << "\r\n"
          << body;

  const std::string payload = request.str();
  std::size_t sent = 0;
  while (sent < payload.size()) {
    const ssize_t n = write(fd, payload.data() + sent, payload.size() - sent);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      close(fd);
      return false;
    }
    sent += static_cast<std::size_t>(n);
  }

  const bool ok = ReadHttpResponse(fd, response);
  close(fd);
  return ok && response->status_code >= 200 && response->status_code < 300;
}

}  // namespace recommendation
