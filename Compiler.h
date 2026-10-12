//
// Created by leich on 12.10.2026.
//

#ifndef XIHASH_COMPILER_H
#define XIHASH_COMPILER_H
#include <memory>
#include <vector>

#include "XiHashTypes/Namespaces.h"


class Compiler {
public:
    static void compile(const std::vector<std::unique_ptr<ASTNode>>& nodes);
};


#endif //XIHASH_COMPILER_H
