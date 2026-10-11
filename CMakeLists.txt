cmake_minimum_required(VERSION 4.3)
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
)
