#pragma once

#include "beacon/config.hpp"

#include <cstdint>
#include <map>
#include <string>

namespace beacon {

class ReputationEngine {
 public:
  explicit ReputationEngine(TrustConfig config);

  void record_malformed(const std::string& peer_id);
  bool accepts_peer(const std::string& peer_id) const;

 private:
  TrustConfig config_;
  std::map<std::string, std::uint32_t> malformed_counts_;
};

}  // namespace beacon

