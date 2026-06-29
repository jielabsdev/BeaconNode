#pragma once

#include "beacon/agent_registry.hpp"
#include "beacon/config.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace beacon { class IdentityService; }

namespace beacon::net {

struct SyncPeer {
  std::string host = "127.0.0.1";
  std::uint16_t control_port = 43171;
};

struct SyncResult {
  std::size_t pulled = 0;
  std::size_t pushed = 0;
};

class SyncClient {
 public:
  explicit SyncClient(std::size_t max_entries_per_round, IdentityService* identity = nullptr);

  SyncResult sync_with_peer(AgentRegistry& local_registry, const SyncPeer& peer) const;

 private:
  std::string send_command(const SyncPeer& peer, const std::string& command) const;
  std::size_t merge_response(AgentRegistry& registry, const std::string& response) const;
  std::size_t push_entries(const SyncPeer& peer, const std::vector<AgentRecord>& entries) const;

  std::size_t max_entries_per_round_;
  IdentityService* identity_;
};

}  // namespace beacon::net
