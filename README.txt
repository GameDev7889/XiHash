# XiHash

XiHash is a small, statically typed language and compiler. The `XiHash` command checks a
`.xi` source file, translates it to C++, and by default compiles and runs the result.

## Build and run

Build the compiler with CMake (CMake 4.3 or newer and a C++23-capable C++ compiler are
required):

```sh
cmake -S . -B build
cmake --build build
```

Run a program with:

```sh
./build/XiHash path/to/program.xi
```

If no path is given, XiHash looks for `examples/HelloWorld.xi` relative to the current
working directory. Source files must have the `.xi` extension. The generated C++ build
directory is `.xihash-build/<source-name>/` in the current working directory.

Command-line options:

| Option | Effect |
| --- | --- |
| `--help` | Show command usage. |
| `--check` | Lex, parse, and type-check without generating C++ or running a program. |
| `--emit-cpp` | Generate C++ and the XiHash runtime header without compiling or running. |
| `--no-opt` | Disable the default constant-folding and unreachable-code optimizations. |
| `--stats` | Print optimization counts to standard error (only when optimization is enabled). |
| `--keep` | Keep generated files and the executable after a successful run. |

The generated C++ is compiled with `g++ -std=c++23 -O2`. A successful compile runs the
program and returns its exit code. XiHash reports lexical, syntax, and type errors with
source positions; a failure to compile generated C++ is reported as a compiler error.

## Language overview

Every function has an explicit return type, a name, typed parameters, and a braced body.
The program must define exactly one function named `main`, with no parameters and an
`int` return type. Other functions may be declared and called from XiHash code. Functions
can return values, but all return statements require an expression. There is no implicit
fall-through return value.

```xi
int add(int left, int right) {
    return left + right;
}

int main() {
    int answer = add(20, 22);
    Console.WriteLine("Answer: " + answer);
    return 0;
}
```

The supported types are `int`, `uint`, `float`, `ufloat`, `double`, `udouble`, `string`,
`bool`, `char`, and `void`. `void` is for function return types, not variables or
parameters. A type can be followed by `*` to make a pointer type; see [Pointers and
ownership](#pointers-and-ownership). Type aliases can be declared at the top level:

```xi
type Count = uint;
```

Variables must be initialized when declared. `const` variables require compile-time
constant initializers and cannot be assigned to. `static` gives a local variable static
storage duration; it can be combined with `const`.

```xi
const int limit = 10;
static int calls = 0;
```

Names are case-sensitive. Identifiers start with a letter or `_` and may then contain
letters, digits, and `_`. Declarations and simple statements end in `;`.

### Statements and expressions

Supported statements are variable declarations, variable assignment, assignment through
a dereferenced pointer, function-call expression statements, `return`, `if`/`else if`/
`else`, `while`, `delete`, and console output.

```xi
if (count > 0) {
    Console.WriteLine(count);
} else {
    Console.WriteLine("empty");
}

while (count > 0) {
    count = count - 1;
}
```

Expressions include literals, variable names, function calls (`name(args)` or
`namespace.name(args)`), parentheses, unary `-` and `!`, pointer dereference `*`,
address-of `&`, and binary operators. From lowest to highest precedence, binary operators
are `||`, `&&`, `==`/`!=`, `<`/`<=`/`>`/`>=`, `+`/`-`, and `*`/`/`. Operators at each
level associate left to right. Logical operators use short-circuit evaluation.

Integer literals are decimal integers; floating literals require digits on both sides of
the decimal point. String and character literals use double and single quotes. Strings
support `\\n`, `\\r`, `\\t`, `\\\\`, and `\\"` escapes. Characters support those
appropriate escapes plus `\\0` and `\\'`, and must contain exactly one character.
Comments are `//` to end of line or `/* ... */` block comments; block comments are not
nested.

Numeric types are not freely interchangeable. Integer constants can initialize numeric
types subject to range checks. Non-constant numeric conversions require matching
signedness and conversion to a wider rank (`int`/`uint`, then `float`/`ufloat`, then
`double`/`udouble`). Floating constants can initialize floating types. `bool`, `char`,
and `string` require matching types. Conditions must be `bool`.

Arithmetic `+` concatenates when either operand is a string; the other value is converted
to text. Numeric arithmetic uses the operands' common numeric type. Two numeric constants
of different types are evaluated as `double`; a numeric constant paired with a variable
is converted to the variable's type when allowed. Equality
supports numeric values and matching `bool`, `char`, or `string` values. Pointers can be
compared for equality with a compatible pointer or `null`. Relational comparisons are
numeric (and `char`); logical operators require `bool` operands.

`Console.WriteLine(value)` prints a value followed by a newline. `Console.Write(charValue)`
prints one character without a newline. `WriteLine` displays booleans as `true`/`false`,
null pointers as `null`, and non-null pointers as addresses.

`SUCCESS` and `FAILURE` are integer constants with values `0` and `1`, respectively.

## Pointers and ownership

A pointer type without `unsafe` is an owning, unique pointer. It is represented by
`std::unique_ptr` in generated C++ and is freed automatically. It cannot be copied; use
`move(pointerVariable)` to transfer ownership, which leaves the source pointer empty.
`null` can initialize either pointer kind.

```xi
int* first = new int(42);
int* second = move(first);
Console.WriteLine(*second);
```

`unsafe` marks a raw pointer type and must be applied to a pointer, for example
`unsafe int*`. `&variable` produces an unsafe pointer. Dereference with `*pointer`;
assignment through a pointer uses `*pointer = value;`. Raw pointers are not freed
automatically: `delete pointer;` accepts only an unsafe pointer and sets that pointer to
`null` after deletion. XiHash's type checking does not prove that raw pointers are valid,
non-null, or used before deletion.

`new T(value)` allocates a `T`; its pointer ownership kind comes from the pointer type
being initialized or assigned. It is not a general expression and can only initialize a
pointer, be assigned to a pointer, or occur inside another `new` initializer.

## Imports and C/C++ interoperability

Top-level XiHash imports use a quoted source path and a semicolon:

```xi
import "helpers.xi";
```

The path is relative to the importing file. A module is included once even if imported
more than once. Circular imports are errors. Imported declarations are combined with the
importing program, so duplicate function definitions are errors.

Use `importC <header>;` to include a system C/C++ header, or
`importC "path/to/source.cpp";` to compile a C++ source file along with generated code.
Quoted source paths are relative to the importing file. `usingL <library>;` adds a linker
library (passed as `-l<library>` to `g++`).

An `extern` block describes functions supplied by an included header. Its name is a
XiHash namespace label; calls can be qualified with that label or made unqualified after
`import namespace label;`:

```xi
importC <cmath>;
extern cmath {
    double sqrt(double value);
}

int main() {
    Console.WriteLine(cmath.sqrt(9.0));
    return 0;
}
```

Extern signatures are used for type checking; XiHash does not emit declarations for
them. The namespace label is removed in generated calls, so the function name must be
visible from the included C++ headers. Extern pointer parameters and return values must
be `unsafe`, because C/C++ functions do not follow XiHash's ownership rules. XiHash
`string` arguments to extern functions are passed as `const char*` via `.c_str()`.

An opaque native pointer alias can be written as
`type Handle = external NativeHandle*;`. Such aliases represent raw pointers to external
types.

## Compiler limits

XiHash currently supports the syntax and type rules listed above. It does not provide
classes, arrays, global variables, exceptions, or a general C++ binding system. Functions
may be declared and called within the loaded XiHash source modules. C++ interop is through
included headers/source files and explicit `extern` signatures.
