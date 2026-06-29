#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <string>

namespace beacon {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

struct AgentRecord {
  std::string agent_id;
  std::string node_id;
  std::string endpoint;
  std::map<std::string, std::string> metadata;
  std::uint64_t revision = 0;
  TimePoint expires_at = Clock::now();
  std::string public_key_hex;
  std::string signature_hex;
};

struct RegistryStats {
  std::size_t active_agents = 0;
  std::uint64_t highest_revision = 0;
};

}  // namespace beacon
