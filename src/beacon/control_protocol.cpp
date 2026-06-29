#include "beacon/control_protocol.hpp"
#include "beacon/crypto.hpp"
#include "beacon/identity_service.hpp"

#include <charconv>
#include <sstream>
#include <string_view>

namespace beacon {
namespace {

std::vector<std::string> split_tokens(const std::string& line) {
  std::istringstream stream(line);
  std::vector<std::string> tokens;
  std::string token;

  while (stream >> token) {
    tokens.push_back(token);
  }

  return tokens;
}

bool parse_revision(std::string_view value, std::uint64_t& revision) {
  const auto* begin = value.data();
  const auto* end = value.data() + value.size();
  const auto result = std::from_chars(begin, end, revision);
  return result.ec == std::errc{} && result.ptr == end;
}

std::string get_value(const std::string& token) {
  const auto equals = token.find('=');
  if (equals == std::string::npos) {
    return {};
  }

  return token.substr(equals + 1);
}

bool starts_with(const std::string& token, std::string_view prefix) {
  return token.rfind(prefix, 0) == 0;
}

void register_identity(IdentityService* identity, const AgentRecord& record) {
  if (!identity) return;
  if (record.public_key_hex.empty()) return;
  identity->verify_and_register(record);
}

}  // namespace

ControlProtocol::ControlProtocol(LifecycleConfig lifecycle, TrustConfig trust, IdentityService* identity)
    : lifecycle_(lifecycle), trust_(trust), identity_(identity) {}

LifecycleConfig ControlProtocol::lifecycle() const {
  return lifecycle_;
}

TrustConfig ControlProtocol::trust() const {
  return trust_;
}

IdentityService* ControlProtocol::identity_service() const {
  return identity_;
}

ControlCommand ControlProtocol::parse_command(const std::string& line) const {
  const auto tokens = split_tokens(line);
  ControlCommand command;

  if (tokens.empty()) {
    command.error = "empty command";
    return command;
  }

  if (tokens[0] == "REGISTER") {
    command.action = ControlAction::Register;
    command.registration.expires_at = Clock::now() + lifecycle_.heartbeat_ttl;
  } else if (tokens[0] == "MERGE") {
    command.action = ControlAction::Merge;
    command.registration.expires_at = Clock::now() + lifecycle_.heartbeat_ttl;
  } else if (tokens[0] == "HEARTBEAT") {
    command.action = ControlAction::Heartbeat;
  } else if (tokens[0] == "LIST") {
    command.action = ControlAction::List;
  } else if (tokens[0] == "STATS") {
    command.action = ControlAction::Stats;
  } else if (tokens[0] == "SYNC_PULL") {
    command.action = ControlAction::SyncPull;
  } else {
    command.error = "unknown command";
    return command;
  }

  for (std::size_t index = 1; index < tokens.size(); ++index) {
    const auto& token = tokens[index];
    if (starts_with(token, "agent_id=")) {
      command.agent_id = get_value(token);
      command.registration.agent_id = command.agent_id;
    } else if (starts_with(token, "node_id=")) {
      command.registration.node_id = get_value(token);
    } else if (starts_with(token, "endpoint=")) {
      command.registration.endpoint = get_value(token);
    } else if (starts_with(token, "revision=")) {
      std::uint64_t revision = 0;
      if (!parse_revision(get_value(token), revision)) {
        command.error = "invalid revision";
        return command;
      }
      command.registration.revision = revision;
    } else if (starts_with(token, "after_revision=")) {
      std::uint64_t revision = 0;
      if (!parse_revision(get_value(token), revision)) {
        command.error = "invalid after_revision";
        return command;
      }
      command.after_revision = revision;
    } else if (starts_with(token, "limit=")) {
      std::uint64_t limit = 0;
      if (!parse_revision(get_value(token), limit)) {
        command.error = "invalid limit";
        return command;
      }
      command.limit = static_cast<std::size_t>(limit);
    } else if (starts_with(token, "public_key=")) {
      command.registration.public_key_hex = get_value(token);
    } else if (starts_with(token, "signature=")) {
      command.registration.signature_hex = get_value(token);
    } else if (starts_with(token, "metadata.")) {
      const auto equals = token.find('=');
      if (equals != std::string::npos) {
        command.registration.metadata[token.substr(9, equals - 9)] = token.substr(equals + 1);
      }
    }
  }

  if ((command.action == ControlAction::Register || command.action == ControlAction::Merge ||
       command.action == ControlAction::Heartbeat) &&
      command.agent_id.empty()) {
    command.error = "agent_id is required";
    return command;
  }

  if ((command.action == ControlAction::Register || command.action == ControlAction::Merge) &&
      (command.registration.node_id.empty() || command.registration.endpoint.empty())) {
    command.error = "node_id and endpoint are required";
    return command;
  }

  return command;
}

std::string ControlProtocol::handle_line(AgentRegistry& registry, const std::string& line) {
  const auto command = parse_command(line);
  if (!command.error.empty()) {
    return "ERR message=" + command.error + "\n";
  }

  if (command.action == ControlAction::Register) {
    const auto signature_status = verify_agent_signature(command.registration);
    if (trust_.require_signatures && signature_status != SignatureStatus::Valid) {
      return "ERR message=" + signature_status_message(signature_status) + "\n";
    }
    if (!trust_.require_signatures && signature_status == SignatureStatus::Invalid) {
      return "ERR message=invalid_signature\n";
    }
    const auto accepted = registry.upsert(command.registration);
    if (accepted) {
      register_identity(identity_, command.registration);
    }
    return accepted ? "OK message=registered\n" : "ERR message=stale_revision\n";
  }

  if (command.action == ControlAction::Merge) {
    const auto signature_status = verify_agent_signature(command.registration);
    if (trust_.require_signatures && signature_status != SignatureStatus::Valid) {
      return "ERR message=" + signature_status_message(signature_status) + "\n";
    }
    if (!trust_.require_signatures && signature_status == SignatureStatus::Invalid) {
      return "ERR message=invalid_signature\n";
    }
    const auto accepted = registry.upsert(command.registration);
    if (accepted) {
      register_identity(identity_, command.registration);
    }
    return accepted ? "OK message=merged\n" : "ERR message=stale_revision\n";
  }

  if (command.action == ControlAction::Heartbeat) {
    const auto expires_at = Clock::now() + lifecycle_.heartbeat_ttl;
    const auto accepted = registry.refresh_heartbeat(command.agent_id, expires_at, command.registration.revision);
    return accepted ? "OK message=heartbeat\n" : "ERR message=unknown_or_stale_agent\n";
  }

  if (command.action == ControlAction::Stats) {
    const auto stats = registry.stats();
    return "OK active_agents=" + std::to_string(stats.active_agents) +
           " highest_revision=" + std::to_string(stats.highest_revision) + "\n";
  }

  if (command.action == ControlAction::List) {
    std::string response = "OK message=list\n";
    for (const auto& record : registry.list()) {
      response += encode_agent_line(record);
    }
    response += "END\n";
    return response;
  }

  if (command.action == ControlAction::SyncPull) {
    std::string response = "OK message=sync_pull\n";
    for (const auto& record : registry.entries_after_revision(command.after_revision, command.limit)) {
      response += encode_agent_line(record);
    }
    response += "END\n";
    return response;
  }

  return "ERR message=unhandled_command\n";
}

std::string encode_agent_line(const AgentRecord& record) {
  std::string line = "AGENT agent_id=" + record.agent_id +
                     " node_id=" + record.node_id +
                     " endpoint=" + record.endpoint +
                     " revision=" + std::to_string(record.revision);

  if (!record.public_key_hex.empty()) {
    line += " public_key=" + record.public_key_hex;
  }
  if (!record.signature_hex.empty()) {
    line += " signature=" + record.signature_hex;
  }

  for (const auto& [key, value] : record.metadata) {
    line += " metadata." + key + "=" + value;
  }

  line += "\n";
  return line;
}

}  // namespace beacon
