#include <iostream>
#include <string_view>

#include "compiler/GetXiHashFile.hpp"
#include "compiler/XiHashCompiler.hpp"

namespace {
void printUsage() {
    std::cout << "Usage: XiHash [options] [source.xi]\n"
                 "Translates source.xi to C++, compiles it with the system C++ compiler, and runs it.\n"
                 "Options:\n"
                 "  --no-opt     disable optimizations\n"
                 "  --stats      print optimizer statistics\n"
                 "  --emit-cpp   only generate the C++ source; don't compile or run it\n"
                 "  --check      check XiHash syntax and types without generating or running code\n"
                 "  --keep       keep the generated .cpp/.hpp/binary after a successful run\n"
                 "  --help       show this message\n";
}
}

int main(int argc, char* argv[]) {
    CompilerOptions options;
    std::filesystem::path sourceFile = "examples/HelloWorld.xi";
    bool sourceGiven = false;

    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        if (argument == "--help") {
            printUsage();
            return 0;
        }
        if (argument == "--no-opt") {
            options.optimize = false;
        } else if (argument == "--stats") {
            options.printStats = true;
        } else if (argument == "--emit-cpp") {
            options.emitCppOnly = true;
        } else if (argument == "--check") {
            options.checkOnly = true;
        } else if (argument == "--keep") {
            options.keepGenerated = true;
        } else if (argument.starts_with("--") || sourceGiven) {
            std::cerr << "Unexpected argument '" << argument << "'\n";
            printUsage();
            return 2;
        } else {
            sourceFile = argument;
            sourceGiven = true;
        }
    }

    const auto xiHashData = GetXiHashFile::load(sourceFile);
    if (!xiHashData) {
        return 1;
    }

    XiHashCompiler compiler;
    return compiler.compile(*xiHashData, options).value_or(1);
}
