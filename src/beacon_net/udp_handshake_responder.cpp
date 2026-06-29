#include "beacon_net/udp_handshake_responder.hpp"

#include "beacon/peer_handshake.hpp"

#include <boost/asio.hpp>

#include <array>
#include <iostream>
#include <utility>

namespace beacon::net {
namespace {

using boost::asio::ip::udp;

}  // namespace

UdpHandshakeResponder::UdpHandshakeResponder(NodeConfig config, AgentRegistry& registry)
    : config_(std::move(config)), registry_(registry) {}

UdpHandshakeResponder::~UdpHandshakeResponder() {
  stop();
}

void UdpHandshakeResponder::start() {
  if (worker_.joinable()) {
    return;
  }

  stopping_ = false;
  worker_ = std::thread([this] {
    run();
  });
}

void UdpHandshakeResponder::stop() {
  stopping_ = true;
  if (worker_.joinable()) {
    try {
      boost::asio::io_context context;
      udp::socket socket(context);
      socket.open(udp::v4());
      const auto endpoint = udp::endpoint(boost::asio::ip::make_address("127.0.0.1"), config_.listen_port);
      socket.send_to(boost::asio::buffer("BEACON_STOP"), endpoint);
    } catch (...) {
    }
    worker_.join();
  }
}

void UdpHandshakeResponder::run() {
  try {
    boost::asio::io_context context;
    udp::socket socket(context, udp::endpoint(boost::asio::ip::udp::v4(), config_.listen_port));
    std::array<char, 1024> data{};

    std::cout << "udp handshake responder listening on " << config_.listen_host << ":" << config_.listen_port << "\n";

    while (!stopping_) {
      udp::endpoint sender;
      const auto length = socket.receive_from(boost::asio::buffer(data), sender);
      if (stopping_) {
        break;
      }

      const std::string payload(data.data(), length);
      const auto hello = decode_peer_hello(payload);
      if (!hello.has_value()) {
        continue;
      }

      const auto stats = registry_.stats();
      const auto response = encode_peer_welcome({config_.node_id, stats.highest_revision});
      socket.send_to(boost::asio::buffer(response), sender);
    }
  } catch (const std::exception& error) {
    if (!stopping_) {
      std::cerr << "udp handshake responder error: " << error.what() << "\n";
    }
  }
}

}  // namespace beacon::net
