#include "GetXiHashFile.hpp"

#include <fstream>
#include <iostream>

std::optional<XiHashData> GetXiHashFile::load(const std::filesystem::path& filepath) {
    if (!std::filesystem::exists(filepath)) {
        std::cerr << "Error: File '" << filepath << "' does not exist.\n";
        return std::nullopt;
    }

    if (filepath.extension() != ".xi") {
        std::cerr << "Error: Expected a .xi source file, got '" << filepath.extension() << "'\n";
        return std::nullopt;
    }

    std::ifstream file(filepath, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Failed to open file '" << filepath << "'.\n";
        return std::nullopt;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();

    return XiHashData{
        .fileName = filepath.filename().string(),
        .fileContent = buffer.str(),
        .filePath = std::filesystem::absolute(filepath)
    };
}