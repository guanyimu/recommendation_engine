#pragma once

#include "types.h"

#include <memory>
#include <vector>

namespace recommendation {

class RankingClient;
class VideoClient;

class BrowseEngine {
 public:
  BrowseEngine();
  ~BrowseEngine();

  BrowseEngine(const BrowseEngine &) = delete;
  BrowseEngine &operator=(const BrowseEngine &) = delete;

  std::vector<Video> Browse(UserId user_id);

 private:
  std::unique_ptr<RankingClient> ranking_client_;
  std::unique_ptr<VideoClient> video_client_;
};

}  // namespace recommendation
