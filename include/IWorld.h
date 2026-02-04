#pragma once

#include <string>

#include "Expected.h"
#include "INode.h"

namespace SnAPI::GameFramework
{

class Level;

class IWorld
{
public:
    virtual ~IWorld() = default;

    virtual void Tick(float DeltaSeconds) = 0;
    virtual void FixedTick(float DeltaSeconds) = 0;
    virtual void LateTick(float DeltaSeconds) = 0;
    virtual void EndFrame() = 0;

    virtual TExpected<NodeHandle> CreateLevel(std::string Name) = 0;
    virtual TExpectedRef<Level> LevelRef(NodeHandle Handle) = 0;
};

} // namespace SnAPI::GameFramework
