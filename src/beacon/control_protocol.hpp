#pragma once

#include "beacon/agent_registry.hpp"
#include "beacon/config.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace beacon {

class IdentityService;

enum class ControlAction {
  Register,
  Heartbeat,
  List,
  Stats,
  Merge,
  SyncPull,
  Unknown,
};

struct ControlCommand {
  ControlAction action = ControlAction::Unknown;
  AgentRecord registration;
  std::string agent_id;
  std::string error;
  std::uint64_t after_revision = 0;
  std::size_t limit = 256;
};

class ControlProtocol {
 public:
  ControlProtocol(LifecycleConfig lifecycle, TrustConfig trust = {}, IdentityService* identity = nullptr);

  ControlCommand parse_command(const std::string& line) const;
  std::string handle_line(AgentRegistry& registry, const std::string& line);
  LifecycleConfig lifecycle() const;
  TrustConfig trust() const;
  IdentityService* identity_service() const;

 private:
  LifecycleConfig lifecycle_;
  TrustConfig trust_;
  IdentityService* identity_;
};

std::string encode_agent_line(const AgentRecord& record);

}  // namespace beacon
