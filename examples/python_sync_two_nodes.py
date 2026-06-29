from beaconnode import BeaconClient, sync_with_peer


def main() -> None:
    first = BeaconClient(port=43171)
    second = BeaconClient(port=43181)
    print(sync_with_peer(first, second))


if __name__ == "__main__":
    main()
