from beaconnode import probe_peer


def main() -> None:
    print(probe_peer(node_id="python-probe"))


if __name__ == "__main__":
    main()
