#ifndef XIHASH_CODEGENERATOR_H
#define XIHASH_CODEGENERATOR_H

#include <string>

#include "Ast.hpp"

// Emits C++ source from an analyzed (and optionally optimized) Program. The language only ever
// has a single callable function (`main`), so only that one is translated. Safe pointers become
// `std::unique_ptr<T>`, unsafe pointers become raw `T*`, `move(p)` becomes `std::move(p)`, and
// `&&`/`||` map straight onto the native C++ operators (which already short-circuit).
//
// No extra runtime safety checks are generated (no overflow/null-deref/double-free/negative-
// unsigned guards) -- misuse becomes real C++ behavior, matching plain C++ semantics. All source-
// level mistakes (bad types, `if (1)`, a missing `;`, ...) are still caught earlier, at compile
// time, by the Parser and Analyzer.
class CodeGenerator {
public:
    struct Output {
        std::string runtimeHeader; // XiHashRuntime.hpp contents
        std::string source;        // the generated .cpp contents
    };

    Output generate(const Program& program);

private:
    static std::string genType(const Type& type);
    // Wraps `code` in a `static_cast<...>` when `type` is a numeric kind; left untouched
    // otherwise (string/bool/char/pointer/null already match exactly, per the Analyzer's rules).
    static std::string castTo(const Type& type, const std::string& code);

    static std::string genBlock(const std::vector<StmtPtr>& body, int indent);
    static std::string genStmt(const Stmt& stmt, int indent);
    static std::string genExpr(const Expr& expr);
    static std::string genBinary(const BinaryExpr& expr);
    static std::string genNew(const NewExpr& expr);
    static std::string genCall(const CallExpr& expr);
};

#endif
