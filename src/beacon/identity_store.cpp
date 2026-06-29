#include "beacon/identity_store.hpp"
#include "beacon/crypto.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

#ifdef _WIN32
#include <aclapi.h>
#include <windows.h>
#endif

namespace beacon {
namespace {

constexpr std::streamsize EXPECTED_KEY_SIZE = 64;

std::string read_value(const std::string& content, const std::string& key) {
  std::istringstream lines(content);
  std::string line;
  while (std::getline(lines, line)) {
    const auto equals = line.find('=');
    if (equals != std::string::npos && line.substr(0, equals) == key) {
      return line.substr(equals + 1);
    }
  }
  return {};
}

std::filesystem::path resolve_secure_path(const std::filesystem::path& base,
                                           const std::filesystem::path& user_path) {
  std::error_code ec;
  auto base_abs = std::filesystem::weakly_canonical(std::filesystem::absolute(base), ec);
  if (ec) {
    throw std::runtime_error("Cannot resolve base path: " + base.string() + " (" + ec.message() + ")");
  }
  auto resolved = std::filesystem::weakly_canonical(base_abs / user_path, ec);
  if (ec) {
    throw std::runtime_error("Cannot resolve path: " + user_path.string() + " (" + ec.message() + ")");
  }
  auto base_str = base_abs.string();
  auto res_str = resolved.string();
  if (res_str.size() < base_str.size() ||
      res_str.compare(0, base_str.size(), base_str) != 0 ||
      (res_str.size() > base_str.size() &&
       res_str[base_str.size()] != '/' && res_str[base_str.size()] != '\\')) {
    throw std::runtime_error("Path traversal detected: " + res_str);
  }
  return resolved;
}

void restrict_file_permissions(const std::filesystem::path& path) {
#ifdef _WIN32
  HANDLE hToken = nullptr;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) return;

  DWORD dwSize = 0;
  GetTokenInformation(hToken, TokenUser, nullptr, 0, &dwSize);
  if (dwSize == 0) { CloseHandle(hToken); return; }

  std::vector<char> token_buf(dwSize);
  if (!GetTokenInformation(hToken, TokenUser, token_buf.data(), dwSize, &dwSize)) {
    CloseHandle(hToken);
    return;
  }
  CloseHandle(hToken);

  auto* token_user = reinterpret_cast<TOKEN_USER*>(token_buf.data());

  EXPLICIT_ACCESS ea = {};
  ea.grfAccessPermissions = GENERIC_READ | GENERIC_WRITE;
  ea.grfAccessMode = SET_ACCESS;
  ea.grfInheritance = NO_INHERITANCE;
  ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
  ea.Trustee.TrusteeType = TRUSTEE_IS_USER;
  ea.Trustee.ptstrName = reinterpret_cast<LPSTR>(token_user->User.Sid);

  PACL p_new_dacl = nullptr;
  if (SetEntriesInAclA(1, &ea, nullptr, &p_new_dacl) != ERROR_SUCCESS) return;

  SetNamedSecurityInfoA(
      const_cast<char*>(path.string().c_str()),
      SE_FILE_OBJECT,
      DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
      nullptr, nullptr, p_new_dacl, nullptr);

  if (p_new_dacl) LocalFree(p_new_dacl);
#else
  if (::chmod(path.c_str(), 0600) != 0) {
    std::cerr << "[IdentityStore] Warning: could not set permissions on "
              << path << " (" << std::strerror(errno) << ")\n";
  }
#endif
}

}  // namespace

IdentityStore::IdentityStore(std::filesystem::path path) : path_(std::move(path)) {}

NodeIdentity IdentityStore::load_or_create(const std::string& node_id) const {
  auto base_dir = path_.parent_path();
  if (base_dir.empty()) base_dir = ".";

  auto config_path = resolve_secure_path(base_dir, path_.filename());

  if (std::filesystem::exists(config_path)) {
    std::ifstream input(config_path);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    const auto content = buffer.str();

    auto key_path = config_path;
    key_path += ".key";

    std::error_code ec;
    auto key_size = std::filesystem::file_size(key_path, ec);
    if (ec) {
      throw std::runtime_error("Cannot access key file: " + key_path.string() + " (" + ec.message() + ")");
    }
    if (key_size != EXPECTED_KEY_SIZE) {
      throw std::runtime_error("Key file " + key_path.string() + " has invalid size (" +
                               std::to_string(key_size) + " bytes), expected " +
                               std::to_string(EXPECTED_KEY_SIZE) + " bytes");
    }

    return NodeIdentity{
        read_value(content, "node_id"),
        read_value(content, "public_key_hex"),
        key_path.string(),
    };
  }

  auto keypair = generate_ed25519_keypair();
  const auto public_key_hex = encode_hex(keypair.public_key);

  std::filesystem::create_directories(config_path.parent_path());

  auto key_path = config_path;
  key_path += ".key";

  {
    std::ofstream key_file(key_path, std::ios::trunc | std::ios::binary);
    key_file.write(reinterpret_cast<const char*>(keypair.secret_key.data()),
                   static_cast<std::streamsize>(keypair.secret_key.size()));
    key_file.flush();
  }

  restrict_file_permissions(key_path);

  NodeIdentity identity{node_id, public_key_hex, key_path.string()};
  {
    std::ofstream output(config_path, std::ios::trunc);
    output << "node_id=" << identity.node_id << "\n";
    output << "public_key_hex=" << identity.public_key_hex << "\n";
    output << "private_key_path=" << identity.private_key_path << "\n";
    output.flush();
  }

  keypair.secret_key.clear();

  return identity;
}

}  // namespace beacon
