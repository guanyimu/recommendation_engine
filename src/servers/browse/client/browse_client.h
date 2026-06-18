#pragma once

#include "types.h"

#include <memory>
#include <string>
#include <vector>

namespace recommendation {

class BrowseClient {
 public:
  BrowseClient();
  ~BrowseClient();

  BrowseClient(const BrowseClient &) = delete;
  BrowseClient &operator=(const BrowseClient &) = delete;
  BrowseClient(BrowseClient &&) noexcept;
  BrowseClient &operator=(BrowseClient &&) noexcept;

  std::vector<Video> Browse(UserId user_id);
  const std::string &target() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace recommendation
