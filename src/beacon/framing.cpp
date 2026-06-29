#include "beacon/framing.hpp"

#include <cstddef>

namespace beacon {
namespace {

constexpr std::uint8_t kMagic0 = 'B';
constexpr std::uint8_t kMagic1 = 'N';
constexpr std::uint8_t kVersion = 1;
constexpr std::size_t kHeaderSize = 9;

void append_u16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
  bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
  bytes.push_back(static_cast<std::uint8_t>(value & 0xff));
}

void append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
  bytes.push_back(static_cast<std::uint8_t>((value >> 24) & 0xff));
  bytes.push_back(static_cast<std::uint8_t>((value >> 16) & 0xff));
  bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
  bytes.push_back(static_cast<std::uint8_t>(value & 0xff));
}

std::uint16_t read_u16(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
  return static_cast<std::uint16_t>((bytes[offset] << 8) | bytes[offset + 1]);
}

std::uint32_t read_u32(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
  return (static_cast<std::uint32_t>(bytes[offset]) << 24) |
         (static_cast<std::uint32_t>(bytes[offset + 1]) << 16) |
         (static_cast<std::uint32_t>(bytes[offset + 2]) << 8) |
         static_cast<std::uint32_t>(bytes[offset + 3]);
}

}  // namespace

std::vector<std::uint8_t> encode_frame(const Frame& frame) {
  std::vector<std::uint8_t> bytes;
  bytes.reserve(kHeaderSize + frame.payload.size());
  bytes.push_back(kMagic0);
  bytes.push_back(kMagic1);
  bytes.push_back(kVersion);
  append_u16(bytes, static_cast<std::uint16_t>(frame.kind));
  append_u32(bytes, static_cast<std::uint32_t>(frame.payload.size()));
  bytes.insert(bytes.end(), frame.payload.begin(), frame.payload.end());
  return bytes;
}

std::optional<Frame> decode_frame(const std::vector<std::uint8_t>& bytes) {
  if (bytes.size() < kHeaderSize || bytes[0] != kMagic0 || bytes[1] != kMagic1 || bytes[2] != kVersion) {
    return std::nullopt;
  }

  const auto payload_length = read_u32(bytes, 5);
  if (bytes.size() != kHeaderSize + payload_length) {
    return std::nullopt;
  }

  Frame frame;
  frame.kind = static_cast<FrameKind>(read_u16(bytes, 3));
  frame.payload.assign(bytes.begin() + static_cast<std::ptrdiff_t>(kHeaderSize), bytes.end());
  return frame;
}

std::string describe_frame_kind(FrameKind kind) {
  switch (kind) {
    case FrameKind::ControlRequest:
      return "control_request";
    case FrameKind::ControlResponse:
      return "control_response";
    case FrameKind::GossipEnvelope:
      return "gossip_envelope";
  }

  return "unknown";
}

}  // namespace beacon
