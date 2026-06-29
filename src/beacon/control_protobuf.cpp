#include "beacon/control_protobuf.hpp"

#include "beacon/crypto.hpp"
#include "beacon/framing.hpp"
#include "beacon/identity_service.hpp"

#include "registry.pb.h"

#include <chrono>
#include <utility>

namespace beacon {
namespace {

AgentRecord from_proto(const beaconnode::v1::AgentRegistration& proto, LifecycleConfig lifecycle) {
  AgentRecord record;
  record.agent_id = proto.agent_id();
  record.node_id = proto.node_id();
  record.endpoint = proto.endpoint();
  record.revision = proto.revision();
  record.expires_at = Clock::now() + lifecycle.heartbeat_ttl;
  record.public_key_hex = encode_hex(std::vector<std::uint8_t>(proto.public_key().begin(), proto.public_key().end()));
  record.signature_hex = encode_hex(std::vector<std::uint8_t>(proto.signature().begin(), proto.signature().end()));

  for (const auto& [key, value] : proto.metadata()) {
    record.metadata[key] = value;
  }

  return record;
}

void to_proto(const AgentRecord& record, beaconnode::v1::AgentRegistration& proto) {
  proto.set_agent_id(record.agent_id);
  proto.set_node_id(record.node_id);
  proto.set_endpoint(record.endpoint);
  proto.set_revision(record.revision);
  auto& metadata = *proto.mutable_metadata();
  for (const auto& [key, value] : record.metadata) {
    metadata[key] = value;
  }

  const auto public_key = decode_hex(record.public_key_hex);
  const auto signature = decode_hex(record.signature_hex);
  proto.set_public_key(public_key.data(), static_cast<int>(public_key.size()));
  proto.set_signature(signature.data(), static_cast<int>(signature.size()));
}

void set_status(beaconnode::v1::ControlResponse& response, bool ok, const std::string& message) {
  response.set_ok(ok);
  response.set_message(message);
}

bool signature_allowed(const AgentRecord& record, TrustConfig trust, beaconnode::v1::ControlResponse& response) {
  const auto signature_status = verify_agent_signature(record);
  if (trust.require_signatures && signature_status != SignatureStatus::Valid) {
    set_status(response, false, signature_status_message(signature_status));
    return false;
  }
  if (!trust.require_signatures && signature_status == SignatureStatus::Invalid) {
    set_status(response, false, "invalid_signature");
    return false;
  }
  return true;
}

}  // namespace

std::vector<std::uint8_t> handle_control_request_payload(
    AgentRegistry& registry,
    const ControlProtocol& protocol,
    const std::vector<std::uint8_t>& payload) {
  beaconnode::v1::ControlRequest request;
  beaconnode::v1::ControlResponse response;

  if (!request.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
    set_status(response, false, "invalid_protobuf_control_request");
  } else if (request.has_register_agent()) {
    auto record = from_proto(request.register_agent(), protocol.lifecycle());
    if (record.agent_id.empty() || record.node_id.empty() || record.endpoint.empty()) {
      set_status(response, false, "agent_id_node_id_and_endpoint_required");
    } else if (signature_allowed(record, protocol.trust(), response)) {
      const auto accepted = registry.upsert(std::move(record));
      if (accepted && protocol.identity_service()) {
        protocol.identity_service()->verify_and_register(record);
      }
      set_status(response, accepted, accepted ? "registered" : "stale_revision");
    }
  } else if (request.has_sync_merge()) {
    auto record = from_proto(request.sync_merge(), protocol.lifecycle());
    if (record.agent_id.empty() || record.node_id.empty() || record.endpoint.empty()) {
      set_status(response, false, "agent_id_node_id_and_endpoint_required");
    } else if (signature_allowed(record, protocol.trust(), response)) {
      const auto accepted = registry.upsert(std::move(record));
      if (accepted && protocol.identity_service()) {
        protocol.identity_service()->verify_and_register(record);
      }
      set_status(response, accepted, accepted ? "merged" : "stale_revision");
    }
  } else if (request.has_heartbeat()) {
    const auto& heartbeat = request.heartbeat();
    const auto accepted = registry.refresh_heartbeat(
        heartbeat.agent_id(),
        Clock::now() + protocol.lifecycle().heartbeat_ttl,
        heartbeat.revision());
    set_status(response, accepted, accepted ? "heartbeat" : "unknown_or_stale_agent");
  } else if (request.has_list_agents()) {
    set_status(response, true, "list");
    for (const auto& record : registry.list()) {
      to_proto(record, *response.mutable_snapshot()->add_agents());
    }
  } else if (request.has_sync_pull()) {
    const auto& pull = request.sync_pull();
    set_status(response, true, "sync_pull");
    for (const auto& record : registry.entries_after_revision(pull.after_revision(), pull.limit())) {
      to_proto(record, *response.mutable_snapshot()->add_agents());
    }
  } else if (request.has_stats()) {
    const auto stats = registry.stats();
    set_status(
        response,
        true,
        "active_agents=" + std::to_string(stats.active_agents) +
            " highest_revision=" + std::to_string(stats.highest_revision));
  } else {
    set_status(response, false, "empty_control_request");
  }

  std::string serialized;
  if (!response.SerializeToString(&serialized)) {
    return {};
  }
  return std::vector<std::uint8_t>(serialized.begin(), serialized.end());
}

std::vector<std::uint8_t> handle_control_frame(
    AgentRegistry& registry,
    const ControlProtocol& protocol,
    const std::vector<std::uint8_t>& frame_bytes) {
  const auto frame = decode_frame(frame_bytes);
  if (!frame.has_value() || frame->kind != FrameKind::ControlRequest) {
    beaconnode::v1::ControlResponse response;
    set_status(response, false, "invalid_control_frame");
    std::string serialized;
    if (!response.SerializeToString(&serialized)) {
      return {};
    }
    return encode_frame({FrameKind::ControlResponse, {serialized.begin(), serialized.end()}});
  }

  return encode_frame({FrameKind::ControlResponse, handle_control_request_payload(registry, protocol, frame->payload)});
}

}  // namespace beacon
