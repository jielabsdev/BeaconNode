import importlib.util
import sys
from pathlib import Path


def test_churn_simulation_reaches_nonzero_convergence() -> None:
    module_path = Path(__file__).parents[2] / "tools" / "run_churn_simulation.py"
    spec = importlib.util.spec_from_file_location("run_churn_simulation", module_path)
    assert spec is not None
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    sys.modules["run_churn_simulation"] = module
    spec.loader.exec_module(module)

    result = module.run_simulation(nodes=4, agents=12, rounds=12, churn=0.1, seed=3)

    assert result["convergence"] > 0
    assert result["live_agents"] > 0
