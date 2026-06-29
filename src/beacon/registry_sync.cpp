#include "beacon/registry_sync.hpp"
#include "beacon/identity_service.hpp"

#include "beacon/control_protocol.hpp"

namespace beacon {

RegistrySync::RegistrySync(std::size_t max_entries_per_round) : max_entries_per_round_(max_entries_per_round) {}

SyncPlan RegistrySync::plan_push_pull(const AgentRegistry& registry, std::uint64_t remote_revision) const {
  SyncPlan plan;
  plan.local_revision = registry.stats().highest_revision;
  plan.push_entries = registry.entries_after_revision(remote_revision, max_entries_per_round_);
  return plan;
}

std::size_t RegistrySync::merge_remote(AgentRegistry& registry, const std::vector<AgentRecord>& remote_entries,
                                        IdentityService* identity) const {
  return registry.merge(remote_entries, identity);
}

std::string encode_merge_command(const AgentRecord& record) {
  auto command = encode_agent_line(record);
  command.replace(0, 5, "MERGE");
  if (!command.empty() && command.back() == '\n') {
    command.pop_back();
  }
  return command;
}

std::string encode_sync_pull_command(std::uint64_t after_revision, std::size_t limit) {
  return "SYNC_PULL after_revision=" + std::to_string(after_revision) + " limit=" + std::to_string(limit);
}

}  // namespace beacon
