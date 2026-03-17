#pragma once

#include <cstdint>
#include <vector>

#include "Reflection.h"
#include "GeneratedReflectionFixture.generated.hpp"

namespace SnAPI::GameFramework::Tests
{

struct UnsupportedGeneratedFieldType;
struct UnsupportedGeneratedArgType;

/**
 * @brief Example generated enum used to validate reflection codegen.
 */
SnType(
    SnName("Generated Mode"),
    SnCategory("Tests|Generated")
)
enum class GeneratedReflectionMode
{
    /** @brief Idle state for the generated enum fixture. */
    SnEnumValue(SnName("Idle"))
    Idle,
    /** @brief Active state for the generated enum fixture. */
    SnEnumValue(SnName("Active"))
    Active,
};

/**
 * @brief Annotated primary template used to validate reflected specialization expansion.
 */
SnType(
    SnName("Generated Template Box"),
    SnCategory("Tests|Generated|Template"),
    SnTemplate
)
template<typename TValue>
struct GeneratedTemplateBox
{
    /** @brief Stored label for the generated template box. */
    SnField(
        SnName("Label"),
        SnCategory("Tests|Generated|Template")
    )
    std::string Label{};

    /** @brief Read the stored label. */
    SnFunction(
        SnName("Read Label"),
        SnCategory("Tests|Generated|Template")
    )
    const std::string& ReadLabel() const
    {
        return Label;
    }
};

/**
 * @brief Annotated fixture type used to validate libclang-driven reflection generation.
 */
SnType(
    SnName("Generated Fixture"),
    SnCategory("Tests|Generated")
)
struct GeneratedReflectionFixture
{
    SnGenerated()

    /** @brief Current value carried by the generated fixture. */
    SnField(
        SnName("Value"),
        SnCategory("Tests|Generated|Fields"),
        SnRep(SnUnreliable),
        SnSerialized,
        SnAdvanced,
        SnValue(SnMin(-16), SnMax(16), SnStep(1))
    )
    int Value = 0;

    /** @brief Mutable accessor for the hidden reflected value. */
    int& EditHiddenValue()
    {
        return m_hiddenValue;
    }

    /** @brief Const accessor for the hidden reflected value. */
    const int& GetHiddenValue() const
    {
        return m_hiddenValue;
    }

    /**
     * @brief Compute a preview value derived from the current fixture state.
     * @return Twice the current stored value.
     */
    SnField(
        SnKey("PreviewValue"),
        SnName("Preview Value"),
        SnCategory("Tests|Generated|Fields"),
        SnReadOnly
    )
    int GetPreviewValue() const
    {
        return Value * 2;
    }

    /** @brief Opaque preview bytes used to validate heavy-data field metadata. */
    SnField(
        SnKey("PreviewBytes"),
        SnName("Preview Bytes"),
        SnCategory("Tests|Generated|Fields"),
        SnHeavyData
    )
    std::vector<std::uint8_t> PreviewBytes{};

    /** @brief Unsupported field that should be ignored by generated reflection. */
    SnField(
        SnName("Unsupported"),
        SnCategory("Tests|Generated|Fields")
    )
    UnsupportedGeneratedFieldType* Unsupported = nullptr;

    /**
     * @brief Add a delta to the fixture value.
     * @param Delta Signed amount to add to the current value.
     */
    SnFunction(
        SnName("Add Value"),
        SnCategory("Tests|Generated|Methods")
    )
    int AddValue(int Delta)
    {
        return Value + Delta;
    }

    /**
     * @brief Unsupported method that should be ignored by generated reflection.
     * @param Value Pointer to an unnamed test-only type.
     * @return Same pointer passed in.
     */
    SnFunction(
        SnName("Unsupported"),
        SnCategory("Tests|Generated|Methods")
    )
    UnsupportedGeneratedArgType* UnsupportedCall(UnsupportedGeneratedArgType* Value);

private:
    /** @brief Hidden value exposed through `EditHiddenValue()` / `GetHiddenValue()`. */
    SnField(
        SnKey("HiddenValue"),
        SnName("Hidden Value"),
        SnCategory("Tests|Generated|Fields"),
        SnHidden,
        SnGetter(EditHiddenValue),
        SnConstGetter(GetHiddenValue)
    )
    int m_hiddenValue = 7;
};

/**
 * @brief Host type that references one concrete template specialization.
 */
SnType(
    SnName("Generated Template Host"),
    SnCategory("Tests|Generated")
)
struct GeneratedTemplateHost
{
    SnGenerated()

    /** @brief Concrete template box instance used to force specialization generation. */
    SnField(
        SnName("Box"),
        SnCategory("Tests|Generated|Template")
    )
    GeneratedTemplateBox<GeneratedReflectionFixture> Box{};
};

/**
 * @brief Node fixture used to validate generated RPC wrapper declarations and definitions.
 */
SnType(
    SnName("Generated RPC Node"),
    SnCategory("Tests|Generated|RPC")
)
struct GeneratedRpcNode : BaseNode, NodeCRTP<GeneratedRpcNode>
{
    SnGenerated()

    GeneratedRpcNode()
    {
        TypeKey(StaticTypeId<GeneratedRpcNode>());
    }

    /** @brief Accumulated effect counter driven by generated RPC implementations. */
    SnField(
        SnName("Counter"),
        SnCategory("Tests|Generated|RPC")
    )
    int Counter = 0;

    /**
     * @brief Server-targeted generated RPC facade.
     * @param Delta Value added by `JumpImpl()`.
     */
    SnFunction(
        SnName("Jump"),
        SnCategory("Tests|Generated|RPC"),
        SnRpc(SnReliable, SnServer)
    )
    void Jump(int Delta);

    /**
     * @brief Owner-client-targeted generated RPC facade.
     * @param Delta Value added by `ShowDamageImpl()`.
     */
    SnFunction(
        SnName("Show Damage"),
        SnCategory("Tests|Generated|RPC"),
        SnRpc(SnReliable, SnClient)
    )
    void ShowDamage(int Delta);
};

} // namespace SnAPI::GameFramework::Tests

namespace SnAPI::GameFramework
{

SNAPI_DEFINE_TYPE_NAME(Tests::GeneratedReflectionMode, "SnAPI::GameFramework::Tests::GeneratedReflectionMode")

} // namespace SnAPI::GameFramework
