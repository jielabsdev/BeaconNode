#include "beacon/agent_registry.hpp"
#include "beacon/config.hpp"
#include "beacon/control_protocol.hpp"
#include "beacon/gossip_scheduler.hpp"
#include "beacon/identity_service.hpp"
#include "beacon/identity_store.hpp"
#include "beacon/peer_handshake.hpp"
#include "beacon/reputation.hpp"

#ifdef BEACONNODE_ENABLE_ASIO_NETWORKING
#include "beacon_net/control_server.hpp"
#include "beacon_net/gossip_service.hpp"
#include "beacon_net/udp_handshake_responder.hpp"
#endif

#include "beacon/crypto.hpp"

#ifdef BEACONNODE_ENABLE_PROTOBUF
#include "beacon_net/persistence_service.hpp"
#include "gossip.pb.h"
#endif

#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

void print_usage() {
  std::cout << "beacon-engine [--config <path>] [--node-id <id>] [--listen-port <port>] "
               "[--control-port <port>] [--seed-node <host:port>] [--require-signatures] [--once]\n";
}

bool parse_port(const std::string& value, std::uint16_t& port) {
  int parsed = 0;
  try {
    parsed = std::stoi(value);
  } catch (const std::exception&) {
    return false;
  }

  if (parsed <= 0 || parsed > 65535) {
    return false;
  }

  port = static_cast<std::uint16_t>(parsed);
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  std::string config_path = "config/beaconnode.example.toml";
  bool run_once = false;
  auto config = beacon::default_config();
  std::vector<std::string> seed_nodes;

  for (int index = 1; index < argc; ++index) {
    const std::string arg = argv[index];
    if (arg == "--help") {
      print_usage();
      return 0;
    }
    if (arg == "--once") {
      run_once = true;
      continue;
    }
    if (arg == "--require-signatures") {
      config.trust.require_signatures = true;
      continue;
    }
    if (arg == "--config" && index + 1 < argc) {
      config_path = argv[++index];
      continue;
    }
    if (arg == "--node-id" && index + 1 < argc) {
      config.node.node_id = argv[++index];
      continue;
    }
    if (arg == "--listen-port" && index + 1 < argc) {
      if (!parse_port(argv[++index], config.node.listen_port)) {
        std::cerr << "invalid listen port\n";
        return 2;
      }
      continue;
    }
    if (arg == "--control-port" && index + 1 < argc) {
      if (!parse_port(argv[++index], config.node.control_port)) {
        std::cerr << "invalid control port\n";
        return 2;
      }
      continue;
    }
    if (arg == "--seed-node" && index + 1 < argc) {
      seed_nodes.push_back(argv[++index]);
      continue;
    }

    std::cerr << "unknown argument: " << arg << "\n";
    print_usage();
    return 2;
  }

  beacon::ensure_sodium_initialized();

  beacon::AgentRegistry registry;

#ifdef BEACONNODE_ENABLE_PROTOBUF
  beaconnode::PersistenceService persistence({});
  {
    auto recovered = persistence.recover();
    if (!recovered.empty()) {
      std::vector<beacon::AgentRecord> restored;
      restored.reserve(recovered.size());
      for (auto& entry : recovered) {
        beacon::AgentRecord record;
        record.agent_id = entry.agent_id();
        record.node_id = entry.capability();
        record.endpoint = entry.endpoint();
        record.revision = entry.timestamp();
        {
          const auto& pk = entry.public_key();
          record.public_key_hex = beacon::encode_hex(
              {reinterpret_cast<const std::uint8_t*>(pk.data()),
               reinterpret_cast<const std::uint8_t*>(pk.data()) + pk.size()});
        }
        {
          const auto& sig = entry.signature();
          record.signature_hex = beacon::encode_hex(
              {reinterpret_cast<const std::uint8_t*>(sig.data()),
               reinterpret_cast<const std::uint8_t*>(sig.data()) + sig.size()});
        }
        restored.push_back(std::move(record));
      }
      registry.load_from_persistence(restored);
      std::cout << "[Persistence] Recovered " << restored.size() << " entries from disk.\n";
    }
  }

  registry.set_upsert_callback([&persistence](const beacon::AgentRecord& record) {
    beaconnode::v1::RegistryEntry entry;
    entry.set_agent_id(record.agent_id);
    entry.set_capability(record.node_id);
    entry.set_endpoint(record.endpoint);
    entry.set_timestamp(record.revision);
    {
      auto raw = beacon::decode_hex(record.public_key_hex);
      entry.set_public_key({raw.begin(), raw.end()});
    }
    {
      auto raw = beacon::decode_hex(record.signature_hex);
      entry.set_signature({raw.begin(), raw.end()});
    }
    persistence.append_entry(entry);
  });
#endif

  beacon::IdentityService identity_service;
  beacon::IdentityStore identity_store(".beaconnode/identity.txt");
  const auto identity = identity_store.load_or_create(config.node.node_id);
  beacon::ControlProtocol control(config.lifecycle, config.trust, &identity_service);
  beacon::GossipScheduler gossip(config.gossip);
  beacon::ReputationEngine reputation(config.trust);
  const auto hello = beacon::encode_peer_hello({config.node.node_id, config.node.listen_port});

#ifdef BEACONNODE_ENABLE_ASIO_NETWORKING
  beacon::net::ControlServer control_server(config.node, registry, control);
  beacon::net::UdpHandshakeResponder handshake_responder(config.node, registry);
  beacon::net::GossipService gossip_service(config.node, config.gossip, registry, &identity_service);

  for (const auto& seed : seed_nodes) {
    const auto colon = seed.find(':');
    if (colon == std::string::npos) {
      gossip_service.add_peer({"", seed, config.node.listen_port, config.node.control_port});
    } else {
      const auto host = seed.substr(0, colon);
      auto port = config.node.control_port;
      try { port = static_cast<std::uint16_t>(std::stoi(seed.substr(colon + 1))); } catch (...) {}
      gossip_service.add_peer({"", host, static_cast<std::uint16_t>(port > 0 ? port - 1 : 43170), port});
    }
  }
#endif

  std::cout << "BeaconNode engine starting\n";
  std::cout << "config: " << config_path << "\n";
  std::cout << "node: " << config.node.node_id << "\n";
  std::cout << "identity_public_key: " << identity.public_key_hex << "\n";
  std::cout << "control: " << config.node.control_host << ":" << config.node.control_port << "\n";
  std::cout << "p2p: " << config.node.listen_host << ":" << config.node.listen_port << "\n";
  std::cout << "udp_hello: " << hello << "\n";
  std::cout << "trusted_agents: " << identity_service.trusted_agents() << "\n";
  std::cout << "require_signatures: " << (config.trust.require_signatures ? "yes" : "no") << "\n";

#ifdef BEACONNODE_ENABLE_ASIO_NETWORKING
  if (!run_once) {
    control_server.start();
    handshake_responder.start();
    gossip_service.start();
  }
#endif

  do {
    const auto purged = registry.purge_expired(beacon::Clock::now());
    const auto stats = registry.stats();
    const auto push_entries = gossip.select_push_entries(registry.list());
    const auto stats_line = control.handle_line(registry, "STATS");

    std::cout << "tick active_agents=" << stats.active_agents
              << " highest_revision=" << stats.highest_revision
              << " gossip_push_entries=" << push_entries.size()
              << " purged=" << purged
              << " accepts_self=" << reputation.accepts_peer(config.node.node_id)
              << " trusted_agents=" << identity_service.trusted_agents()
              << "\n";
    std::cout << "control_stats: " << stats_line;

    if (run_once) {
      break;
    }

    std::this_thread::sleep_for(config.lifecycle.purge_interval);
  } while (true);

  return 0;
}
