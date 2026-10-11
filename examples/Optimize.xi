int main()
{
    int seconds = 60 * 60 * 24;
    string greeting = "Hello, " + "optimizer" + "!";
    Console.WriteLine(greeting);
    Console.WriteLine("Seconds per day: " + seconds);

    if (2 > 1) {
        Console.WriteLine("always runs");
    } else {
        Console.WriteLine("never runs");
    }

    bool debug = false;
    if (true || debug) {
        Console.WriteLine("short-circuited");
    }

    return SUCCESS;
    Console.WriteLine("unreachable");
}
