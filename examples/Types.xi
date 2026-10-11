using XiHash.Console;

int main()
{
    int a = 7;
    uint b = 3;
    float f = 1.5;
    double d = 2.25 * 2;
    ufloat uf = 0.1;
    udouble ud = 10 / 4.0;
    string name = "Xi";

    Console.WriteLine("Hello, " + name + "!");
    Console.WriteLine(a / 2);
    Console.WriteLine(b * 5);
    Console.WriteLine(f + a);
    Console.WriteLine(d);
    Console.WriteLine(uf);
    Console.WriteLine(ud);

    Console.WriteLine("I love XiHash!");

    a = a - 10;
    Console.WriteLine("a = " + a);
    return SUCCESS;
}
