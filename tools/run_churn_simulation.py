from __future__ import annotations

import argparse
import random
from dataclasses import dataclass, field


@dataclass
class SimAgent:
    agent_id: str
    revision: int
    ttl: int


@dataclass
class SimNode:
    node_id: str
    registry: dict[str, SimAgent] = field(default_factory=dict)

    @property
    def highest_revision(self) -> int:
        return max((agent.revision for agent in self.registry.values()), default=0)

    def register(self, agent: SimAgent) -> None:
        existing = self.registry.get(agent.agent_id)
        if existing is None or agent.revision >= existing.revision:
            self.registry[agent.agent_id] = SimAgent(agent.agent_id, agent.revision, agent.ttl)

    def purge(self) -> None:
        expired = [agent_id for agent_id, agent in self.registry.items() if agent.ttl <= 0]
        for agent_id in expired:
            del self.registry[agent_id]

    def tick(self) -> None:
        for agent in self.registry.values():
            agent.ttl -= 1
        self.purge()


def gossip_round(left: SimNode, right: SimNode, limit: int) -> None:
    left_delta = sorted(
        [agent for agent in left.registry.values() if agent.revision > right.highest_revision],
        key=lambda agent: agent.revision,
    )[:limit]
    right_delta = sorted(
        [agent for agent in right.registry.values() if agent.revision > left.highest_revision],
        key=lambda agent: agent.revision,
    )[:limit]

    for agent in left_delta:
        right.register(agent)
    for agent in right_delta:
        left.register(agent)


def run_simulation(nodes: int, agents: int, rounds: int, churn: float, seed: int) -> dict[str, float]:
    random.seed(seed)
    cluster = [SimNode(f"node-{index}") for index in range(nodes)]
    revision = 0

    for index in range(agents):
        revision += 1
        cluster[index % nodes].register(SimAgent(f"agent-{index}", revision, ttl=random.randint(4, 12)))

    for _ in range(rounds):
        for node in cluster:
            node.tick()
            if random.random() < churn:
                revision += 1
                node.register(SimAgent(f"agent-new-{revision}", revision, ttl=random.randint(4, 12)))

        for _ in range(nodes * 2):
            left, right = random.sample(cluster, 2)
            gossip_round(left, right, limit=32)

    live_sets = [set(node.registry) for node in cluster]
    union = set().union(*live_sets)
    intersection = set.intersection(*live_sets) if live_sets else set()
    convergence = len(intersection) / len(union) if union else 1.0
    avg_registry_size = sum(len(node.registry) for node in cluster) / len(cluster)

    return {
        "nodes": nodes,
        "rounds": rounds,
        "live_agents": len(union),
        "avg_registry_size": avg_registry_size,
        "convergence": convergence,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--nodes", type=int, default=8)
    parser.add_argument("--agents", type=int, default=40)
    parser.add_argument("--rounds", type=int, default=30)
    parser.add_argument("--churn", type=float, default=0.2)
    parser.add_argument("--seed", type=int, default=7)
    args = parser.parse_args()

    result = run_simulation(args.nodes, args.agents, args.rounds, args.churn, args.seed)
    for key, value in result.items():
        print(f"{key}={value}")


if __name__ == "__main__":
    main()

