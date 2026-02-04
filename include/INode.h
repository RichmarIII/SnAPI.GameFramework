#pragma once

#include <string>
#include <vector>

#include "Handles.h"

namespace SnAPI::GameFramework
{

class BaseNode;

class INode
{
public:
    virtual ~INode() = default;

    virtual void Tick(float DeltaSeconds) { (void)DeltaSeconds; }
    virtual void FixedTick(float DeltaSeconds) { (void)DeltaSeconds; }
    virtual void LateTick(float DeltaSeconds) { (void)DeltaSeconds; }

    virtual const std::string& Name() const = 0;
    virtual void Name(std::string Name) = 0;

    virtual NodeHandle Handle() const = 0;
};

} // namespace SnAPI::GameFramework
