#pragma once

#include <vector>

namespace SnAPI::GameFramework
{

struct TypeInfo;

namespace Benchmarks
{

struct ConduitPerfHarness
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::ConduitPerfHarness";

    int Health = 0;

    void AddHealth(int Delta);
};

struct ConduitPerfHandle
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::ConduitPerfHandle";

    int Id = 0;
};

struct HandleResolverState
{
    const TypeInfo* HarnessType = nullptr;
    std::vector<ConduitPerfHarness*> Targets{};
};

} // namespace Benchmarks

} // namespace SnAPI::GameFramework
