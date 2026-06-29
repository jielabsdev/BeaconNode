# Resilience Testing

BeaconNode includes a deterministic churn simulator in `tools/run_churn_simulation.py`.

```powershell
python tools\run_churn_simulation.py --nodes 8 --agents 40 --rounds 30 --churn 0.2
```

The simulator models:

- new agent registrations
- heartbeat expiry
- registry purging
- random peer gossip rounds
- convergence ratio across the cluster

Example output:

```text
nodes=8
rounds=30
live_agents=17
avg_registry_size=12.25
convergence=0.47
```

The simulator is intentionally lightweight. It gives quick feedback about convergence strategy changes before running heavier integration tests with real engines.

