// Demonstrates calling into C/C++ library functions from XiHash.
usingC <cmath>;
usingC <cstdlib>;

// Signatures for the functions we want to call, grouped by header.
extern cmath {
    double sqrt(double x);
    double pow(double base, double exponent);
}

extern cstdlib {
    void srand(uint seed);
    int rand();
}

// Opens `cmath` so its functions can be called unqualified (without the `cmath.` prefix).
using namespace cmath;

int main()
{
    double root = sqrt(2.0);
    Console.WriteLine("sqrt(2.0) = " + root);

    double cube = cmath.pow(2.0, 3.0);
    Console.WriteLine("2.0 ^ 3.0 = " + cube);

    cstdlib.srand(1);
    int first = cstdlib.rand();
    cstdlib.srand(1);
    int second = cstdlib.rand();
    Console.WriteLine("rand() is deterministic with the same seed: " + (first == second));

    return SUCCESS;
}
