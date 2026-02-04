#pragma once

#include "HandleFwd.h"

namespace SnAPI::GameFramework
{

class BaseNode;
class IComponent;

using NodeHandle = THandle<BaseNode>;
using ComponentHandle = THandle<IComponent>;

} // namespace SnAPI::GameFramework
