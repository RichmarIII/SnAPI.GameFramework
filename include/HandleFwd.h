#pragma once

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Forward declaration of the strong typed-handle wrapper used throughout GameFramework.
 *
 * `THandle<T>` is forward-declared here so headers can accept or store handle types
 * without pulling in the full handle implementation and all of its dependencies.
 *
 * @tparam T Object type identified by the handle.
 */
template<typename T>
struct THandle;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Forward declaration of the hash functor used for unordered containers of `THandle`.
 */
struct HandleHash;

} // namespace SnAPI::GameFramework
