import "MathLibrary.xi";

int main()
{
    const int answer = 40 + 2;
    const int doubled = answer * 2;
    Console.WriteLine("2 + 3 = " + add(2, 3));
    Console.WriteLine(greet("XiHash"));
    Console.WriteLine("const expression = " + doubled);
    Console.WriteLine("static calls = " + nextCallCount());
    Console.WriteLine("static calls = " + nextCallCount());
    return SUCCESS;
}
