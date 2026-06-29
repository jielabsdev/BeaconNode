#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace beacon {

struct PeerHello {
  std::string node_id;
  std::uint16_t listen_port = 0;
};

struct PeerWelcome {
  std::string node_id;
  std::uint64_t registry_revision = 0;
};

std::string encode_peer_hello(const PeerHello& hello);
std::optional<PeerHello> decode_peer_hello(const std::string& payload);
std::string encode_peer_welcome(const PeerWelcome& welcome);
std::optional<PeerWelcome> decode_peer_welcome(const std::string& payload);

}  // namespace beacon

