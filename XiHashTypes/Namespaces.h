#ifndef XIHASH_NAMESPACES_H
#define XIHASH_NAMESPACES_H
#include <string>
#include <memory>
#include <iostream>

inline std::string xiHashDecodeEscapes(const std::string& value) {
    std::string decoded;
    decoded.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] != '\\' || i + 1 >= value.size()) {
            decoded += value[i];
            continue;
        }
        switch (value[++i]) {
            case 'n': decoded += '\n'; break;
            case 'r': decoded += '\r'; break;
            case 't': decoded += '\t'; break;
            case '\\': decoded += '\\'; break;
            case '"': decoded += '"'; break;
            case '\'': decoded += '\''; break;
            default: decoded += '\\'; decoded += value[i]; break;
        }
    }
    return decoded;
}

class ASTNode {
public:
    virtual ~ASTNode() = default;
    [[nodiscard]] virtual  std::string toCpp() const = 0;
    virtual void execute() const {}
};

class ImportNode : public ASTNode {
public:
    explicit ImportNode(std::string moduleName) : moduleName(std::move(moduleName)) {}

    [[nodiscard]] std::string toCpp() const override {
        return "#include <" + moduleName + ">";
    }
private:
    std::string moduleName;
};

class UsingNamespaceNode : public ASTNode {
public:
    explicit UsingNamespaceNode(std::string namespaceName) : namespaceName(std::move(namespaceName)) {}

    [[nodiscard]] std::string toCpp() const override {
        return "using namespace " + namespaceName + ";";
    }

private:
    std::string namespaceName;
};

#pragma region XiHash_namespace
class PrintNode : public ASTNode {
public:
    explicit PrintNode(std::string message) : message(std::move(message)) {}

    [[nodiscard]] std::string toCpp() const override {
        return "std::cout << " + message + " << std::endl;";
    }

    void execute() const override {
        if (message.size() >= 2 && message.front() == '"' && message.back() == '"')
            std::cout << xiHashDecodeEscapes(message.substr(1, message.size() - 2)) << std::endl;
    }

private:
    std::string message;
};

class PrintCNode : public ASTNode {
public:
    explicit PrintCNode(std::string message) : message(std::move(message)) {}

    [[nodiscard]] std::string toCpp() const override {
        return "std::cout << " + message + ";";
    }

    void execute() const override {
        if (message.size() >= 3 && message.front() == '\'' && message.back() == '\'')
            std::cout << xiHashDecodeEscapes(message.substr(1, message.size() - 2));
        else if (message.size() >= 3 && message.front() == '"' && message.back() == '"')
            std::cout << xiHashDecodeEscapes(message.substr(1, message.size() - 2));
    }
private:
    std::string message;
};
#pragma endregion

#endif
