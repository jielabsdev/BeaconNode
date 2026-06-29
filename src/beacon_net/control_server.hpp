#pragma once

#include "beacon/agent_registry.hpp"
#include "beacon/config.hpp"
#include "beacon/control_protocol.hpp"

#include <atomic>
#include <memory>
#include <thread>

namespace beacon::net {

class ControlServer {
 public:
  ControlServer(NodeConfig config, AgentRegistry& registry, ControlProtocol& protocol);
  ~ControlServer();

  ControlServer(const ControlServer&) = delete;
  ControlServer& operator=(const ControlServer&) = delete;

  void start();
  void stop();

 private:
  void run();

  NodeConfig config_;
  AgentRegistry& registry_;
  ControlProtocol& protocol_;
  std::atomic_bool stopping_{false};
  std::thread worker_;
};

}  // namespace beacon::net

