#include "beacon/agent_registry.hpp"
#include "beacon/config.hpp"
#include "beacon/control_protocol.hpp"
#include "beacon/framing.hpp"
#include "beacon/crypto.hpp"
#include "beacon/gossip_scheduler.hpp"
#include "beacon/peer_handshake.hpp"
#include "beacon/registry_sync.hpp"
#include "beacon/reputation.hpp"

#include <cassert>
#include <chrono>

int main() {
  beacon::AgentRegistry registry;
  const auto now = beacon::Clock::now();

  beacon::AgentRecord first;
  first.agent_id = "agent-1";
  first.node_id = "node-1";
  first.endpoint = "127.0.0.1:9000";
  first.revision = 1;
  first.expires_at = now + std::chrono::seconds(5);

  assert(registry.upsert(first));
  assert(registry.find("agent-1").has_value());
  assert(registry.stats().active_agents == 1);
  assert(registry.stats().highest_revision == 1);

  beacon::AgentRecord stale = first;
  stale.endpoint = "127.0.0.1:9999";
  stale.revision = 0;
  assert(!registry.upsert(stale));
  assert(registry.find("agent-1")->endpoint == "127.0.0.1:9000");

  assert(registry.refresh_heartbeat("agent-1", now + std::chrono::seconds(10), 2));
  assert(registry.stats().highest_revision == 2);
  assert(registry.purge_expired(now + std::chrono::seconds(11)) == 1);
  assert(registry.stats().active_agents == 0);

  beacon::GossipScheduler gossip(beacon::GossipConfig{});
  assert(gossip.select_push_entries(registry.list()).empty());

  beacon::ReputationEngine reputation(beacon::TrustConfig{});
  assert(reputation.accepts_peer("peer-1"));
  reputation.record_malformed("peer-1");
  assert(reputation.accepts_peer("peer-1"));

  beacon::ControlProtocol control(beacon::LifecycleConfig{});
  assert(control.handle_line(registry, "REGISTER agent_id=agent-2 node_id=node-2 endpoint=127.0.0.1:9001 revision=1 metadata.role=worker") == "OK message=registered\n");
  assert(control.handle_line(registry, "HEARTBEAT agent_id=agent-2 revision=2") == "OK message=heartbeat\n");
  assert(control.handle_line(registry, "STATS").find("active_agents=1") != std::string::npos);
  assert(control.handle_line(registry, "LIST").find("AGENT agent_id=agent-2") != std::string::npos);
  assert(control.handle_line(registry, "SYNC_PULL after_revision=1 limit=10").find("AGENT agent_id=agent-2") != std::string::npos);

  beacon::RegistrySync sync(10);
  const auto sync_plan = sync.plan_push_pull(registry, 0);
  assert(sync_plan.local_revision == 2);
  assert(sync_plan.push_entries.size() == 1);
  assert(beacon::encode_merge_command(sync_plan.push_entries[0]).find("MERGE agent_id=agent-2") == 0);

  const auto hello = beacon::encode_peer_hello({"node-3", 43170});
  const auto decoded_hello = beacon::decode_peer_hello(hello);
  assert(decoded_hello.has_value());
  assert(decoded_hello->node_id == "node-3");
  assert(decoded_hello->listen_port == 43170);

  const auto welcome = beacon::encode_peer_welcome({"node-4", 7});
  const auto decoded_welcome = beacon::decode_peer_welcome(welcome);
  assert(decoded_welcome.has_value());
  assert(decoded_welcome->registry_revision == 7);

  const std::vector<std::uint8_t> payload = {'o', 'k'};
  const auto frame_bytes = beacon::encode_frame({beacon::FrameKind::GossipEnvelope, payload});
  const auto decoded_frame = beacon::decode_frame(frame_bytes);
  assert(decoded_frame.has_value());
  assert(decoded_frame->kind == beacon::FrameKind::GossipEnvelope);
  assert(decoded_frame->payload == payload);

  assert(beacon::canonical_agent_payload(sync_plan.push_entries[0]).find("agent_id=agent-2") != std::string::npos);
  assert(beacon::verify_agent_signature(sync_plan.push_entries[0]) == beacon::SignatureStatus::Missing);

  return 0;
}
