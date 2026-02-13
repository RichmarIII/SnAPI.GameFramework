# Audio Components

This page explains how audio is integrated into GameFramework through world-owned subsystems and components.

## 1. Architecture

- `World` owns `AudioSystem`.
- `World` also owns `NetworkSystem`, so audio gameplay RPC can route through world networking.
- `World::Tick(...)` performs the per-frame `AudioSystem::Update(...)`.
- `AudioListenerComponent` writes listener transform each tick.
- `AudioSourceComponent` controls emitter, sound loading, playback, and spatial settings.

Design goal: gameplay code accesses systems through world context (`Owner()->World()->Audio()` pattern), not globals.

## 2. Add a Listener to a Camera Node

```cpp
auto CameraNodeResult = Graph.CreateNode("Camera");
auto* CameraNode = CameraNodeResult->Borrowed();

auto CameraTransform = CameraNode->Add<TransformComponent>();
CameraTransform->Position = Vec3(0.0f, 2.0f, -6.0f);

auto Listener = CameraNode->Add<AudioListenerComponent>();
Listener->Active(true);
```

What happens:

- `OnCreate()` initializes audio system lazily.
- Each tick, listener transform is pushed to `AudioEngine`.
- The audio backend frame update itself is handled once by `World::Tick(...)`.

## 3. Add a Source to an Actor Node

```cpp
auto ActorNodeResult = Graph.CreateNode("Speaker");
auto* ActorNode = ActorNodeResult->Borrowed();

auto ActorTransform = ActorNode->Add<TransformComponent>();
ActorTransform->Position = Vec3(3.0f, 0.0f, 0.0f);

auto Source = ActorNode->Add<AudioSourceComponent>();
Source->EditSettings().SoundPath = "assets/audio/loop.wav";
Source->EditSettings().Streaming = false;
Source->EditSettings().AutoPlay = true;
Source->EditSettings().Looping = true;
Source->EditSettings().Volume = 0.8f;
Source->EditSettings().MinDistance = 1.0f;
Source->EditSettings().MaxDistance = 40.0f;
Source->EditSettings().Rolloff = 1.0f;
```

## 4. Runtime Control

```cpp
Source->Play();
Source->Stop();

bool Loaded = Source->IsLoaded();
bool Playing = Source->IsPlaying();

Source->LoadSound("assets/audio/one_shot.wav", false);
Source->UnloadSound();
```

`AudioSourceComponent` keeps emitter state synced with current settings and owner transform.

## 5. Networked Playback Flow

`AudioSourceComponent` exposes gameplay methods (`Play`, `Stop`) and internally routes through `IComponent::CallRPC(...)` when networking is attached.

```cpp
void AudioSourceComponent::Play()
{
    if (CallRPC("PlayServer"))
    {
        return;
    }
    PlayClient();
}
```

`Play()` flow:

1. Client call: `CallRPC("PlayServer")` forwards to server.
2. Server `PlayServer`: `CallRPC("PlayClient")` multicasts.
3. `PlayClient`: executes playback on listening peers.

`Stop()` follows the same pattern (`Stop -> StopServer -> StopClient`).

Dedicated server guard:

- `PlayClient`/`StopClient` do not emit audio on non-listen dedicated server instances.
- Listen server will execute local playback plus multicast to clients.

## 6. What Actually Replicates

By default in built-in reflection registration:

- `AudioSourceComponent::Settings` is replication-visible as a container field.
- Inside `Settings`, only `SoundPath` is flagged for replication.
- Other settings (`Volume`, `Looping`, distances, etc.) are local unless you add replication flags for them.

Important consequence:

- current default network payload for settings carries the path string (`SoundPath`), not the full settings struct.
- audio bytes are not replicated; clients must resolve/load the referenced asset locally.

## 7. Field Flags vs Value Codec (Nested Structs)

Replication for a reflected field behaves as follows:

- if the field type has a registered value codec, that codec serializes the full field value.
- if no codec exists, replication walks nested reflected fields and serializes only nested fields marked with replication flags.

For `AudioSourceComponent::Settings`, default behavior is nested-field traversal, so only flagged members replicate.

## 8. Important Runtime Notes

- Source/listener components rely on owner node transform if `TransformComponent` exists.
- Audio system initialization is lazy (`Initialize()` called as needed).
- Audio components are ordinary reflected components, so they serialize like other components.

## 9. Minimal Frame Pump

```cpp
while (Running)
{
    const float Dt = 1.0f / 60.0f;
    WorldInstance.Tick(Dt);
    WorldInstance.LateTick(Dt);
    WorldInstance.EndFrame();
}
```

Listener/source updates happen through normal component ticking.
Audio backend update is world-owned, not component-owned.

## 10. Troubleshooting

- No audio output:
    - verify audio asset path in `Settings.SoundPath`
    - ensure listener exists and is active
    - ensure source was loaded/played

- Replicated play event arrives but no sound:
    - ensure the client machine can resolve the same `SoundPath`
    - confirm the source node/component has `Replicated(true)` enabled

- Spatial audio sounds wrong:
    - check transform values and distance parameters (`MinDistance`, `MaxDistance`, `Rolloff`)

Next: [Testing and Validation](testing.md)
