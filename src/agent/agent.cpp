#include "agent.h"

namespace recommendation {

Agent::Agent(UserId user_id) : user_id_(user_id) {}

std::vector<Video> Agent::BrowseVideos() {
  return browse_client_.Browse(user_id_);
}

}  // namespace recommendation
