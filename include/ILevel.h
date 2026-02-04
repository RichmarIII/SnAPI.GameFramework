#pragma once

#include <functional>
#include <memory>
#include <string>

#include "Expected.h"
#include "Handle.h"

namespace SnAPI::GameFramework
{

class BaseNode;
class NodeGraph;

using NodeHandle = THandle<BaseNode>;

class ILevel
{
public:
    virtual ~ILevel() = default;

    virtual void Tick(float DeltaSeconds) = 0;
    virtual void FixedTick(float DeltaSeconds) = 0;
    virtual void LateTick(float DeltaSeconds) = 0;
    virtual void EndFrame() = 0;

    virtual TExpected<NodeHandle> CreateGraph(std::string Name) = 0;
    virtual TExpectedRef<NodeGraph> Graph(NodeHandle Handle) = 0;
};

} // namespace SnAPI::GameFramework
