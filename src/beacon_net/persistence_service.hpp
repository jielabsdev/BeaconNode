#pragma once

#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

namespace beaconnode {

struct PersistenceConfig {
    std::string base_dir = ".";
    std::string wal_path = "registry.wal";
    std::string snapshot_path = "registry.bin";
};

namespace v1 { class RegistryEntry; }

class PersistenceService {
public:
    PersistenceService(const PersistenceConfig& config);
    ~PersistenceService();

    void append_entry(const v1::RegistryEntry& entry);
    void create_snapshot(const std::vector<v1::RegistryEntry>& current_entries);
    std::vector<v1::RegistryEntry> recover();

private:
    PersistenceConfig config_;
    FILE* wal_file_ = nullptr;
    std::mutex persistence_mutex_;

    static uint32_t compute_crc32(const std::string& data);
    static void flush_handle(FILE* fp);
    void open_wal();
};

} // namespace beaconnode
