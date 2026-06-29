#include "beacon_net/gossip_service.hpp"
#include "beacon/identity_service.hpp"
#include "beacon_net/sync_client.hpp"

#include <boost/asio.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <random>
#include <utility>

namespace beacon::net {
namespace {

using boost::asio::ip::tcp;
using boost::asio::ip::udp;

constexpr auto GOSSIP_INTERVAL = std::chrono::seconds(2);
constexpr std::size_t GOSSIP_FANOUT = 3;

std::vector<std::size_t> pick_random_indices(std::size_t count, std::size_t fanout) {
    if (count == 0) return {};
    std::vector<std::size_t> indices(count);
    for (std::size_t i = 0; i < count; ++i) indices[i] = i;

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(indices.begin(), indices.end(), g);

    const auto n = std::min(fanout, count);
    indices.resize(n);
    return indices;
}

} // namespace

GossipService::GossipService(NodeConfig config, GossipConfig gossip_config, AgentRegistry& registry,
                              IdentityService* identity)
    : config_(std::move(config)), gossip_config_(std::move(gossip_config)), registry_(registry), identity_(identity) {}

GossipService::~GossipService() {
    stop();
}

void GossipService::start() {
    if (worker_.joinable()) return;
    stopping_ = false;
    worker_ = std::thread([this] { run(); });
}

void GossipService::stop() {
    stopping_ = true;
    if (worker_.joinable()) {
        worker_.join();
    }
}

void GossipService::add_peer(GossipPeer peer) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    for (const auto& existing : peers_) {
        if (existing.host == peer.host && existing.control_port == peer.control_port) {
            return;
        }
    }
    peers_.push_back(std::move(peer));
}

std::vector<GossipPeer> GossipService::peers() const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    return peers_;
}

void GossipService::run() {
    try {
        boost::asio::io_context context;
        boost::asio::steady_timer timer(context);
        schedule_round(timer);
        context.run();
    } catch (const std::exception& error) {
        if (!stopping_) {
            std::cerr << "gossip service error: " << error.what() << "\n";
        }
    }
}

void GossipService::schedule_round(boost::asio::steady_timer& timer) {
    if (stopping_) return;
    timer.expires_after(GOSSIP_INTERVAL);
    timer.async_wait([this, &timer](const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted || stopping_) return;
        if (!ec) {
            perform_gossip_round();
            schedule_round(timer);
        }
    });
}

void GossipService::perform_gossip_round() {
    auto current_peers = peers();
    if (current_peers.empty()) {
        return;
    }

    const auto indices = pick_random_indices(current_peers.size(), GOSSIP_FANOUT);
    for (const auto idx : indices) {
        const auto& peer = current_peers[idx];
        send_udp_heartbeat(peer);
        sync_with_peer_tcp(peer);
    }
}

void GossipService::send_udp_heartbeat(const GossipPeer& peer) {
    try {
        boost::asio::io_context context;
        udp::socket socket(context);
        socket.open(udp::v4());

        const std::string heartbeat =
            "GOSSIP_HEARTBEAT node_id=" + config_.node_id +
            " listen_port=" + std::to_string(config_.listen_port) +
            " control_port=" + std::to_string(config_.control_port);

        const auto endpoint = udp::endpoint(
            boost::asio::ip::make_address(peer.host), peer.listen_port);
        socket.send_to(boost::asio::buffer(heartbeat), endpoint);
    } catch (const std::exception& error) {
        if (!stopping_) {
            std::cerr << "gossip heartbeat to " << peer.host << ":" << peer.listen_port
                      << " failed: " << error.what() << "\n";
        }
    }
}

void GossipService::sync_with_peer_tcp(const GossipPeer& peer) {
    try {
        const SyncClient sync(gossip_config_.max_registry_items_per_round, identity_);
        const SyncPeer sync_peer{peer.host, peer.control_port};
        const auto result = sync.sync_with_peer(registry_, sync_peer);

        if (result.pulled > 0 || result.pushed > 0) {
            std::cout << "gossip sync with " << peer.host << ":" << peer.control_port
                      << " pulled=" << result.pulled
                      << " pushed=" << result.pushed << "\n";
        }
    } catch (const std::exception& error) {
        if (!stopping_) {
            std::cerr << "gossip tcp sync to " << peer.host << ":" << peer.control_port
                      << " failed: " << error.what() << "\n";
        }
    }
}

} // namespace beacon::net
