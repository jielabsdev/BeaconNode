#pragma once

#include "beacon/agent_registry.hpp"
#include "beacon/config.hpp"

namespace beacon { class IdentityService; }

#include <boost/asio.hpp>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace beacon::net {

struct GossipPeer {
    std::string node_id;
    std::string host;
    std::uint16_t listen_port;
    std::uint16_t control_port;
};

class GossipService {
public:
    GossipService(NodeConfig config, GossipConfig gossip_config, AgentRegistry& registry,
                  IdentityService* identity = nullptr);
    ~GossipService();

    GossipService(const GossipService&) = delete;
    GossipService& operator=(const GossipService&) = delete;

    void start();
    void stop();

    void add_peer(GossipPeer peer);
    std::vector<GossipPeer> peers() const;

private:
    void run();
    void schedule_round(boost::asio::steady_timer& timer);
    void perform_gossip_round();
    void send_udp_heartbeat(const GossipPeer& peer);
    void sync_with_peer_tcp(const GossipPeer& peer);

    NodeConfig config_;
    GossipConfig gossip_config_;
    AgentRegistry& registry_;
    IdentityService* identity_;
    std::atomic_bool stopping_{false};
    std::thread worker_;
    std::vector<GossipPeer> peers_;
    mutable std::mutex peers_mutex_;
};

} // namespace beacon::net
