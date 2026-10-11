#ifndef XIHASH_XIHASHCOMPILER_H
#define XIHASH_XIHASHCOMPILER_H

#include <optional>

#include "GetXiHashFile.hpp"

struct CompilerOptions {
    bool optimize = true;
    bool printStats = false;
    bool emitCppOnly = false;  // generate the .cpp/.hpp only; don't invoke g++ or run anything
    bool checkOnly = false; // lex, parse, and analyze without generating or running code
    bool keepGenerated = false; // don't delete the generated files/binary after a successful run
};

class XiHashCompiler {
public:
    // Translates the program to C++, compiles it with the system C++ compiler, and (unless
    // emitCppOnly is set) runs the resulting binary. Returns the program's exit code, or nullopt
    // if compilation failed at any stage (a XiHash error, or the generated C++ failing to build).
    std::optional<int> compile(const XiHashData& file, const CompilerOptions& options = {});
};

#endif
