#include "beacon/peer_handshake.hpp"

#include <charconv>
#include <sstream>
#include <string_view>
#include <vector>

namespace beacon {
namespace {

std::vector<std::string> split_tokens(const std::string& payload) {
  std::istringstream stream(payload);
  std::vector<std::string> tokens;
  std::string token;

  while (stream >> token) {
    tokens.push_back(token);
  }

  return tokens;
}

bool parse_u16(std::string_view value, std::uint16_t& output) {
  std::uint32_t parsed = 0;
  const auto* begin = value.data();
  const auto* end = value.data() + value.size();
  const auto result = std::from_chars(begin, end, parsed);
  if (result.ec != std::errc{} || result.ptr != end || parsed > 65535) {
    return false;
  }

  output = static_cast<std::uint16_t>(parsed);
  return true;
}

bool parse_u64(std::string_view value, std::uint64_t& output) {
  const auto* begin = value.data();
  const auto* end = value.data() + value.size();
  const auto result = std::from_chars(begin, end, output);
  return result.ec == std::errc{} && result.ptr == end;
}

std::string value_after(const std::string& token) {
  const auto equals = token.find('=');
  if (equals == std::string::npos) {
    return {};
  }

  return token.substr(equals + 1);
}

}  // namespace

std::string encode_peer_hello(const PeerHello& hello) {
  return "BEACON_HELLO node_id=" + hello.node_id + " listen_port=" + std::to_string(hello.listen_port);
}

std::optional<PeerHello> decode_peer_hello(const std::string& payload) {
  const auto tokens = split_tokens(payload);
  if (tokens.empty() || tokens[0] != "BEACON_HELLO") {
    return std::nullopt;
  }

  PeerHello hello;
  for (std::size_t index = 1; index < tokens.size(); ++index) {
    if (tokens[index].rfind("node_id=", 0) == 0) {
      hello.node_id = value_after(tokens[index]);
    } else if (tokens[index].rfind("listen_port=", 0) == 0 && !parse_u16(value_after(tokens[index]), hello.listen_port)) {
      return std::nullopt;
    }
  }

  if (hello.node_id.empty() || hello.listen_port == 0) {
    return std::nullopt;
  }

  return hello;
}

std::string encode_peer_welcome(const PeerWelcome& welcome) {
  return "BEACON_WELCOME node_id=" + welcome.node_id +
         " registry_revision=" + std::to_string(welcome.registry_revision);
}

std::optional<PeerWelcome> decode_peer_welcome(const std::string& payload) {
  const auto tokens = split_tokens(payload);
  if (tokens.empty() || tokens[0] != "BEACON_WELCOME") {
    return std::nullopt;
  }

  PeerWelcome welcome;
  for (std::size_t index = 1; index < tokens.size(); ++index) {
    if (tokens[index].rfind("node_id=", 0) == 0) {
      welcome.node_id = value_after(tokens[index]);
    } else if (tokens[index].rfind("registry_revision=", 0) == 0 &&
               !parse_u64(value_after(tokens[index]), welcome.registry_revision)) {
      return std::nullopt;
    }
  }

  if (welcome.node_id.empty()) {
    return std::nullopt;
  }

  return welcome;
}

}  // namespace beacon
