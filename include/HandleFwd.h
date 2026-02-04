#pragma once

namespace SnAPI::GameFramework
{

/**
 * @brief Strongly typed handle that stores a UUID.
 * @remarks Forward declaration for use in headers without full definition.
 */
template<typename T>
struct THandle;

/**
 * @brief Hash functor for THandle.
 * @remarks Forward declaration for unordered containers.
 */
struct HandleHash;

} // namespace SnAPI::GameFramework
