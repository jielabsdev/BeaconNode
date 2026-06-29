#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace beacon {

struct NodeConfig {
  std::string node_id = "dev-node";
  std::string listen_host = "0.0.0.0";
  std::uint16_t listen_port = 43170;
  std::string control_host = "127.0.0.1";
  std::uint16_t control_port = 43171;
};

struct GossipConfig {
  std::chrono::milliseconds interval{2000};
  std::size_t fanout = 3;
  std::size_t max_registry_items_per_round = 256;
};

struct LifecycleConfig {
  std::chrono::milliseconds heartbeat_ttl{30000};
  std::chrono::milliseconds purge_interval{10000};
};

struct TrustConfig {
  std::uint32_t malformed_threshold = 5;
  std::uint32_t rate_limit_per_minute = 120;
  bool require_signatures = false;
};

struct BeaconConfig {
  NodeConfig node;
  GossipConfig gossip;
  LifecycleConfig lifecycle;
  TrustConfig trust;
};

BeaconConfig default_config();

}  // namespace beacon
