#pragma once

#include "beacon/agent_registry.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace beacon { class IdentityService; }

namespace beacon {

struct SyncPlan {
  std::uint64_t local_revision = 0;
  std::vector<AgentRecord> push_entries;
};

class RegistrySync {
 public:
  explicit RegistrySync(std::size_t max_entries_per_round);

  SyncPlan plan_push_pull(const AgentRegistry& registry, std::uint64_t remote_revision) const;
  std::size_t merge_remote(AgentRegistry& registry, const std::vector<AgentRecord>& remote_entries,
                            IdentityService* identity = nullptr) const;

 private:
  std::size_t max_entries_per_round_;
};

std::string encode_merge_command(const AgentRecord& record);
std::string encode_sync_pull_command(std::uint64_t after_revision, std::size_t limit);

}  // namespace beacon

