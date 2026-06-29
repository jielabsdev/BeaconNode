#include "beacon/agent_registry.hpp"
#include "beacon/identity_service.hpp"

#include <algorithm>
#include <utility>

namespace beacon {

bool AgentRegistry::upsert(AgentRecord record) {
  {
    std::lock_guard lock(mutex_);
    auto existing = std::find_if(records_.begin(), records_.end(), [&](const AgentRecord& candidate) {
      return candidate.agent_id == record.agent_id;
    });

    if (existing == records_.end()) {
      records_.push_back(record);
    } else {
      if (record.revision < existing->revision) {
        return false;
      }
      if (!existing->public_key_hex.empty() && !record.public_key_hex.empty() &&
          existing->public_key_hex != record.public_key_hex) {
        return false;
      }
      *existing = record;
    }
  }

  if (on_upsert_) {
    on_upsert_(record);
  }

  return true;
}

bool AgentRegistry::refresh_heartbeat(const std::string& agent_id, TimePoint expires_at, std::uint64_t revision) {
  std::lock_guard lock(mutex_);
  auto existing = std::find_if(records_.begin(), records_.end(), [&](const AgentRecord& candidate) {
    return candidate.agent_id == agent_id;
  });

  if (existing == records_.end() || revision < existing->revision) {
    return false;
  }

  existing->expires_at = expires_at;
  existing->revision = revision;
  return true;
}

std::size_t AgentRegistry::merge(const std::vector<AgentRecord>& records, IdentityService* identity) {
  std::size_t accepted = 0;
  for (const auto& record : records) {
    if (identity) {
      const auto status = identity->verify_and_register(record);
      if (status != SignatureStatus::Valid && status != SignatureStatus::Missing) {
        continue;
      }
    }
    if (upsert(AgentRecord(record))) {
      accepted += 1;
    }
  }

  return accepted;
}

std::vector<AgentRecord> AgentRegistry::entries_after_revision(std::uint64_t revision, std::size_t limit) const {
  std::lock_guard lock(mutex_);
  std::vector<AgentRecord> selected;

  for (const auto& record : records_) {
    if (record.revision > revision) {
      selected.push_back(record);
    }
  }

  std::sort(selected.begin(), selected.end(), [](const AgentRecord& left, const AgentRecord& right) {
    return left.revision < right.revision;
  });

  if (limit > 0 && selected.size() > limit) {
    selected.resize(limit);
  }

  return selected;
}

std::optional<AgentRecord> AgentRegistry::find(const std::string& agent_id) const {
  std::lock_guard lock(mutex_);
  auto existing = std::find_if(records_.begin(), records_.end(), [&](const AgentRecord& candidate) {
    return candidate.agent_id == agent_id;
  });

  if (existing == records_.end()) {
    return std::nullopt;
  }

  return *existing;
}

std::vector<AgentRecord> AgentRegistry::list() const {
  std::lock_guard lock(mutex_);
  return records_;
}

std::size_t AgentRegistry::purge_expired(TimePoint now) {
  std::lock_guard lock(mutex_);
  const auto before = records_.size();
  records_.erase(
      std::remove_if(records_.begin(), records_.end(), [&](const AgentRecord& record) {
        return record.expires_at <= now;
      }),
      records_.end());
  return before - records_.size();
}

void AgentRegistry::set_upsert_callback(UpsertCallback cb) {
  std::lock_guard lock(mutex_);
  on_upsert_ = std::move(cb);
}

void AgentRegistry::load_from_persistence(const std::vector<AgentRecord>& records) {
  std::lock_guard lock(mutex_);
  const auto far_future = Clock::now() + std::chrono::hours(24 * 365);
  for (const auto& record : records) {
    auto existing = std::find_if(records_.begin(), records_.end(), [&](const AgentRecord& candidate) {
      return candidate.agent_id == record.agent_id;
    });
    if (existing == records_.end()) {
      auto copy = record;
      copy.expires_at = far_future;
      records_.push_back(std::move(copy));
    } else if (record.revision > existing->revision) {
      existing->expires_at = far_future;
      *existing = record;
    }
  }
}

RegistryStats AgentRegistry::stats() const {
  std::lock_guard lock(mutex_);
  RegistryStats result;
  result.active_agents = records_.size();

  for (const auto& record : records_) {
    result.highest_revision = std::max(result.highest_revision, record.revision);
  }

  return result;
}

}  // namespace beacon
