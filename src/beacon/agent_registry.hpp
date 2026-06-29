#pragma once

#include "beacon/types.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace beacon {

class IdentityService;

class AgentRegistry {
  using UpsertCallback = std::function<void(const AgentRecord&)>;

 public:
  bool upsert(AgentRecord record);
  bool refresh_heartbeat(const std::string& agent_id, TimePoint expires_at, std::uint64_t revision);
  std::size_t merge(const std::vector<AgentRecord>& records, IdentityService* identity = nullptr);
  std::vector<AgentRecord> entries_after_revision(std::uint64_t revision, std::size_t limit) const;
  std::optional<AgentRecord> find(const std::string& agent_id) const;
  std::vector<AgentRecord> list() const;
  std::size_t purge_expired(TimePoint now);
  void load_from_persistence(const std::vector<AgentRecord>& records);
  RegistryStats stats() const;

  void set_upsert_callback(UpsertCallback cb);

 private:
  mutable std::mutex mutex_;
  std::vector<AgentRecord> records_;
  UpsertCallback on_upsert_;
};

}  // namespace beacon
