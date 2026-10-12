#ifndef XIHASH_COMPILER_H
#define XIHASH_COMPILER_H

#include <string>
#include <string_view>

// Translate the supported XiHash source subset into a standalone C++ program.
std::string compileToCpp(std::string_view source);

#endif // XIHASH_COMPILER_H
