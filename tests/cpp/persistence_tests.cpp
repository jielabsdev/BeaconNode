#include "beacon_net/persistence_service.hpp"
#include "gossip.pb.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

beaconnode::v1::RegistryEntry make_entry(const std::string& agent_id, uint64_t timestamp) {
    beaconnode::v1::RegistryEntry e;
    e.set_agent_id(agent_id);
    e.set_capability("n");
    e.set_endpoint("e:1");
    e.set_timestamp(timestamp);
    return e;
}

void cleanup() {
    std::error_code ec;
    std::filesystem::remove("test_persist.wal", ec);
    std::filesystem::remove("test_persist.bin", ec);
    std::filesystem::remove("test_persist.bin.tmp", ec);
}

void test_append_and_recover() {
    cleanup();
    beaconnode::PersistenceConfig cfg{".", "test_persist.wal", "test_persist.bin"};
    {
        beaconnode::PersistenceService p(cfg);
        p.append_entry(make_entry("agent-1", 100));
        p.append_entry(make_entry("agent-2", 200));
    }
    {
        beaconnode::PersistenceService p(cfg);
        auto recovered = p.recover();
        assert(recovered.size() == 2);
        assert(recovered[0].agent_id() == "agent-1");
        assert(recovered[1].agent_id() == "agent-2");
    }
    cleanup();
    std::cout << "  test_append_and_recover PASS\n";
}

void test_snapshot_atomic_write() {
    cleanup();
    beaconnode::PersistenceConfig cfg{".", "test_persist.wal", "test_persist.bin"};
    {
        beaconnode::PersistenceService p(cfg);
        p.append_entry(make_entry("agent-1", 100));
        std::vector<beaconnode::v1::RegistryEntry> entries;
        entries.push_back(make_entry("agent-1", 100));
        p.create_snapshot(entries);
    }
    assert(std::filesystem::exists("test_persist.bin"));
    assert(!std::filesystem::exists("test_persist.bin.tmp"));
    {
        beaconnode::PersistenceService p(cfg);
        auto recovered = p.recover();
        assert(recovered.size() == 1);
        assert(recovered[0].agent_id() == "agent-1");
    }
    cleanup();
    std::cout << "  test_snapshot_atomic_write PASS\n";
}

void test_snapshot_crc_detects_corruption() {
    cleanup();
    beaconnode::PersistenceConfig cfg{".", "test_persist.wal", "test_persist.bin"};
    {
        beaconnode::PersistenceService p(cfg);
        std::vector<beaconnode::v1::RegistryEntry> entries;
        entries.push_back(make_entry("agent-1", 100));
        p.create_snapshot(entries);
    }
    // Corrupt the snapshot by flipping a byte in the first entry payload
    {
        std::fstream f("test_persist.bin", std::ios::binary | std::ios::in | std::ios::out);
        f.seekp(sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t) + 5, std::ios::beg);
        char corrupted = static_cast<char>(0xFF);
        f.write(&corrupted, 1);
    }
    {
        beaconnode::PersistenceService p(cfg);
        auto recovered = p.recover();
        assert(recovered.size() == 0);
    }
    cleanup();
    std::cout << "  test_snapshot_crc_detects_corruption PASS\n";
}

void test_rejects_oversized_entry() {
    cleanup();
    beaconnode::PersistenceConfig cfg{".", "test_persist.wal", "test_persist.bin"};
    // Write a snapshot with an entry claiming >10MB size
    {
        std::ofstream f("test_persist.bin", std::ios::binary);
        uint32_t total = 1;
        uint32_t size = 11ULL * 1024 * 1024;
        uint32_t crc = 0;
        f.write(reinterpret_cast<const char*>(&total), sizeof(total));
        f.write(reinterpret_cast<const char*>(&size), sizeof(size));
        f.write(reinterpret_cast<const char*>(&crc), sizeof(crc));
    }
    {
        beaconnode::PersistenceService p(cfg);
        bool threw = false;
        try {
            p.recover();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        assert(threw);
    }
    cleanup();
    std::cout << "  test_rejects_oversized_entry PASS\n";
}

void test_rejects_excessive_records() {
    cleanup();
    beaconnode::PersistenceConfig cfg{".", "test_persist.wal", "test_persist.bin"};
    {
        std::ofstream f("test_persist.bin", std::ios::binary);
        uint32_t total = 2'000'000;
        f.write(reinterpret_cast<const char*>(&total), sizeof(total));
    }
    {
        beaconnode::PersistenceService p(cfg);
        bool threw = false;
        try {
            p.recover();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        assert(threw);
    }
    cleanup();
    std::cout << "  test_rejects_excessive_records PASS\n";
}

void test_rejects_trailing_bytes() {
    cleanup();
    beaconnode::PersistenceConfig cfg{".", "test_persist.wal", "test_persist.bin"};
    {
        beaconnode::PersistenceService p(cfg);
        std::vector<beaconnode::v1::RegistryEntry> entries;
        entries.push_back(make_entry("agent-1", 100));
        p.create_snapshot(entries);
    }
    // Append garbage after the snapshot
    {
        std::ofstream f("test_persist.bin", std::ios::binary | std::ios::app);
        char junk[] = "EXTRA_BYTES";
        f.write(junk, sizeof(junk) - 1);
    }
    {
        beaconnode::PersistenceService p(cfg);
        bool threw = false;
        try {
            p.recover();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        assert(threw);
    }
    cleanup();
    std::cout << "  test_rejects_trailing_bytes PASS\n";
}

} // namespace

int main() {
    test_append_and_recover();
    test_snapshot_atomic_write();
    test_snapshot_crc_detects_corruption();
    test_rejects_oversized_entry();
    test_rejects_excessive_records();
    test_rejects_trailing_bytes();
    std::cout << "All persistence tests passed.\n";
    return 0;
}
