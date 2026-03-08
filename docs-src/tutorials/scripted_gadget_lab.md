# Scripted Gadget Lab

This tutorial introduces the current script integration model through a tiny gadget object.

The gadget is simple:

- it is a node in the world
- it has a `ScriptComponent`
- the script module path decides which backend is used
- the script instance binds lazily and participates in engine lifecycle hooks

## What You Will Learn

- what `ScriptComponent` actually does
- why bind is lazy instead of immediate
- how hot reload interacts with the component
- where scripting fits relative to native nodes/components

## 1. Create A Gadget Node

```cpp
auto GadgetHandle = WorldInstance.CreateNode<BaseNode>("GlowGadget");
auto* Gadget = GadgetHandle ? GadgetHandle->Borrowed() : nullptr;
if (!Gadget)
{
    return;
}

(void)Gadget->Add<TransformComponent>();
```

## 2. Attach `ScriptComponent`

```cpp
auto Script = Gadget->Add<ScriptComponent>();
if (!Script)
{
    return;
}

Script->ScriptModule = "scripts/glow_gadget.lua";
Script->ScriptType = "GlowGadget";
```

The current backend resolution rules matter here:

- empty extension or `.lua` resolves to the Lua backend
- unsupported extensions resolve to `EScriptBackend::None`

## 3. Understand The Lazy Bind Rule

`ScriptComponent::OnCreate()` does not immediately create the script instance.

Instead it marks the component as needing a create hook and binds lazily on later tick/editor-triggered flow.

Why that is the right design:

- world services are definitely available by then
- hot reload state can be checked
- editor property changes can safely force rebinding
- failed binds do not crash the entire world

## 4. The World Owns The Script Runtime Service

Access it through:

```cpp
ScriptRuntimeService& Scripts = WorldInstance.Scripts();
```

The world registers builtin script backends during its own construction, so the runtime service is part of normal world ownership rather than something you bolt on later.

## 5. Hook Flow

The component can forward engine lifecycle hooks such as:

- pre-tick
- tick
- fixed tick
- late tick
- post tick
- create/destroy

That means script code can participate in the same broad lifecycle as native components.

## 6. Hot Reload Model

The runtime service tracks module generations.

`ScriptComponent` rebinding can happen when:

- backend changes
- module path changes
- entry point changes
- module generation changes after a hot reload

That is why the component keeps track of:

- bound module
- bound entry point
- bound backend
- bound module generation

## 7. Error Behavior

This is one of the friendlier parts of the design.

- bind failures are logged to `stderr`
- hook failures are logged and swallowed
- the world keeps ticking

That is the right failure mode for a tool-friendly gameplay framework.

## 8. Good Extensions

1. Add a `StaticMeshComponent` so the gadget can animate a visible prop.
2. Add an `AudioSourceComponent` and let the script trigger `Play()`.
3. Add a reflected native field and let the script read it through your script ABI layer.

Continue with [Haunted Radio](haunted_radio.md) if you want a script-controlled spooky prop.
