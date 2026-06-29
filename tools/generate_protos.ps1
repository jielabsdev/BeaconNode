param(
  [string]$OutDir = "generated"
)

$ErrorActionPreference = "Stop"

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
New-Item -ItemType Directory -Force -Path "$OutDir/cpp" | Out-Null
New-Item -ItemType Directory -Force -Path "$OutDir/python" | Out-Null

protoc `
  --proto_path=proto `
  --cpp_out="$OutDir/cpp" `
  --python_out="$OutDir/python" `
  proto/beaconnode/v1/registry.proto `
  proto/beaconnode/v1/gossip.proto

Write-Host "Generated protobuf bindings in $OutDir"
