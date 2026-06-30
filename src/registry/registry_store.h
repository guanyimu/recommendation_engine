#pragma once

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace recommendation {
namespace registry {

struct Record {
  std::string service;
  std::string instance_id;
  std::string kind;
  std::string host;
  int port{0};
  int pid{0};
};

class RegistryStore {
 public:
  void Upsert(Record record);
  bool Remove(const std::string &service, const std::string &instance_id);
  bool Resolve(const std::string &service, std::string *address_out);
  std::vector<Record> ListInstances(const std::string &service) const;
  std::vector<Record> ListAll() const;

 private:
  static std::string Key(const std::string &service,
                         const std::string &instance_id);

  mutable std::mutex mu_;
  std::unordered_map<std::string, Record> records_;
  std::unordered_map<std::string, std::size_t> rr_index_;
};

}  // namespace registry
}  // namespace recommendation
