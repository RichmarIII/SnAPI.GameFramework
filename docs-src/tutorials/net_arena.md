# Net Arena

This tutorial connects the session and object layers of networking.

The premise is a tiny arena:

- the server hosts the world
- clients join players
- a level is loaded by request
- gameplay objects replicate
- one reflected RPC is used for a toy action

## What You Will Learn

- how to bootstrap server and client runtimes
- when to use `GameplayHost`
- when to use reflected object RPC
- what replication is responsible for

## 1. Server Runtime Settings

```cpp
GameRuntimeSettings Server{};
Server.WorldName = "ArenaServer";

GameRuntimeNetworkingSettings Net{};
Net.Role = ESessionRole::Server;
Net.BindAddress = "0.0.0.0";
Net.BindPort = 7777;
Net.AutoConnect = false;
Server.Networking = Net;

Server.Gameplay = GameRuntimeGameplaySettings{};
```

## 2. Client Runtime Settings

```cpp
GameRuntimeSettings Client{};
Client.WorldName = "ArenaClient";

GameRuntimeNetworkingSettings Net{};
Net.Role = ESessionRole::Client;
Net.ConnectAddress = "127.0.0.1";
Net.ConnectPort = 7777;
Net.AutoConnect = true;
Client.Networking = Net;

Client.Gameplay = GameRuntimeGameplaySettings{};
```

## 3. Use `GameplayHost` For Join And Level Flow

On either side, once initialized:

```cpp
auto* Host = Runtime.Gameplay();
if (!Host)
{
    return;
}

(void)Host->RequestJoinPlayer("ArenaPlayer", std::optional<unsigned int>{0}, true);
(void)Host->RequestLoadLevel("Arena01");
```

On the server, those operations execute authoritatively.
On clients, they route through the gameplay RPC gateway.

## 4. Mark Gameplay Objects For Replication

For a node or component to replicate, do both:

- set the object replication gate
- flag the fields that should replicate

That division is deliberate. It prevents entire objects from leaking over the network just because one field happened to be marked.

## 5. Add A Toy Reflected RPC

For example, a turret node that tells the server it wants to fire and tells clients to play a muzzle flash.

The key design lesson is:

- session-level actions -> `GameplayHost`
- object-level actions -> reflected RPC on the object

## 6. What The Bridges Handle For You

`NetReplicationBridge` handles:

- spawn
- update
- despawn
- pending parent resolution
- pending component resolution

`NetRpcBridge` handles:

- reflected method lookup
- method id stability
- argument packing/unpacking
- target UUID resolution

## 7. What You Still Own

You still own:

- deciding which fields replicate
- deciding which methods are RPC methods
- designing authority rules
- choosing whether a behavior belongs on a gameplay host, node, or component

## 8. Debugging Checklist

When something seems wrong:

1. Is the networking session actually initialized?
2. Is the object itself marked replicated?
3. Are the fields actually flagged for replication?
4. Is the reflected RPC signature an exact match?
5. Are you using a session-level request where you really needed `GameplayHost`?

Continue with [Networking, Replication, and Reflected RPC](networking.md).
