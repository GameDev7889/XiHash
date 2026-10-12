//
// Created by leich on 12.10.2026.
//

#ifndef XIHASH_PARSER_H
#define XIHASH_PARSER_H

#include <vector>

#include "XiHashTypes/Namespaces.h"

class Parser {
public:
    static std::vector<std::unique_ptr<ASTNode>> parse(const std::string& fileSource);
};


#endif //XIHASH_PARSER_H
