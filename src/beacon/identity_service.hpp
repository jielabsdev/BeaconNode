#pragma once

#include "beacon/agent_registry.hpp"
#include "beacon/crypto.hpp"

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace beacon {

class IdentityService {
 public:
  IdentityService() = default;

  IdentityService(const IdentityService&) = delete;
  IdentityService& operator=(const IdentityService&) = delete;

  SignatureStatus verify_and_register(const AgentRecord& record);
  bool check_key_consistency(const std::string& agent_id,
                             const SecureBuffer& public_key) const;
  void forget_key(const std::string& agent_id);

  std::size_t trusted_agents() const;

 private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, SecureBuffer> trust_store_;
};

}  // namespace beacon
