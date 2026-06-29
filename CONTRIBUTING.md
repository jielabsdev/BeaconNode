# Contributing

BeaconNode is intended to be a production-ready open-source project. Keep changes small, tested, and documented.

## Development Checks

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build
cd python
python -m pytest
python ..\tools\run_churn_simulation.py
```

## Design Principles

- Keep the Python SDK thin.
- Keep protocol contracts in protobuf.
- Prefer deterministic state transitions in the registry.
- Treat malformed peer data as untrusted input.
- Fail closed when signature verification is required but unavailable.
