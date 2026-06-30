#include "registry_store.h"

#include "net_util.h"
#include "service_names.h"

namespace recommendation {
namespace registry {

std::string RegistryStore::Key(const std::string &service,
                               const std::string &instance_id) {
  return service + '/' + instance_id;
}

void RegistryStore::Upsert(Record record) {
  std::lock_guard<std::mutex> lock(mu_);
  records_[Key(record.service, record.instance_id)] = std::move(record);
}

bool RegistryStore::Remove(const std::string &service,
                           const std::string &instance_id) {
  std::lock_guard<std::mutex> lock(mu_);
  return records_.erase(Key(service, instance_id)) > 0;
}

bool RegistryStore::Resolve(const std::string &service,
                            std::string *address_out) {
  if (address_out == nullptr) {
    return false;
  }

  std::lock_guard<std::mutex> lock(mu_);
  for (const auto &item : records_) {
    const Record &record = item.second;
    if (record.service == service && record.kind == kRegistryKindEntry) {
      *address_out = FormatHostPort(record.host, record.port);
      return true;
    }
  }

  std::vector<const Record *> instances;
  instances.reserve(records_.size());
  for (const auto &item : records_) {
    const Record &record = item.second;
    if (record.service == service && record.kind == kRegistryKindInstance) {
      instances.push_back(&record);
    }
  }
  if (instances.empty()) {
    return false;
  }

  std::size_t &index = rr_index_[service];
  const Record &picked = *instances[index % instances.size()];
  ++index;
  *address_out = FormatHostPort(picked.host, picked.port);
  return true;
}

std::vector<Record> RegistryStore::ListInstances(
    const std::string &service) const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<Record> out;
  for (const auto &item : records_) {
    const Record &record = item.second;
    if (record.service == service && record.kind == kRegistryKindInstance) {
      out.push_back(record);
    }
  }
  return out;
}

std::vector<Record> RegistryStore::ListAll() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<Record> out;
  out.reserve(records_.size());
  for (const auto &item : records_) {
    out.push_back(item.second);
  }
  return out;
}

}  // namespace registry
}  // namespace recommendation
