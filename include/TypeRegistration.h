#pragma once

namespace SnAPI::GameFramework
{

/**
 * @brief Register built-in types and default serializers.
 * @remarks Must be called once at startup before using reflection/serialization.
 * @note Safe to call multiple times; duplicate registrations are ignored or fail gracefully.
 */
void RegisterBuiltinTypes();

} // namespace SnAPI::GameFramework
