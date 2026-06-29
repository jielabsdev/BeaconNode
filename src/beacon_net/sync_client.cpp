#include "beacon_net/sync_client.hpp"
#include "beacon/identity_service.hpp"

#include "beacon/registry_sync.hpp"

#include <boost/asio.hpp>

#include <charconv>
#include <chrono>
#include <sstream>
#include <utility>

namespace beacon::net {
namespace {

using boost::asio::ip::tcp;

std::string field_value(const std::string& token) {
  const auto equals = token.find('=');
  if (equals == std::string::npos) {
    return {};
  }

  return token.substr(equals + 1);
}

bool parse_revision(const std::string& value, std::uint64_t& revision) {
  const auto* begin = value.data();
  const auto* end = value.data() + value.size();
  const auto result = std::from_chars(begin, end, revision);
  return result.ec == std::errc{} && result.ptr == end;
}

std::vector<AgentRecord> parse_agent_lines(const std::string& response) {
  std::istringstream lines(response);
  std::vector<AgentRecord> records;
  std::string line;

  while (std::getline(lines, line)) {
    std::istringstream tokens(line);
    std::string token;
    tokens >> token;
    if (token != "AGENT") {
      continue;
    }

    AgentRecord record;
    record.expires_at = Clock::now() + std::chrono::minutes(1);
    while (tokens >> token) {
      if (token.rfind("agent_id=", 0) == 0) {
        record.agent_id = field_value(token);
      } else if (token.rfind("node_id=", 0) == 0) {
        record.node_id = field_value(token);
      } else if (token.rfind("endpoint=", 0) == 0) {
        record.endpoint = field_value(token);
      } else if (token.rfind("revision=", 0) == 0) {
        parse_revision(field_value(token), record.revision);
      } else if (token.rfind("public_key=", 0) == 0) {
        record.public_key_hex = field_value(token);
      } else if (token.rfind("signature=", 0) == 0) {
        record.signature_hex = field_value(token);
      } else if (token.rfind("metadata.", 0) == 0) {
        const auto equals = token.find('=');
        if (equals != std::string::npos) {
          record.metadata[token.substr(9, equals - 9)] = token.substr(equals + 1);
        }
      }
    }

    if (!record.agent_id.empty()) {
      records.push_back(std::move(record));
    }
  }

  return records;
}

std::uint64_t parse_highest_revision(const std::string& response) {
  std::istringstream tokens(response);
  std::string token;

  while (tokens >> token) {
    if (token.rfind("highest_revision=", 0) == 0) {
      std::uint64_t revision = 0;
      parse_revision(field_value(token), revision);
      return revision;
    }
  }

  return 0;
}

}  // namespace

SyncClient::SyncClient(std::size_t max_entries_per_round, IdentityService* identity)
    : max_entries_per_round_(max_entries_per_round), identity_(identity) {}

SyncResult SyncClient::sync_with_peer(AgentRegistry& local_registry, const SyncPeer& peer) const {
  const auto local_revision = local_registry.stats().highest_revision;
  const auto remote_stats = send_command(peer, "STATS");
  const auto remote_revision = parse_highest_revision(remote_stats);

  SyncResult result;
  RegistrySync sync(max_entries_per_round_);
  result.pulled = merge_response(
      local_registry,
      send_command(peer, encode_sync_pull_command(local_revision, max_entries_per_round_)));

  const auto local_plan = sync.plan_push_pull(local_registry, remote_revision);
  result.pushed = push_entries(peer, local_plan.push_entries);
  return result;
}

std::string SyncClient::send_command(const SyncPeer& peer, const std::string& command) const {
  boost::asio::io_context context;
  tcp::socket socket(context);
  socket.connect(tcp::endpoint(boost::asio::ip::make_address(peer.host), peer.control_port));

  boost::asio::write(socket, boost::asio::buffer(command + "\n"));

  boost::asio::streambuf response;
  boost::system::error_code error;
  boost::asio::read(socket, response, error);
  if (error != boost::asio::error::eof && error) {
    throw boost::system::system_error(error);
  }

  std::ostringstream output;
  output << &response;
  return output.str();
}

std::size_t SyncClient::merge_response(AgentRegistry& registry, const std::string& response) const {
  return registry.merge(parse_agent_lines(response), identity_);
}

std::size_t SyncClient::push_entries(const SyncPeer& peer, const std::vector<AgentRecord>& entries) const {
  std::size_t pushed = 0;
  for (const auto& entry : entries) {
    const auto response = send_command(peer, encode_merge_command(entry));
    if (response.rfind("OK", 0) == 0) {
      pushed += 1;
    }
  }

  return pushed;
}

}  // namespace beacon::net
