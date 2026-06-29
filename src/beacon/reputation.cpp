#include "beacon/reputation.hpp"

namespace beacon {

ReputationEngine::ReputationEngine(TrustConfig config) : config_(config) {}

void ReputationEngine::record_malformed(const std::string& peer_id) {
  malformed_counts_[peer_id] += 1;
}

bool ReputationEngine::accepts_peer(const std::string& peer_id) const {
  const auto existing = malformed_counts_.find(peer_id);
  if (existing == malformed_counts_.end()) {
    return true;
  }

  return existing->second < config_.malformed_threshold;
}

}  // namespace beacon

