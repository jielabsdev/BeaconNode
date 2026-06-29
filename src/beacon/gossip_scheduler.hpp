#pragma once

#include "beacon/config.hpp"
#include "beacon/types.hpp"

#include <vector>

namespace beacon {

class GossipScheduler {
 public:
  explicit GossipScheduler(GossipConfig config);

  std::vector<AgentRecord> select_push_entries(const std::vector<AgentRecord>& registry) const;

 private:
  GossipConfig config_;
};

}  // namespace beacon

