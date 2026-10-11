int add(int left, int right)
{
    return left + right;
}

string greet(string name)
{
    return "Hello, " + name;
}

int nextCallCount()
{
    static int callCount = 0;
    callCount = callCount + 1;
    return callCount;
}
