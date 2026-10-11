cmake_minimum_required(VERSION 3.25)
project(XiHash)

set(CMAKE_CXX_STANDARD 23)

add_executable(XiHash compiler.cpp
        compiler/Ast.hpp
        compiler/Lexer.cpp
        compiler/Lexer.hpp
        compiler/Parser.cpp
        compiler/Parser.hpp
        compiler/Analyzer.cpp
        compiler/Analyzer.hpp
        compiler/Optimizer.cpp
        compiler/Optimizer.hpp
        compiler/ConstEvaluator.cpp
        compiler/ConstEvaluator.hpp
        compiler/CodeGenerator.cpp
        compiler/CodeGenerator.hpp
        compiler/GetXiHashFile.cpp
        compiler/GetXiHashFile.hpp
        compiler/XiHashCompiler.cpp
        compiler/XiHashCompiler.hpp
        include/XiHash/Numerals.hpp
)

target_compile_definitions(XiHash PRIVATE XIHASH_MODULE_INCLUDE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/include")
