#include <iostream>
#include <string>

// Read one console line after flushing any prompt printed without a newline.
static std::string xiReadLine() {
    std::cout.flush();
    std::string line;
    std::getline(std::cin, line);
    return line;
}

// XiHash top-level statements execute in this generated C++ entry point.
int main() {
    std::cout << "=== XiHash Console Chat ===\n";
    std::cout << "What is your name? ";
    std::string name = xiReadLine();
    std::cout << (("Hello, " + name) + "!\n");
    std::cout << "What do you think of XiHash? ";
    std::string opinion = xiReadLine();
    std::cout << (((("Thanks, " + name) + ". You said: ") + opinion) + "\n");
    std::cout.put('-');
    std::cout.put('-');
    std::cout.put('-');
    std::cout.put('\n');
    std::cout << (("Goodbye, " + name) + "!\n");
    return 0;
}
