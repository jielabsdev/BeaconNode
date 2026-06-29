#include "beacon/gossip_scheduler.hpp"

#include <algorithm>

namespace beacon {

GossipScheduler::GossipScheduler(GossipConfig config) : config_(config) {}

std::vector<AgentRecord> GossipScheduler::select_push_entries(const std::vector<AgentRecord>& registry) const {
  std::vector<AgentRecord> selected = registry;
  std::sort(selected.begin(), selected.end(), [](const AgentRecord& left, const AgentRecord& right) {
    return left.revision > right.revision;
  });

  if (selected.size() > config_.max_registry_items_per_round) {
    selected.resize(config_.max_registry_items_per_round);
  }

  return selected;
}

}  // namespace beacon

