#include "beacon/crypto.hpp"

#ifdef BEACONNODE_ENABLE_LIBSODIUM
#include <sodium.h>
#endif

#include <iomanip>
#include <sstream>

namespace beacon {
namespace {

int hex_value(char value) {
  if (value >= '0' && value <= '9') {
    return value - '0';
  }
  if (value >= 'a' && value <= 'f') {
    return 10 + value - 'a';
  }
  if (value >= 'A' && value <= 'F') {
    return 10 + value - 'A';
  }
  return -1;
}

}  // namespace

std::string canonical_agent_payload(const AgentRecord& record) {
  std::ostringstream output;
  output << "agent_id=" << record.agent_id << "\n";
  output << "node_id=" << record.node_id << "\n";
  output << "endpoint=" << record.endpoint << "\n";
  output << "revision=" << record.revision << "\n";
  for (const auto& [key, value] : record.metadata) {
    output << "metadata." << key << "=" << value << "\n";
  }
  return output.str();
}

std::string canonical_entry_payload(const std::string& agent_id,
                                     const std::string& capability,
                                     const std::string& endpoint,
                                     std::uint64_t timestamp) {
  std::ostringstream output;
  output << "agent_id=" << agent_id << "\n";
  output << "capability=" << capability << "\n";
  output << "endpoint=" << endpoint << "\n";
  output << "timestamp=" << timestamp << "\n";
  return output.str();
}

std::vector<std::uint8_t> decode_hex(const std::string& hex) {
  if (hex.size() % 2 != 0) {
    return {};
  }

  std::vector<std::uint8_t> bytes;
  bytes.reserve(hex.size() / 2);
  for (std::size_t index = 0; index < hex.size(); index += 2) {
    const auto high = hex_value(hex[index]);
    const auto low = hex_value(hex[index + 1]);
    if (high < 0 || low < 0) {
      return {};
    }
    bytes.push_back(static_cast<std::uint8_t>((high << 4) | low));
  }
  return bytes;
}

std::string encode_hex(const std::vector<std::uint8_t>& bytes) {
  return encode_hex(bytes.data(), bytes.size());
}

std::string encode_hex(const std::uint8_t* data, std::size_t size) {
  std::ostringstream output;
  for (std::size_t i = 0; i < size; ++i) {
    output << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(data[i]);
  }
  return output.str();
}

bool ensure_sodium_initialized() {
#ifdef BEACONNODE_ENABLE_LIBSODIUM
  if (sodium_init() < 0) return false;
#endif
  return true;
}

SignatureStatus verify_agent_signature(const AgentRecord& record) {
  if (record.public_key_hex.empty() || record.signature_hex.empty()) {
    return SignatureStatus::Missing;
  }

  const auto public_key = decode_hex(record.public_key_hex);
  const auto signature = decode_hex(record.signature_hex);
  if (public_key.empty() || signature.empty()) {
    return SignatureStatus::Invalid;
  }

#ifdef BEACONNODE_ENABLE_LIBSODIUM
  if (public_key.size() != crypto_sign_PUBLICKEYBYTES ||
      signature.size() != crypto_sign_BYTES) {
    return SignatureStatus::Invalid;
  }

  const auto payload = canonical_agent_payload(record);
  const auto result = crypto_sign_verify_detached(
      signature.data(),
      reinterpret_cast<const unsigned char*>(payload.data()),
      static_cast<unsigned long long>(payload.size()),
      public_key.data());
  return result == 0 ? SignatureStatus::Valid : SignatureStatus::Invalid;
#else
  (void)public_key;
  (void)signature;
  return SignatureStatus::Unsupported;
#endif
}

std::string signature_status_message(SignatureStatus status) {
  switch (status) {
    case SignatureStatus::Valid:
      return "valid";
    case SignatureStatus::Missing:
      return "missing_signature";
    case SignatureStatus::Invalid:
      return "invalid_signature";
    case SignatureStatus::Unsupported:
      return "signature_verification_unsupported";
  }
  return "unknown_signature_status";
}

Ed25519Keypair generate_ed25519_keypair() {
  Ed25519Keypair result;
#ifdef BEACONNODE_ENABLE_LIBSODIUM
  result.public_key = SecureBuffer(crypto_sign_PUBLICKEYBYTES);
  result.secret_key = SecureBuffer(crypto_sign_SECRETKEYBYTES);
  crypto_sign_keypair(result.public_key.data(), result.secret_key.data());
#endif
  return result;
}

SecureBuffer sign_ed25519(const SecureBuffer& payload,
                          const SecureBuffer& private_key) {
#ifdef BEACONNODE_ENABLE_LIBSODIUM
  if (private_key.size() != crypto_sign_SECRETKEYBYTES) {
    return SecureBuffer{};
  }
  SecureBuffer signature(crypto_sign_BYTES);
  unsigned long long sig_size = 0;
  crypto_sign_detached(signature.data(), &sig_size,
                       payload.data(),
                       static_cast<unsigned long long>(payload.size()),
                       private_key.data());
  signature.resize(sig_size);
  return signature;
#else
  (void)payload;
  (void)private_key;
  return SecureBuffer{};
#endif
}

bool verify_ed25519(const SecureBuffer& payload,
                    const SecureBuffer& signature,
                    const SecureBuffer& public_key) {
#ifdef BEACONNODE_ENABLE_LIBSODIUM
  if (public_key.size() != crypto_sign_PUBLICKEYBYTES ||
      signature.size() != crypto_sign_BYTES) {
    return false;
  }
  const auto result = crypto_sign_verify_detached(
      signature.data(),
      payload.data(),
      static_cast<unsigned long long>(payload.size()),
      public_key.data());
  return result == 0;
#else
  (void)payload;
  (void)signature;
  (void)public_key;
  return false;
#endif
}

}  // namespace beacon
