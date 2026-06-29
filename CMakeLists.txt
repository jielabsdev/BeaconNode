cmake_minimum_required(VERSION 3.24)

project(
  BeaconNode
  VERSION 0.1.0
  DESCRIPTION "High-performance P2P discovery service for AI agents"
  LANGUAGES CXX
)

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

option(BEACONNODE_BUILD_TESTS "Build BeaconNode tests" ON)
option(BEACONNODE_ENABLE_ASIO_NETWORKING "Enable Boost.Asio networking services" ON)
option(BEACONNODE_ENABLE_LIBSODIUM "Enable Ed25519 verification through libsodium" OFF)
option(BEACONNODE_ENABLE_PROTOBUF "Enable protobuf code generation" OFF)
option(BEACONNODE_ENABLE_PYTHON_BINDINGS "Enable pybind11 bindings" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/CompilerWarnings.cmake)

if(BEACONNODE_ENABLE_ASIO_NETWORKING)
  find_package(Boost REQUIRED)
endif()

if(BEACONNODE_ENABLE_LIBSODIUM)
  find_package(unofficial-sodium CONFIG QUIET)
  if(NOT TARGET unofficial-sodium::sodium)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)
  endif()
endif()

if(BEACONNODE_ENABLE_PROTOBUF)
  find_package(Protobuf REQUIRED)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(PROTOBUF_PC QUIET IMPORTED_TARGET protobuf)
  endif()
  protobuf_generate_cpp(BEACONNODE_PROTO_SRCS BEACONNODE_PROTO_HDRS
    proto/beaconnode/v1/registry.proto
    proto/beaconnode/v1/gossip.proto
  )
  add_library(beacon_proto ${BEACONNODE_PROTO_SRCS} ${BEACONNODE_PROTO_HDRS})
  if(TARGET PkgConfig::PROTOBUF_PC)
    target_link_libraries(beacon_proto PUBLIC PkgConfig::PROTOBUF_PC)
  else()
    target_link_libraries(beacon_proto PUBLIC protobuf::libprotobuf)
  endif()
  target_include_directories(beacon_proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
endif()

add_library(beacon_core
  src/beacon/agent_registry.cpp
  src/beacon/config.cpp
  src/beacon/control_protocol.cpp
  src/beacon/crypto.cpp
  src/beacon/framing.cpp
  src/beacon/gossip_scheduler.cpp
  src/beacon/identity_service.cpp
  src/beacon/identity_store.cpp
  src/beacon/peer_handshake.cpp
  src/beacon/reputation.cpp
  src/beacon/registry_sync.cpp
)

if(BEACONNODE_ENABLE_PROTOBUF)
  target_sources(beacon_core PRIVATE src/beacon/control_protobuf.cpp)
  target_compile_definitions(beacon_core PUBLIC BEACONNODE_ENABLE_PROTOBUF=1)
endif()

target_include_directories(beacon_core PUBLIC src)
if(BEACONNODE_ENABLE_LIBSODIUM)
  target_compile_definitions(beacon_core PUBLIC BEACONNODE_ENABLE_LIBSODIUM=1)
  if(TARGET unofficial-sodium::sodium)
    target_link_libraries(beacon_core PUBLIC unofficial-sodium::sodium)
  else()
    target_link_libraries(beacon_core PUBLIC PkgConfig::SODIUM)
  endif()
endif()
if(BEACONNODE_ENABLE_PROTOBUF)
  target_link_libraries(beacon_core PUBLIC beacon_proto)
endif()
if(WIN32)
  target_link_libraries(beacon_core PRIVATE advapi32)
endif()
beaconnode_enable_warnings(beacon_core)

add_executable(beacon-engine src/beacon_engine/main.cpp)
target_link_libraries(beacon-engine PRIVATE beacon_core)
if(BEACONNODE_ENABLE_ASIO_NETWORKING)
  target_sources(beacon-engine PRIVATE
    src/beacon_net/control_server.cpp
    src/beacon_net/gossip_service.cpp
    src/beacon_net/persistence_service.cpp
    src/beacon_net/sync_client.cpp
    src/beacon_net/udp_handshake_responder.cpp
  )
  target_compile_definitions(beacon-engine PRIVATE BEACONNODE_ENABLE_ASIO_NETWORKING=1)
  if(BEACONNODE_ENABLE_PROTOBUF)
    target_compile_definitions(beacon-engine PRIVATE BEACONNODE_ENABLE_PROTOBUF=1)
  endif()
  if(TARGET Boost::headers)
    target_link_libraries(beacon-engine PRIVATE Boost::headers)
  else()
    target_include_directories(beacon-engine PRIVATE ${Boost_INCLUDE_DIRS})
  endif()
  if(WIN32)
    target_link_libraries(beacon-engine PRIVATE ws2_32 advapi32)
  endif()
endif()
beaconnode_enable_warnings(beacon-engine)

if(BEACONNODE_BUILD_TESTS)
  enable_testing()
  add_executable(beacon_core_tests tests/cpp/agent_registry_tests.cpp)
  target_link_libraries(beacon_core_tests PRIVATE beacon_core)
  beaconnode_enable_warnings(beacon_core_tests)
  add_test(NAME beacon_core_tests COMMAND beacon_core_tests)

  if(BEACONNODE_ENABLE_PROTOBUF)
    add_executable(persistence_tests
      tests/cpp/persistence_tests.cpp
      src/beacon_net/persistence_service.cpp
    )
    target_include_directories(persistence_tests PRIVATE src)
    target_compile_definitions(persistence_tests PRIVATE BEACONNODE_ENABLE_PROTOBUF=1)
    target_link_libraries(persistence_tests PRIVATE beacon_proto)
    if(WIN32)
      target_link_libraries(persistence_tests PRIVATE ws2_32 advapi32)
    endif()
    beaconnode_enable_warnings(persistence_tests)
    add_test(NAME persistence_tests COMMAND persistence_tests)
  endif()
endif()
