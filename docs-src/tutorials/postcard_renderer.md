# Postcard Renderer

This tutorial is about building a tiny scene that looks intentional instead of being just a cube on a floor.

You will not build a full renderer feature showcase. You will learn how the renderer-facing gameplay nodes fit together in a clean, modern setup.

## Scene Idea

Create a quiet postcard scene:

- a ground plane
- a few stylized primitive props
- one camera
- one `WorldRenderSettings` node
- optional atmospheric/post-process asset references

## 1. Start With A Camera And Three Props

Use:

- `CameraComponent`
- `TransformComponent`
- `StaticMeshComponent`

Choose three primitive props with strong silhouettes, for example:

- a monument block
- a mailbox cube tower
- a giant tilted sign

The point is to practice transform, camera, and mesh setup, not asset import.

## 2. Use Primitive Mesh Tokens First

Primitive mesh tokens are the most reliable beginner path for `StaticMeshComponent`.

```cpp
Mesh->EditSettings().MeshPath = "primitive://box";
```

That lets you focus on composition instead of import debugging.

## 3. Add A `WorldRenderSettings` Node

```cpp
auto SettingsHandle = MainLevel->CreateNode<WorldRenderSettings>("WorldRenderSettings");
auto* RenderSettings = SettingsHandle ? NodeCast<WorldRenderSettings>(SettingsHandle->Borrowed()) : nullptr;
if (!RenderSettings)
{
    return;
}
```

`WorldRenderSettings` is a data-driven organizer for world-scoped render parameter nodes.

It is especially useful when you want a reproducible scene look.

## 4. Point It At Parameter Assets

If your project has parameter assets, wire them through the asset refs.

```cpp
RenderSettings->EditToneMapParams().SetAsset("tone_map_sunrise", "");
RenderSettings->EditBloomParams().SetAsset("bloom_soft", "");
RenderSettings->EditSSAOParams().SetAsset("ssao_medium", "");
```

That is the intended style:

- store references on `WorldRenderSettings`
- let it materialize or refresh the corresponding child parameter nodes

## 5. Why This Node Exists

Without `WorldRenderSettings`, projects end up scattering post-process nodes around the world manually.

This node gives you:

- one place to reason about scene look
- one place to swap parameter sets
- one node that can be serialized with the scene layout

## 6. Keep The Lazy Viewport Rule In Mind

If you are doing this inside the editor, remember:

- renderer subsystem readiness is not the same as viewport readiness
- viewport creation can still be layout-driven and lazy
- render-setting application during editor bootstrap depends on deferred `OnCreate` safety

That is real engine behavior, not a docs footnote.

## 7. Fun Extensions

1. Create three postcard variants using three different tone-map assets.
2. Serialize each as a separate level asset.
3. Add one `UIRenderViewport`-backed inspector panel that swaps between them.

Continue with [Renderer Integration](renderer.md).
