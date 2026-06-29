from __future__ import annotations

from dataclasses import dataclass

from .client import BeaconClient


@dataclass(slots=True)
class SyncResult:
    pulled: int = 0
    pushed: int = 0


def sync_with_peer(local: BeaconClient, remote: BeaconClient, limit: int = 256) -> SyncResult:
    local_stats = local.stats()
    remote_stats = remote.stats()

    local_revision = int(local_stats.get("highest_revision", 0))
    remote_revision = int(remote_stats.get("highest_revision", 0))

    pulled = 0
    for registration in remote.sync_pull(after_revision=local_revision, limit=limit):
        response = local.merge(registration)
        if response.get("ok"):
            pulled += 1

    pushed = 0
    for registration in local.sync_pull(after_revision=remote_revision, limit=limit):
        response = remote.merge(registration)
        if response.get("ok"):
            pushed += 1

    return SyncResult(pulled=pulled, pushed=pushed)
