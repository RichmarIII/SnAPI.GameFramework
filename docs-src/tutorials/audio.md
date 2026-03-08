# Audio Components

Audio follows the same world-owned pattern as the other subsystems.

- `World` owns `AudioSystem`
- `AudioListenerComponent` writes the shared listener transform
- `AudioSourceComponent` owns an emitter handle and optional replicated playback flow

## 1. Add A Listener

```cpp
auto CameraHandle = WorldInstance.CreateNode<BaseNode>("AudioCamera");
if (!CameraHandle)
{
    return;
}

auto* CameraNode = CameraHandle->Borrowed();
if (!CameraNode)
{
    return;
}

if (auto Transform = CameraNode->Add<TransformComponent>())
{
    Transform->Position = Vec3(0.0f, 1.8f, -6.0f);
}

if (auto Listener = CameraNode->Add<AudioListenerComponent>())
{
    Listener->SetActive(true);
}
```

Important reality:

- the audio backend has one shared listener state per world audio system
- multiple active listener components are not arbitrated
- the last active listener to tick wins

## 2. Add A Source

```cpp
auto SpeakerHandle = WorldInstance.CreateNode<BaseNode>("Speaker");
if (!SpeakerHandle)
{
    return;
}

auto* Speaker = SpeakerHandle->Borrowed();
if (!Speaker)
{
    return;
}

if (auto Transform = Speaker->Add<TransformComponent>())
{
    Transform->Position = Vec3(3.0f, 0.0f, 0.0f);
}

if (auto Source = Speaker->Add<AudioSourceComponent>())
{
    auto& Settings = Source->EditSettings();
    Settings.SoundPath = "assets/audio/loop.wav";
    Settings.Streaming = false;
    Settings.AutoPlay = true;
    Settings.Looping = true;
    Settings.Volume = 0.8f;
    Settings.MinDistance = 1.0f;
    Settings.MaxDistance = 40.0f;
    Settings.Rolloff = 1.0f;
}
```

## 3. Understand Deferred Playback

`AudioSourceComponent` can defer actual playback if the sound or engine is not ready yet.

That means calling `Play()` does not guarantee the backend starts in the same line of code. It guarantees the component records the intent and tries to fulfill it when the load/runtime state allows.

## 4. Network-Aware Playback

`AudioSourceComponent::Play()` and `Stop()` are gameplay-facing entry points.

They follow a role-aware flow:

- client tries the server RPC path first
- authority applies local state and fans out to client endpoints
- dedicated servers skip actual local playback

So the high-level rule is:

- call `Play()` / `Stop()` from gameplay code
- do not manually jump straight to `PlayClient()` unless you are intentionally bypassing the normal routing

## 5. What Actually Replicates

The settings field itself is reflected and marked for replication, but actual nested replication still depends on reflected field metadata and codec behavior.

The practical point is:

- do not assume every nested audio setting auto-replicates just because the settings container exists
- verify the actual reflected fields and replication flags for the values you care about

## 6. Sound Data Is Not Replicated

Only metadata and gameplay intent are networked.

Clients are still expected to resolve and load the referenced audio asset locally.

## What To Read Next

- [Haunted Radio](haunted_radio.md)
- [Networking, Replication, and Reflected RPC](networking.md)
