int main()
{
    char first = 'X';
    char second = 'i';
    Console.Write(first);
    Console.Write(second);
    Console.Write('\n');
    Console.WriteLine("Letters: " + first + second + '!');
    if (first < second) {
        Console.WriteLine("'X' sorts before 'i'");
    }
    return SUCCESS;
}
