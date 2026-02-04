#pragma once

#include "HandleFwd.h"

namespace SnAPI::GameFramework
{

class BaseNode;
class IComponent;

/**
 * @brief Handle type for nodes.
 * @remarks Resolves to BaseNode instances via the global registry.
 */
using NodeHandle = THandle<BaseNode>;
/**
 * @brief Handle type for components.
 * @remarks Resolves to IComponent instances via the global registry.
 */
using ComponentHandle = THandle<IComponent>;

} // namespace SnAPI::GameFramework
