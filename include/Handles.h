#pragma once

#include "HandleFwd.h"

namespace SnAPI::GameFramework
{

class BaseNode;
class BaseComponent;

/**
 * @brief Handle type for nodes.
 * @remarks Resolves to BaseNode instances via the global registry.
 */
using NodeHandle = THandle<BaseNode>;
/**
 * @brief Handle type for components.
 * @remarks Resolves to BaseComponent instances via the global registry.
 */
using ComponentHandle = THandle<BaseComponent>;

} // namespace SnAPI::GameFramework
