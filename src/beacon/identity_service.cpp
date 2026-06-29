#include "beacon/identity_service.hpp"

#include <algorithm>
#include <utility>

namespace beacon {

SignatureStatus IdentityService::verify_and_register(const AgentRecord& record) {
  const auto status = verify_agent_signature(record);
  if (status != SignatureStatus::Valid) {
    return status;
  }

  const auto public_key_bytes = decode_hex(record.public_key_hex);
  if (public_key_bytes.empty()) {
    return SignatureStatus::Invalid;
  }

  SecureBuffer public_key(public_key_bytes.size());
  std::copy(public_key_bytes.begin(), public_key_bytes.end(), public_key.data());

  std::lock_guard lock(mutex_);
  const auto existing = trust_store_.find(record.agent_id);
  if (existing == trust_store_.end()) {
    trust_store_.emplace(record.agent_id, std::move(public_key));
    return SignatureStatus::Valid;
  }

  if (!existing->second.constant_time_equals(public_key)) {
    return SignatureStatus::Invalid;
  }

  return SignatureStatus::Valid;
}

bool IdentityService::check_key_consistency(const std::string& agent_id,
                                            const SecureBuffer& public_key) const {
  std::lock_guard lock(mutex_);
  const auto existing = trust_store_.find(agent_id);
  if (existing == trust_store_.end()) {
    return true;
  }
  return existing->second.constant_time_equals(public_key);
}

void IdentityService::forget_key(const std::string& agent_id) {
  std::lock_guard lock(mutex_);
  trust_store_.erase(agent_id);
}

std::size_t IdentityService::trusted_agents() const {
  std::lock_guard lock(mutex_);
  return trust_store_.size();
}

}  // namespace beacon
