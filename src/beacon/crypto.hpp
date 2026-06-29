#pragma once

#include "beacon/types.hpp"
#include "beacon_net/secure_buffer.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace beacon {

enum class SignatureStatus {
  Valid,
  Missing,
  Invalid,
  Unsupported,
};

struct Ed25519Keypair {
  SecureBuffer public_key;
  SecureBuffer secret_key;
};

std::string canonical_agent_payload(const AgentRecord& record);
std::string canonical_entry_payload(const std::string& agent_id,
                                     const std::string& capability,
                                     const std::string& endpoint,
                                     std::uint64_t timestamp);
std::vector<std::uint8_t> decode_hex(const std::string& hex);
std::string encode_hex(const std::vector<std::uint8_t>& bytes);
std::string encode_hex(const std::uint8_t* data, std::size_t size);
inline std::string encode_hex(const SecureBuffer& buf) {
  return encode_hex(buf.data(), buf.size());
}
SignatureStatus verify_agent_signature(const AgentRecord& record);
std::string signature_status_message(SignatureStatus status);

Ed25519Keypair generate_ed25519_keypair();
SecureBuffer sign_ed25519(const SecureBuffer& payload,
                          const SecureBuffer& private_key);
bool verify_ed25519(const SecureBuffer& payload,
                    const SecureBuffer& signature,
                    const SecureBuffer& public_key);

bool ensure_sodium_initialized();

}  // namespace beacon
