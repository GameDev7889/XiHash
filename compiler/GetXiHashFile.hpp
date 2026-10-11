#ifndef XIHASH_GETXIHASHFILE_H
#define XIHASH_GETXIHASHFILE_H
#include <string>
#include <optional>
#include <filesystem>

struct XiHashData {
    std::string fileName;
    std::string fileContent;
    std::filesystem::path filePath;
};

class GetXiHashFile {
public:
    static std::optional<XiHashData> load(const std::filesystem::path& filepath);
};


#endif //XIHASH_GETXIHASHFILE_H
