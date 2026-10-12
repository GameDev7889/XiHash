cmake_minimum_required(VERSION 4.3)
project(XiHash)

set(CMAKE_CXX_STANDARD 20)

add_executable(XiHash compilerManager.cpp
        XiHashTypes/Namespaces.h
        Parser.cpp
        Parser.h
        Compiler.cpp
        Compiler.h
)
