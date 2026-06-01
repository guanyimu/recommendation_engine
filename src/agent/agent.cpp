#include "agent.h"

namespace recommendation {

Agent::Agent(UserId user_id) : user_id_(user_id) {}

std::vector<Video> Agent::BrowseVideos(const MainServer& main_server) const {
  return main_server.HandleBrowse(user_id_);
}

}  // namespace recommendation
