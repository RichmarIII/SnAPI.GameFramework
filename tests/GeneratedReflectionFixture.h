#pragma once

#include "Reflection.h"

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
 * @brief Annotated fixture type used to validate libclang-driven reflection generation.
 */
SnType(
    SnName("Generated Fixture"),
    SnCategory("Tests|Generated")
)
struct GeneratedReflectionFixture
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::GeneratedReflectionFixture";

    /** @brief Current value carried by the generated fixture. */
    SnField(
        SnName("Value"),
        SnCategory("Tests|Generated|Fields"),
        SnRep(SnUnreliable),
        SnSerialized,
        SnValue(SnMin(-16), SnMax(16), SnStep(1))
    )
    int Value = 0;

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
        SnCategory("Tests|Generated|Methods"),
        SnRpc(SnReliable, SnServer)
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
};

} // namespace SnAPI::GameFramework::Tests

namespace SnAPI::GameFramework
{

SNAPI_DEFINE_TYPE_NAME(Tests::GeneratedReflectionMode, "SnAPI::GameFramework::Tests::GeneratedReflectionMode")

} // namespace SnAPI::GameFramework
