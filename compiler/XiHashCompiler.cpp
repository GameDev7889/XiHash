#include "XiHashCompiler.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_set>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#endif

#include "Analyzer.hpp"
#include "CodeGenerator.hpp"
#include "Lexer.hpp"
#include "Optimizer.hpp"
#include "Parser.hpp"

namespace fs = std::filesystem;

namespace {
void writeFile(const fs::path& path, const std::string& contents) {
    std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
    file << contents;
}

// Runs a shell command and returns the program's exit code (or 128+signal if it was killed by
// one, or -1 if the command itself could not be run).
int runCommand(const std::string& command) {
    const int status = std::system(command.c_str());
    if (status == -1) {
        return -1;
    }
#if defined(__unix__) || defined(__APPLE__)
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return -1;
#else
    return status;
#endif
}

std::string quoted(const fs::path& path) { return "\"" + path.string() + "\""; }

void appendModule(Program& target, Program source) {
    target.cHeaders.insert(target.cHeaders.end(), source.cHeaders.begin(), source.cHeaders.end());
    target.linkLibraries.insert(target.linkLibraries.end(), source.linkLibraries.begin(), source.linkLibraries.end());
    target.externBlocks.insert(target.externBlocks.end(),
                               std::make_move_iterator(source.externBlocks.begin()),
                               std::make_move_iterator(source.externBlocks.end()));
    target.openedNamespaces.insert(target.openedNamespaces.end(), source.openedNamespaces.begin(),
                                   source.openedNamespaces.end());
    target.functions.insert(target.functions.end(), std::make_move_iterator(source.functions.begin()),
                            std::make_move_iterator(source.functions.end()));
}

Program loadModule(const fs::path& path, const std::string& contents,
                   std::unordered_set<fs::path>& loaded, std::unordered_set<fs::path>& active) {
    std::error_code ec;
    fs::path normalized = fs::weakly_canonical(path, ec);
    if (ec) {
        normalized = fs::absolute(path).lexically_normal();
    }
    if (active.contains(normalized)) {
        throw XiHashError({1, 1}, "Circular XiHash import involving '" + path.string() + "'");
    }
    if (!loaded.insert(normalized).second) {
        return {};
    }
    active.insert(normalized);

    Program own;
    try {
        own = Parser(lex(contents)).parseProgram();
    } catch (const XiHashError& error) {
        throw XiHashError(error.position, error.what(), path.string());
    }

    Program combined;
    for (const auto& [importPath, position] : own.imports) {
        const fs::path importedPath = path.parent_path() / importPath;
        std::ifstream input(importedPath, std::ios::in | std::ios::binary);
        if (!input) {
            throw XiHashError(position, "Could not open imported XiHash file '" + importedPath.string() + "'",
                              path.string());
        }
        std::ostringstream buffer;
        buffer << input.rdbuf();
        try {
            appendModule(combined, loadModule(importedPath, buffer.str(), loaded, active));
        } catch (const XiHashError& error) {
            if (!error.sourceFile.empty()) {
                throw;
            }
            throw XiHashError(error.position, error.what(), path.string());
        }
    }
    appendModule(combined, std::move(own));
    active.erase(normalized);
    return combined;
}
}

std::optional<int> XiHashCompiler::compile(const XiHashData& file, const CompilerOptions& options) {
    Program program;
    try {
        std::unordered_set<fs::path> loaded;
        std::unordered_set<fs::path> active;
        const fs::path sourcePath = file.filePath.empty() ? fs::path(file.fileName) : file.filePath;
        program = loadModule(sourcePath, file.fileContent, loaded, active);
        Analyzer().analyze(program);
        if (options.checkOnly) {
            return 0;
        }
        if (options.optimize) {
            const OptimizerStats stats = Optimizer().optimize(program);
            if (options.printStats) {
                std::cerr << "optimizer: " << stats.foldedExpressions << " expressions folded, "
                          << stats.removedBranches << " branches removed, "
                          << stats.removedStatements << " unreachable statements removed\n";
            }
        }
    } catch (const XiHashError& error) {
        std::cerr << (error.sourceFile.empty()
                          ? (file.filePath.empty() ? file.fileName : file.filePath.string())
                          : error.sourceFile)
                  << ':' << error.position.line << ':' << error.position.column
                  << ": error: " << error.what() << '\n';
        return std::nullopt;
    }

    const CodeGenerator::Output generated = CodeGenerator().generate(program);

    const std::string stem = fs::path(file.fileName).stem().string();
    const fs::path buildDir = fs::absolute(fs::path(".xihash-build") / stem);
    std::error_code ec;
    fs::create_directories(buildDir, ec);
    if (ec) {
        std::cerr << "error: could not create build directory " << buildDir << ": " << ec.message() << '\n';
        return std::nullopt;
    }

    const fs::path headerPath = buildDir / "XiHashRuntime.hpp";
    const fs::path sourcePath = buildDir / (stem + ".cpp");
    const fs::path binaryPath = buildDir / stem;

    writeFile(headerPath, generated.runtimeHeader);
    writeFile(sourcePath, generated.source);

    if (options.emitCppOnly) {
        std::cout << "Generated " << sourcePath << " (using " << headerPath << ")\n";
        return 0;
    }

    std::ostringstream compileCommand;
    compileCommand << "g++ -std=c++23 -O2 -o " << quoted(binaryPath) << ' ' << quoted(sourcePath);
    for (const auto& library : program.linkLibraries) {
        compileCommand << " -l" << library;
    }
    if (runCommand(compileCommand.str()) != 0) {
        std::cerr << "error: the generated C++ failed to compile; this is a XiHash compiler bug. "
                     "Inspect the generated files in " << buildDir << '\n';
        return std::nullopt;
    }

    const int exitCode = runCommand(quoted(binaryPath));

    if (!options.keepGenerated) {
        fs::remove_all(buildDir, ec);
    }

    return exitCode;
}
