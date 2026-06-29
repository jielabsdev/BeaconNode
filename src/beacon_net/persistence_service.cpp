#include "persistence_service.hpp"
#include "gossip.pb.h"

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#endif

namespace beaconnode {
namespace {

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

}  // namespace

PersistenceService::PersistenceService(const PersistenceConfig& config) : config_(config) {
    open_wal();
}

PersistenceService::~PersistenceService() {
    std::lock_guard<std::mutex> lock(persistence_mutex_);
    if (wal_file_) {
        fclose(wal_file_);
    }
}

void PersistenceService::open_wal() {
    auto path = resolve_secure_path(config_.base_dir, config_.wal_path);
    wal_file_ = fopen(path.string().c_str(), "ab");
    if (!wal_file_) {
        std::cerr << "[Persistence] CRITICAL: Failed to initialize WAL for path: " << path
                  << " (" << std::strerror(errno) << ")\n";
    }
}

uint32_t PersistenceService::compute_crc32(const std::string& data) {
    uint32_t crc = 0xFFFFFFFF;
    for (char c : data) {
        crc ^= static_cast<uint8_t>(c);
        for (int i = 0; i < 8; ++i) {
            if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
            else crc >>= 1;
        }
    }
    return ~crc;
}

void PersistenceService::flush_handle(FILE* fp) {
    if (!fp) return;
#ifdef _WIN32
    HANDLE hFile = (HANDLE)_get_osfhandle(_fileno(fp));
    if (hFile != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(hFile);
    }
#else
    fsync(fileno(fp));
#endif
}

void PersistenceService::append_entry(const v1::RegistryEntry& entry) {
    std::lock_guard<std::mutex> lock(persistence_mutex_);
    if (!wal_file_) return;

    std::string serialized;
    if (!entry.SerializeToString(&serialized)) {
        std::cerr << "[Persistence] Error: Serialization failure on entry append.\n";
        return;
    }

    uint32_t size = static_cast<uint32_t>(serialized.size());
    uint32_t checksum = compute_crc32(serialized);

    fwrite(&size, sizeof(size), 1, wal_file_);
    fwrite(&checksum, sizeof(checksum), 1, wal_file_);
    fwrite(serialized.data(), 1, size, wal_file_);

    fflush(wal_file_);
    flush_handle(wal_file_);
}

void PersistenceService::create_snapshot(const std::vector<v1::RegistryEntry>& current_entries) {
    std::lock_guard<std::mutex> lock(persistence_mutex_);

    if (wal_file_) {
        fclose(wal_file_);
        wal_file_ = nullptr;
    }

    auto snapshot_path = resolve_secure_path(config_.base_dir, config_.snapshot_path);
    auto tmp_path = snapshot_path;
    tmp_path += ".tmp";

    FILE* snap_file = fopen(tmp_path.string().c_str(), "wb");
    if (!snap_file) {
        std::cerr << "[Persistence] Error: Could not open temporary snapshot file: " << tmp_path << "\n";
        open_wal();
        return;
    }

    uint32_t total_records = static_cast<uint32_t>(current_entries.size());
    fwrite(&total_records, sizeof(total_records), 1, snap_file);

    for (const auto& entry : current_entries) {
        std::string serialized;
        if (entry.SerializeToString(&serialized)) {
            uint32_t size = static_cast<uint32_t>(serialized.size());
            uint32_t checksum = compute_crc32(serialized);
            fwrite(&size, sizeof(size), 1, snap_file);
            fwrite(&checksum, sizeof(checksum), 1, snap_file);
            fwrite(serialized.data(), 1, size, snap_file);
        }
    }

    fflush(snap_file);
    flush_handle(snap_file);
    fclose(snap_file);

    std::error_code ec;
    std::filesystem::rename(tmp_path, snapshot_path, ec);
    if (ec) {
        std::cerr << "[Persistence] Error: Failed to finalize snapshot (" << ec.message() << ")\n";
    }

    auto wal_path = resolve_secure_path(config_.base_dir, config_.wal_path);
    FILE* trunc = fopen(wal_path.string().c_str(), "wb");
    if (trunc) fclose(trunc);

    open_wal();
    std::cout << "[Persistence] Snapshot created; WAL rotated.\n";
}

static constexpr uint64_t MAX_ENTRY_SIZE = 10ULL * 1024 * 1024;
static constexpr uint32_t MAX_SNAPSHOT_RECORDS = 1'000'000;

std::vector<v1::RegistryEntry> PersistenceService::recover() {
    std::lock_guard<std::mutex> lock(persistence_mutex_);
    std::vector<v1::RegistryEntry> recovered_records;

    auto snapshot_path = resolve_secure_path(config_.base_dir, config_.snapshot_path);
    std::ifstream snap_file(snapshot_path, std::ios::binary);
    uint64_t max_snapshot_timestamp = 0;

    if (snap_file.is_open()) {
        uint32_t total_records = 0;
        snap_file.read(reinterpret_cast<char*>(&total_records), sizeof(total_records));

        if (total_records > MAX_SNAPSHOT_RECORDS) {
            throw std::runtime_error("Snapshot header reports " + std::to_string(total_records) +
                                     " records, which exceeds maximum of " +
                                     std::to_string(MAX_SNAPSHOT_RECORDS));
        }

        for (uint32_t i = 0; i < total_records; ++i) {
            uint32_t size = 0;
            uint32_t stored_crc = 0;

            if (!snap_file.read(reinterpret_cast<char*>(&size), sizeof(size))) break;

            if (size > MAX_ENTRY_SIZE) {
                throw std::runtime_error("Snapshot entry size " + std::to_string(size) +
                                         " exceeds maximum of " + std::to_string(MAX_ENTRY_SIZE));
            }

            if (!snap_file.read(reinterpret_cast<char*>(&stored_crc), sizeof(stored_crc))) break;

            std::string buffer(size, '\0');
            if (!snap_file.read(&buffer[0], size)) break;

            if (compute_crc32(buffer) != stored_crc) {
                std::cerr << "[Persistence] Snapshot entry CRC mismatch; skipping.\n";
                continue;
            }

            v1::RegistryEntry entry;
            if (entry.ParseFromString(buffer)) {
                if (entry.timestamp() > max_snapshot_timestamp) {
                    max_snapshot_timestamp = entry.timestamp();
                }
                recovered_records.push_back(std::move(entry));
            }
        }

        snap_file.clear();
        char extra;
        if (snap_file.read(&extra, 1).gcount() > 0) {
            throw std::runtime_error("Unexpected trailing bytes in snapshot: " + snapshot_path.string());
        }

        snap_file.close();
        std::cout << "[Persistence] Snapshot recovered: " << recovered_records.size() << " entries.\n";
    }

    auto wal_path = resolve_secure_path(config_.base_dir, config_.wal_path);
    std::ifstream replay_wal(wal_path, std::ios::binary);
    if (replay_wal.is_open()) {
        uint32_t corruption_dropped = 0;
        uint32_t timeline_dropped = 0;

        while (true) {
            uint32_t size = 0;
            uint32_t read_checksum = 0;

            if (!replay_wal.read(reinterpret_cast<char*>(&size), sizeof(size))) break;
            if (!replay_wal.read(reinterpret_cast<char*>(&read_checksum), sizeof(read_checksum))) break;

            if (size > MAX_ENTRY_SIZE) {
                std::cerr << "[Persistence] WAL entry size " << size << " exceeds limit; dropping.\n";
                corruption_dropped++;
                continue;
            }

            std::string buffer(size, '\0');
            if (!replay_wal.read(&buffer[0], size)) break;

            if (compute_crc32(buffer) != read_checksum) {
                corruption_dropped++;
                continue;
            }

            v1::RegistryEntry entry;
            if (entry.ParseFromString(buffer)) {
                if (entry.timestamp() <= max_snapshot_timestamp) {
                    timeline_dropped++;
                    continue;
                }
                recovered_records.push_back(std::move(entry));
            }
        }
        replay_wal.close();
        std::cout << "[Persistence] WAL replay done. (corrupt-dropped=" << corruption_dropped
                  << ", stale-dropped=" << timeline_dropped << ")\n";
    }

    return recovered_records;
}

} // namespace beaconnode
