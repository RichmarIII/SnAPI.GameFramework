#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "BaseComponent.h"
#include "ScriptRuntime.h"
#include "ReflectionAnnotations.h"

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Component that binds one gameplay object to a backend script instance.
 *
 * `ScriptComponent` is the standard engine-side bridge from the Node/Component model to
 * the scripting runtime. Users configure a script module path in `ScriptModule` and an
 * optional entry point name in `ScriptType`; the component then creates and maintains a
 * live `IScript` instance on demand.
 *
 * Design intent:
 * - let gameplay authors attach script behavior declaratively to Nodes
 * - keep script lifetime aligned with component lifetime
 * - allow hot reload and editor property edits to rebind safely without forcing the rest
 *   of the engine to know about backend details
 *
 * Core semantics:
 * - `OnCreate()` does not bind immediately. It marks the component as needing a create
 *   hook and defers the actual bind until a later tick or editor-triggered rebind.
 * - Backend selection is derived from `ScriptModule`. The current implementation treats
 *   an empty extension or `.lua` as `EScriptBackend::Lua`; any other extension resolves
 *   to `EScriptBackend::None`.
 * - Rebinding occurs when the backend changes, the module path changes, the entry point
 *   changes, or the backing module's hot-reload generation changes.
 * - Script hook failures are logged to `stderr` and swallowed so the owning World can
 *   continue ticking.
 *
 * Ownership and lifetime:
 * - The component owns no backend runtime itself.
 * - The bound script instance is held by `std::shared_ptr<IScript>` and released on
 *   destroy or rebind.
 * - `Instance` is a diagnostic runtime id only; `0` means "currently unbound".
 *
 * Threading:
 * - Main-thread/world-thread only.
 * - Not thread-safe; it mutates runtime bindings and may touch World-owned services.
 *
 * Performance:
 * - Tick hooks may trigger lazy binding and therefore can allocate, initialize a backend,
 *   or reload modules on the first use after configuration changes.
 *
 * @warning A component with an invalid script path or unsupported extension will log a
 * bind warning once until the configuration changes or a new bind succeeds.
 * @see ScriptRuntimeService, IScript, EScriptHook
 */
SnType()
class ScriptComponent : public BaseComponent, public ComponentCRTP<ScriptComponent>
{
public:
    /** @brief Stable type name for reflection. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::ScriptComponent";

    SnField(SnKey("ScriptModule"))
    std::string ScriptModule; /**< @brief Backend-visible script module path or module identifier. Resolved through `PathResolver` before backend selection and instance creation. */
    SnField(SnKey("ScriptType"))
    std::string ScriptType; /**< @brief Optional backend-specific entry point, such as a Lua table or factory field inside `ScriptModule`. */
    SnField(SnKey("Instance"))
    ScriptInstanceId Instance = 0; /**< @brief Runtime instance id of the currently bound script. `0` means no live script instance is bound. */

    /**
     * @brief Begin the component's script lifecycle for a new engine lifetime.
     *
     * This call resets the runtime instance id, marks the component as needing an
     * `EScriptHook::OnCreate`, and clears one-shot bind-failure suppression flags. It does
     * not create a script instance immediately.
     *
     * @param WorldRef Owning World. Present for lifecycle symmetry; the current
     *        implementation defers actual binding to later calls.
     */
    void OnCreate(IWorld& WorldRef);

    /**
     * @brief Tear down the currently bound script instance, if any.
     * @param WorldRef Owning World.
     *
     * @post Any bound script instance is released.
     * @post `EScriptHook::OnDestroy` is delivered only when a script was successfully
     *       bound and had previously received `EScriptHook::OnCreate`.
     */
    void OnDestroy(IWorld& WorldRef);

    /**
     * @brief Forward the engine pre-tick phase to the bound script.
     * @param WorldRef Owning World.
     * @param DeltaSeconds Elapsed time in seconds since the previous pre-tick.
     */
    void PreTick(IWorld& WorldRef, float DeltaSeconds);

    /**
     * @brief Forward the engine main tick phase to the bound script.
     * @param WorldRef Owning World.
     * @param DeltaSeconds Elapsed time in seconds since the previous variable tick.
     */
    void Tick(IWorld& WorldRef, float DeltaSeconds);

    /**
     * @brief Forward the engine fixed-timestep phase to the bound script.
     * @param WorldRef Owning World.
     * @param DeltaSeconds Fixed simulation step in seconds.
     */
    void FixedTick(IWorld& WorldRef, float DeltaSeconds);

    /**
     * @brief Forward the engine late-tick phase to the bound script.
     * @param WorldRef Owning World.
     * @param DeltaSeconds Elapsed time in seconds since the previous late-tick.
     */
    void LateTick(IWorld& WorldRef, float DeltaSeconds);

    /**
     * @brief Forward the engine post-tick phase to the bound script.
     * @param WorldRef Owning World.
     * @param DeltaSeconds Elapsed time in seconds since the previous post-tick.
     */
    void PostTick(IWorld& WorldRef, float DeltaSeconds);
#if defined(WITH_EDITOR) && WITH_EDITOR
    /**
     * @brief Respond to editor-side property edits that affect script binding.
     *
     * The current implementation only reacts to `ScriptModule` and `ScriptType`. Those
     * edits reset the bind state, mark `OnCreate` pending again, and attempt an immediate
     * rebind if the component is already associated with a World.
     *
     * @param Name Reflected property name that changed.
     */
    void EditorOnPropertyChanged(std::string_view Name);
#endif

private:
    [[nodiscard]] EScriptBackend ResolveBackend() const;
    [[nodiscard]] Result EnsureBound(IWorld& WorldRef);
    void Unbind(bool InvokeDestroyHook);
    void InvokeHook(IWorld& WorldRef, EScriptHook Hook, std::span<const Variant> Args = {});

    std::shared_ptr<IScript> m_script{}; /**< @brief Shared handle to the currently bound script instance. Empty when unbound. */
    std::string m_boundModule{}; /**< @brief Module path used to create the current script instance. Compared against `ScriptModule` to decide whether rebinding is required. */
    std::string m_boundEntryPoint{}; /**< @brief Entry point used to create the current script instance. Compared against `ScriptType` to decide whether rebinding is required. */
    EScriptBackend m_boundBackend = EScriptBackend::None; /**< @brief Backend used by the current binding. */
    std::uint64_t m_boundModuleGeneration = 0; /**< @brief Hot-reload generation captured when the current script instance was created. */
    bool m_pendingCreateHook = false; /**< @brief True when the next successful bind must deliver `EScriptHook::OnCreate`. */
    bool m_createHookDelivered = false; /**< @brief True after `EScriptHook::OnCreate` has been delivered to the current binding. */
    bool m_bindFailureLogged = false; /**< @brief Suppresses repeated identical bind warnings until configuration changes or a new bind succeeds. */
};

} // namespace SnAPI::GameFramework
