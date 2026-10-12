cmake_minimum_required(VERSION 4.3)
project(XiHash)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(XiHash
        mainCompiler.cpp
        Compiler.cpp
        Compiler.h
        Tokenizer.cpp
        Tokenizer.h
        Tokenize/TokenizeGenerall.h
)
