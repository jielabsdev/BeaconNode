#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace beacon {

enum class FrameKind : std::uint16_t {
  ControlRequest = 1,
  ControlResponse = 2,
  GossipEnvelope = 3,
};

struct Frame {
  FrameKind kind = FrameKind::ControlRequest;
  std::vector<std::uint8_t> payload;
};

std::vector<std::uint8_t> encode_frame(const Frame& frame);
std::optional<Frame> decode_frame(const std::vector<std::uint8_t>& bytes);
std::string describe_frame_kind(FrameKind kind);

}  // namespace beacon

