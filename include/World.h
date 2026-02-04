#pragma once

#include <string>
#include <vector>

#include "IWorld.h"
#include "JobSystem.h"
#include "Level.h"
#include "NodeGraph.h"

namespace SnAPI::GameFramework
{

class World : public NodeGraph, public IWorld
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::World";

    World();
    explicit World(std::string Name);

    void Tick(float DeltaSeconds) override;
    void FixedTick(float DeltaSeconds) override;
    void LateTick(float DeltaSeconds) override;
    void EndFrame() override;

    TExpected<NodeHandle> CreateLevel(std::string Name) override;
    TExpectedRef<Level> LevelRef(NodeHandle Handle) override;
    std::vector<NodeHandle> Levels() const;
    JobSystem& Jobs();

private:
    JobSystem m_jobSystem{};
};

} // namespace SnAPI::GameFramework
