# Haunted Radio

This tutorial is a deliberately silly audio exercise: build a radio prop that crackles to life when the player gets close and becomes a network-aware spooky object later.

## What You Will Learn

- listener vs source responsibilities
- audio source settings that matter in practice
- deferred playback behavior
- how object-level RPC-aware playback differs from asset replication

## 1. Place The Listener On The Player Camera

```cpp
auto CameraHandle = WorldInstance.CreateNode<BaseNode>("PlayerCamera");
auto* CameraNode = CameraHandle ? CameraHandle->Borrowed() : nullptr;
if (!CameraNode)
{
    return;
}

(void)CameraNode->Add<TransformComponent>();
(void)CameraNode->Add<AudioListenerComponent>();
```

Use `SetActive(true)` if you want the gameplay-facing, role-aware path.

## 2. Create The Haunted Radio Prop

```cpp
auto RadioHandle = WorldInstance.CreateNode<BaseNode>("HauntedRadio");
auto* Radio = RadioHandle ? RadioHandle->Borrowed() : nullptr;
if (!Radio)
{
    return;
}

if (auto Transform = Radio->Add<TransformComponent>())
{
    Transform->Position = Vec3(4.0f, 0.8f, -2.0f);
}

if (auto Source = Radio->Add<AudioSourceComponent>())
{
    auto& Audio = Source->EditSettings();
    Audio.SoundPath = "assets/audio/radio_loop.wav";
    Audio.Streaming = false;
    Audio.AutoPlay = false;
    Audio.Looping = true;
    Audio.MinDistance = 1.0f;
    Audio.MaxDistance = 12.0f;
    Audio.Rolloff = 1.0f;
}
```

## 3. Trigger Playback From Gameplay Distance Checks

A beginner-friendly version can simply measure distance between the player and the prop each tick and call `Play()` or `Stop()` when crossing a threshold.

The key takeaway is not the exact distance formula. The key takeaway is that `Play()` is the correct gameplay entry point.

## 4. Understand Why `Play()` Is Better Than Forcing Backend Calls

`AudioSourceComponent::Play()` knows about:

- authority/client routing
- deferred load readiness
- backend availability
- dedicated-server no-audio behavior

That is much safer than bypassing the component and directly poking the audio backend from gameplay code.

## 5. What Actually Goes Over The Network

When the radio becomes networked, remember this:

- the sound bytes are not replicated
- clients still need the asset locally
- the component can replicate or RPC the intent/state that causes playback

This is the correct model for most game audio.

## 6. Make It More Haunted

Good extensions for practice:

1. When the player enters the room, `Play()` the loop.
2. When the player presses an interact key, swap `SoundPath` to a one-shot whisper.
3. Add a second `AudioSourceComponent` for burst static.
4. Add a `ScriptComponent` so a Lua script controls the timing.

Continue with [Scripted Gadget Lab](scripted_gadget_lab.md) if you want that last step.
