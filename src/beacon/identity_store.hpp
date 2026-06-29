#pragma once

#include <filesystem>
#include <string>

namespace beacon {

struct NodeIdentity {
  std::string node_id;
  std::string public_key_hex;
  std::string private_key_path;
};

class IdentityStore {
 public:
  explicit IdentityStore(std::filesystem::path path);

  NodeIdentity load_or_create(const std::string& node_id) const;

 private:
  std::filesystem::path path_;
};

}  // namespace beacon

