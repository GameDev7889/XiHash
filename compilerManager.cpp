#include "Compiler.h"
#include "Parser.h"

#include <fstream>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <string>

int main(int argc, char* argv[]) {
    namespace fs = std::filesystem;
    fs::path inputPath = argc > 1 ? fs::path(argv[1]) : fs::path("XiHashTest.xi");
    std::ifstream input(inputPath);
    if (!input && argc <= 1) {
        const fs::path executablePath = fs::absolute(argv[0]);
        const fs::path besideExecutable = executablePath.parent_path() / inputPath;
        const fs::path projectRoot = executablePath.parent_path().parent_path() / inputPath;
        input.clear();
        input.open(besideExecutable);
        if (input) inputPath = besideExecutable;
        else {
            input.clear();
            input.open(projectRoot);
            if (input) inputPath = projectRoot;
        }
    }
    if (!input) {
        std::cerr << "Could not open XiHash source file: " << inputPath.string() << '\n';
        return 1;
    }

    const std::string source((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    auto nodes = Parser::parse(source);
    if (nodes.empty()) {
        if (source.empty()) std::cerr << "XiHash source file is empty: " << inputPath << '\n';
        return 1;
    }

    Compiler::compile(nodes);
    return 0;
}
