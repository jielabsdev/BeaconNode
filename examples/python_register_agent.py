from beaconnode import AgentRegistration, BeaconClient, BeaconConnectionError


def main() -> None:
    client = BeaconClient()
    try:
        response = client.register(
            AgentRegistration(
                agent_id="example-agent",
                node_id="dev-node",
                endpoint="127.0.0.1:9000",
                metadata={"capability": "demo"},
            )
        )
        print(response)
        print(client.stats())
        print(list(client.list_agents()))
    except BeaconConnectionError as error:
        print(error)


if __name__ == "__main__":
    main()
