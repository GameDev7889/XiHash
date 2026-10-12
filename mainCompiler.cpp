#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <system_error>

#include "Compiler.h"

namespace {

// Show how to invoke the XiHash-to-C++ command-line compiler.
void printUsage(const char* executable) {
    std::cerr << "Usage: " << executable << " [input.xi] [-o <output.cpp>]\n"
              << "With no input, XiHashTest.xi is searched for in the current directory and its parents.\n"
              << "If -o is omitted, the output uses the input filename with a .cpp extension.\n";
}

// Search a directory and each parent for the example source used by a no-argument launch.
std::filesystem::path findExampleSourceInParents(std::filesystem::path directory) {
    std::error_code error;
    directory = std::filesystem::absolute(directory, error);
    if (error) {
        return {};
    }

    while (!directory.empty()) {
        const std::filesystem::path candidate = directory / "XiHashTest.xi";
        error.clear();
        if (std::filesystem::is_regular_file(candidate, error)) {
            return candidate;
        }

        const std::filesystem::path parent = directory.parent_path();
        if (parent == directory) {
            break;
        }
        directory = parent;
    }
    return {};
}

// Locate the sample when CLion starts XiHash.exe without command-line arguments.
std::filesystem::path findDefaultSource(const char* executable) {
    std::error_code error;
    const std::filesystem::path workingDirectory = std::filesystem::current_path(error);
    if (!error) {
        const std::filesystem::path fromWorkingDirectory = findExampleSourceInParents(workingDirectory);
        if (!fromWorkingDirectory.empty()) {
            return fromWorkingDirectory;
        }
    }

    const std::filesystem::path executablePath = std::filesystem::absolute(executable, error);
    if (!error) {
        const std::filesystem::path fromExecutable = findExampleSourceInParents(executablePath.parent_path());
        if (!fromExecutable.empty()) {
            return fromExecutable;
        }
    }

    // Keep a useful path for the normal input-file error if the example was not found.
    return "XiHashTest.xi";
}

} // namespace

int main(int argc, char* argv[]) {
    // Default to the sample for a convenient no-argument CLion launch.
    const std::filesystem::path inputPath = argc == 1 ? findDefaultSource(argv[0]) : argv[1];
    std::filesystem::path outputPath;
    if (argc == 1 || argc == 2) {
        outputPath = inputPath;
        outputPath.replace_extension(".cpp");
    } else if (argc == 4 && std::string(argv[2]) == "-o") {
        outputPath = argv[3];
    } else {
        printUsage(argv[0]);
        return 2;
    }

    try {
        // Read the entire XiHash file because tokenization operates on source text.
        std::ifstream input(inputPath, std::ios::binary);
        if (!input) {
            throw std::runtime_error("cannot open input file '" + inputPath.string() + "'");
        }
        const std::string source((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

        // Transpile the source and write the generated C++ to the requested path.
        const std::string cpp = compileToCpp(source);
        std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
        if (!output) {
            throw std::runtime_error("cannot open output file '" + outputPath.string() + "'");
        }
        output << cpp;
        if (!output) {
            throw std::runtime_error("failed while writing output file '" + outputPath.string() + "'");
        }

        std::cout << "Generated C++: " << outputPath.string() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "XiHash compiler error: " << error.what() << '\n';
        return 1;
    }
}
