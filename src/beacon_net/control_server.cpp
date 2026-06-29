#include "beacon_net/control_server.hpp"

#include "beacon/framing.hpp"
#ifdef BEACONNODE_ENABLE_PROTOBUF
#include "beacon/control_protobuf.hpp"
#endif

#include <boost/asio.hpp>

#include <array>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <utility>
#include <vector>

namespace beacon::net {
namespace {

using boost::asio::ip::tcp;

bool is_binary_frame_header(const std::array<unsigned char, 9>& header) {
  return header[0] == static_cast<unsigned char>('B') && header[1] == static_cast<unsigned char>('N');
}

void serve_session(tcp::socket socket, AgentRegistry& registry, ControlProtocol& protocol) {
  try {
    std::array<unsigned char, 9> header{};
    const auto read = socket.receive(boost::asio::buffer(header), boost::asio::socket_base::message_peek);
    if (read >= header.size() && is_binary_frame_header(header)) {
#ifdef BEACONNODE_ENABLE_PROTOBUF
      std::vector<std::uint8_t> frame_bytes(header.size());
      boost::asio::read(socket, boost::asio::buffer(frame_bytes));
      const auto payload_length =
          (static_cast<std::uint32_t>(header[5]) << 24) |
          (static_cast<std::uint32_t>(header[6]) << 16) |
          (static_cast<std::uint32_t>(header[7]) << 8) |
          static_cast<std::uint32_t>(header[8]);
      frame_bytes.resize(header.size() + payload_length);
      if (payload_length > 0) {
        boost::asio::read(socket, boost::asio::buffer(frame_bytes.data() + header.size(), payload_length));
      }
      const auto response = handle_control_frame(registry, protocol, frame_bytes);
      boost::asio::write(socket, boost::asio::buffer(response));
      return;
#else
      const std::string response = "ERR message=protobuf_control_disabled\n";
      boost::asio::write(socket, boost::asio::buffer(response));
      return;
#endif
    }

    boost::asio::streambuf buffer;
    boost::asio::read_until(socket, buffer, '\n');

    std::istream input(&buffer);
    std::string line;
    std::getline(input, line);

    const auto response = protocol.handle_line(registry, line);
    boost::asio::write(socket, boost::asio::buffer(response));
  } catch (const std::exception& error) {
    std::cerr << "control session error: " << error.what() << "\n";
  }
}

}  // namespace

ControlServer::ControlServer(NodeConfig config, AgentRegistry& registry, ControlProtocol& protocol)
    : config_(std::move(config)), registry_(registry), protocol_(protocol) {}

ControlServer::~ControlServer() {
  stop();
}

void ControlServer::start() {
  if (worker_.joinable()) {
    return;
  }

  stopping_ = false;
  worker_ = std::thread([this] {
    run();
  });
}

void ControlServer::stop() {
  stopping_ = true;
  if (worker_.joinable()) {
    try {
      boost::asio::io_context context;
      tcp::socket socket(context);
      socket.connect(tcp::endpoint(boost::asio::ip::make_address(config_.control_host), config_.control_port));
    } catch (...) {
    }
    worker_.join();
  }
}

void ControlServer::run() {
  try {
    boost::asio::io_context context;
    const auto address = boost::asio::ip::make_address(config_.control_host);
    tcp::acceptor acceptor(context, tcp::endpoint(address, config_.control_port));

    std::cout << "control server listening on " << config_.control_host << ":" << config_.control_port << "\n";

    while (!stopping_) {
      tcp::socket socket(context);
      acceptor.accept(socket);
      if (!stopping_) {
        serve_session(std::move(socket), registry_, protocol_);
      }
    }
  } catch (const std::exception& error) {
    if (!stopping_) {
      std::cerr << "control server error: " << error.what() << "\n";
    }
  }
}

}  // namespace beacon::net
