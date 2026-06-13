#pragma once

#include "Editor/EditorExport.h"
#include "Conduit/Asset.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace SnAPI::GameFramework::Conduit::Editor
{

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief Authoring-level node family exposed by the Conduit schema.
 */
enum class ESchemaNodeFamily : std::uint8_t
{
    EntryPoint = 0,
    Variable,
    ControlFlow,
    Constant,
    Intrinsic,
    FieldRead,
    FieldWrite,
    MethodCall,
};

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief Direction of one authored node pin.
 */
enum class ESchemaPinDirection : std::uint8_t
{
    Input = 0,
    Output,
};

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief Type contract for one authored node pin.
 *
 * For handle pins, `Type` describes the expected resolved target type rather than the concrete
 * runtime handle payload type.
 */
struct SchemaPinType
{
    TypeId Type{}; /**< @brief Reflected value or expected target type. Empty means polymorphic or editor-specialized. */
    ESlotKind Kind = ESlotKind::Value; /**< @brief Value vs handle interpretation. */
    bool IsExec = false; /**< @brief True when the pin represents control-flow rather than data. */
    bool IsPolymorphic = false; /**< @brief True when the final concrete type is inferred or authored later. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief One authored node pin descriptor surfaced by the schema.
 */
struct SchemaPinDescriptor
{
    std::string Name{}; /**< @brief UI-facing pin label. */
    std::string Tooltip{}; /**< @brief Hover tooltip describing pin semantics, type contract, and reflected docs. */
    ESchemaPinDirection Direction = ESchemaPinDirection::Input; /**< @brief Input vs output. */
    SchemaPinType Type{}; /**< @brief Type and semantic contract. */
    bool SupportsLiteral = false; /**< @brief True when unlinked pins may expose inline literal editors. */
    bool IsAdvanced = false; /**< @brief True when the pin should default to a collapsed/advanced section. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief One authoring-time node template descriptor.
 */
struct SchemaNodeDescriptor
{
    std::string StableId{}; /**< @brief Stable schema/template id for search, commands, and clipboard payloads. */
    std::string DisplayName{}; /**< @brief UI-facing title. */
    std::string Category{}; /**< @brief Palette/search category path. */
    std::string Tooltip{}; /**< @brief Short explanatory tooltip. */
    ESchemaNodeFamily Family = ESchemaNodeFamily::Intrinsic; /**< @brief High-level behavior family. */
    bool IsPure = false; /**< @brief True when the node has no exec pins and no side effects by contract. */
    bool RequiresSpecialization = false; /**< @brief True when concrete types or lowering data are authored after spawn. */
    TypeId OwnerType{}; /**< @brief Reflected owner type for member-access nodes. */
    std::string MemberName{}; /**< @brief Reflected field or method name for member-access nodes. */
    Uuid VariableId{}; /**< @brief Authored graph-variable id for variable get/set nodes. */
    EBuiltinEntryPoint BuiltinEntryPoint = EBuiltinEntryPoint::None; /**< @brief Built-in lifecycle entry for `EntryPoint` nodes. */
    EUnaryIntrinsicOp UnaryOp = EUnaryIntrinsicOp::LogicalNot; /**< @brief Unary intrinsic opcode for `UnaryIntrinsic` nodes. */
    EBinaryIntrinsicOp BinaryOp = EBinaryIntrinsicOp::Add; /**< @brief Binary intrinsic opcode for `BinaryIntrinsic` nodes. */
    std::optional<EGraphAssetNodeKind> LoweredKind{}; /**< @brief Direct low-level asset opcode when available. */
    std::vector<SchemaPinDescriptor> Pins{}; /**< @brief Ordered pin descriptors. */
};

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief Reflection-backed schema source for Conduit graph authoring.
 *
 * The registry owns the builtin node catalog and can expand reflected fields/methods into
 * authoring templates for one self or instance-owner type. It intentionally describes only
 * what the current runtime can actually compile and execute.
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API SchemaRegistry
{
public:
    /** @brief Rebuild the builtin Conduit node catalog. */
    void RebuildBuiltins();
    /** @brief Access the builtin node catalog. */
    [[nodiscard]] const std::vector<SchemaNodeDescriptor>& Builtins() const { return m_builtins; }

    /**
     * @brief Describe reflected nodes available against one self type.
     * @param SelfType Reflected self type.
     * @return Field/method node descriptors.
     */
    [[nodiscard]] std::vector<SchemaNodeDescriptor> DescribeSelf(const TypeInfo& SelfType) const;
    /**
     * @brief Describe authored graph-variable nodes available for one graph asset.
     * @param Asset Authored graph payload.
     * @return Variable get/set node descriptors.
     */
    [[nodiscard]] std::vector<SchemaNodeDescriptor> DescribeVariables(const GraphAsset& Asset) const;

    /**
     * @brief Describe reflected nodes available against one handle-resolved owner type.
     * @param OwnerType Reflected owner type.
     * @return Field/method node descriptors.
     */
    [[nodiscard]] std::vector<SchemaNodeDescriptor> DescribeInstance(const TypeInfo& OwnerType) const;

private:
    std::vector<SchemaNodeDescriptor> m_builtins{};
};

} // namespace SnAPI::GameFramework::Conduit::Editor
