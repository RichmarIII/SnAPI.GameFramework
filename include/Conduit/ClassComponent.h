#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "BaseComponent.h"
#include "Conduit/Asset.h"

namespace SnAPI::GameFramework
{
class IWorld;
}

namespace SnAPI::GameFramework::Conduit
{

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Runtime component that binds a `ClassAsset` to a live host node.
 *
 * `ClassComponent` is the first concrete runtime bridge for Conduit-backed gameplay classes.
 * It attaches to an existing node, resolves/compiles the referenced `ClassAsset`, and executes
 * the bound graph against the owning node as `self` through authored built-in and custom
 * entrypoints.
 *
 * Current design:
 * - binding is lazy and retries after earlier failures
 * - the owning node must satisfy the authored `HostType`
 * - one `GraphInstance` is retained per component, so frame slots persist across executions
 * - binding uses the process-wide default `AssetManager` resolver
 */
class ClassComponent : public BaseComponent, public ComponentCRTP<ClassComponent>
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Conduit::ClassComponent";

    TAssetRef<ClassAsset> Class{}; /**< @brief Referenced Conduit class asset to bind to the owning node. */

    void OnCreate(IWorld& WorldRef);
    void OnDestroy(IWorld& WorldRef);
    void PreTick(IWorld& WorldRef, float DeltaSeconds);
    void Tick(IWorld& WorldRef, float DeltaSeconds);
    void FixedTick(IWorld& WorldRef, float DeltaSeconds);
    void LateTick(IWorld& WorldRef, float DeltaSeconds);
    void PostTick(IWorld& WorldRef, float DeltaSeconds);
    Result ExecuteEntry(std::string_view Name);
#if defined(WITH_EDITOR) && WITH_EDITOR
    void EditorOnPropertyChanged(std::string_view Name);
#endif

    /**
     * @brief Force the component to drop any current binding and rebind from its asset reference.
     * @param WorldRef Owning world.
     * @return Success or error.
     */
    Result Rebind(IWorld& WorldRef);

    /** @brief Whether a compiled class and graph instance are currently bound. */
    [[nodiscard]] bool IsBound() const;

    /** @brief The last bind/execute failure message, or empty string when no error is present. */
    [[nodiscard]] const std::string& LastError() const;

private:
    Result EnsureBound(IWorld& WorldRef);
    Result ExecutePendingOnCreate(IWorld& WorldRef);
    Result ExecuteBuiltinEntry(IWorld& WorldRef, EBuiltinEntryPoint EntryPoint, float DeltaSeconds, bool HasDeltaSeconds);
    Result ExecuteResolvedEntry(IWorld& WorldRef, const GraphEntryPoint& EntryPoint, std::optional<float> DeltaSeconds);
    void ClearBinding();
    void RememberError(const std::string& Message);
    void LogWarningOnce(const std::string& Message);

    TAssetRef<ClassAsset> m_boundClass{}; /**< @brief Asset ref that produced the current binding. */
    std::unique_ptr<CompiledClass> m_compiledClass{}; /**< @brief Heap-stable compiled class so the graph instance can safely reference it. */
    std::unique_ptr<GraphInstance> m_instance{}; /**< @brief Live graph instance for this component. */
    std::string m_lastError{}; /**< @brief Last bind/execute failure for diagnostics. */
    bool m_pendingOnCreate = false; /**< @brief True until the class has had a chance to run its authored `OnCreate` phase. */
    bool m_bindFailureLogged = false; /**< @brief Suppress repeated identical bind warnings until state changes. */
};

} // namespace SnAPI::GameFramework::Conduit
