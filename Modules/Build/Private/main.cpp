#include "BuildCli.h"

#include <iostream>
#include <string>
#include <vector>

int main(const int argc, char** argv)
{
    using namespace SnAPI::GameFramework;

    std::vector<std::string> Arguments{};
    Arguments.reserve(argc > 0 ? static_cast<std::size_t>(argc - 1) : 0u);
    for (int Index = 1; Index < argc; ++Index)
    {
        Arguments.emplace_back(argv[Index]);
    }

    const BuildCliResult Result = BuildCliService::Run(Arguments);
    if (!Result.StandardOutput.empty())
    {
        std::cout << Result.StandardOutput;
        if (Result.StandardOutput.back() != '\n')
        {
            std::cout << '\n';
        }
    }
    if (!Result.StandardError.empty())
    {
        std::cerr << Result.StandardError;
        if (Result.StandardError.back() != '\n')
        {
            std::cerr << '\n';
        }
    }

    return static_cast<int>(Result.ExitCode);
}
