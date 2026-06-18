#pragma once

#include "types.h"

#include <memory>
#include <string>
#include <vector>

namespace recommendation {

class VideoClient {
 public:
  VideoClient();
  ~VideoClient();

  VideoClient(const VideoClient &) = delete;
  VideoClient &operator=(const VideoClient &) = delete;
  VideoClient(VideoClient &&) noexcept;
  VideoClient &operator=(VideoClient &&) noexcept;

  std::vector<Video> Fetch(UserId user_id, const std::vector<ItemId> &item_ids);
  const std::string &target() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace recommendation
