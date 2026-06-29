#pragma once

#include "beacon/agent_registry.hpp"
#include "beacon/control_protocol.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace beacon {

std::vector<std::uint8_t> handle_control_frame(
    AgentRegistry& registry,
    const ControlProtocol& protocol,
    const std::vector<std::uint8_t>& frame_bytes);

std::vector<std::uint8_t> handle_control_request_payload(
    AgentRegistry& registry,
    const ControlProtocol& protocol,
    const std::vector<std::uint8_t>& payload);

}  // namespace beacon

