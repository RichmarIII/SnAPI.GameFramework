#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "BaseComponent.h"
#include "ScriptRuntime.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Component that binds a node to a script instance.
 * @remarks
 * `ScriptModule` is interpreted as script path.
 * `ScriptType` is interpreted as optional entry point/class name within that module.
 *
 * Runtime behavior:
 * - component lifecycle hooks forward to script lifecycle hooks
 * - binding/rebinding is lazy and hot-reload aware
 */
class ScriptComponent : public BaseComponent, public ComponentCRTP<ScriptComponent>
{
public:
    /** @brief Stable type name for reflection. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::ScriptComponent";

    std::string ScriptModule; /**< @brief Script path/module identifier. */
    std::string ScriptType; /**< @brief Optional script entry point/class identifier. */
    ScriptInstanceId Instance = 0; /**< @brief Runtime instance id (0 indicates unbound). */

    void OnCreate(IWorld& WorldRef);
    void OnDestroy(IWorld& WorldRef);

    void PreTick(IWorld& WorldRef, float DeltaSeconds);
    void Tick(IWorld& WorldRef, float DeltaSeconds);
    void FixedTick(IWorld& WorldRef, float DeltaSeconds);
    void LateTick(IWorld& WorldRef, float DeltaSeconds);
    void PostTick(IWorld& WorldRef, float DeltaSeconds);
#if defined(WITH_EDITOR) && WITH_EDITOR
    void EditorOnPropertyChanged(std::string_view Name);
#endif

private:
    [[nodiscard]] EScriptBackend ResolveBackend() const;
    [[nodiscard]] Result EnsureBound(IWorld& WorldRef);
    void Unbind(bool InvokeDestroyHook);
    void InvokeHook(IWorld& WorldRef, EScriptHook Hook, std::span<const Variant> Args = {});

    std::shared_ptr<IScript> m_script{};
    std::string m_boundModule{};
    std::string m_boundEntryPoint{};
    EScriptBackend m_boundBackend = EScriptBackend::None;
    std::uint64_t m_boundModuleGeneration = 0;
    bool m_pendingCreateHook = false;
    bool m_createHookDelivered = false;
    bool m_bindFailureLogged = false;
};

} // namespace SnAPI::GameFramework
