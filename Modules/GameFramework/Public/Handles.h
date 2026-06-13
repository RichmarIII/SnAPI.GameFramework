#pragma once

#include "HandleFwd.h"

namespace SnAPI::GameFramework
{

class BaseNode;
class BaseComponent;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Stable non-owning handle type for node instances.
 *
 * `NodeHandle` is the preferred way to reference nodes across frames, through
 * serialization/replication payloads, and across deferred-destroy boundaries.
 * The handle does not own the node and may outlive it.
 *
 * @see THandle
 * @see BaseNode
 */
using NodeHandle = THandle<BaseNode>;
/**
 * @ingroup SnAPI_GameFramework
 * @brief Stable non-owning handle type for component instances.
 *
 * `ComponentHandle` carries stable component identity independently of any raw pointer
 * returned by storage or lookup APIs. Use it when persisting or relaying component references.
 *
 * @see THandle
 * @see BaseComponent
 */
using ComponentHandle = THandle<BaseComponent>;

} // namespace SnAPI::GameFramework
