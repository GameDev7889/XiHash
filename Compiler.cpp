//
// Created by leich on 12.10.2026.
//

#include "Compiler.h"

void Compiler::compile(const std::vector<std::unique_ptr<ASTNode>>& nodes) {
    for (const auto& node : nodes) {
        if (node) node->execute();
    }
}
