// A normal pointer is a unique owner: it frees its memory automatically.
// An `unsafe` pointer is a raw pointer: you must `delete` it yourself.
int main()
{
    int* owner = new int(42);
    Console.WriteLine("owner points to " + *owner);
    *owner = *owner + 8;
    Console.WriteLine("after update: " + *owner);

    // Ownership can only be transferred explicitly.
    int* other = move(owner);
    Console.WriteLine("owner is null: " + (owner == null));
    Console.WriteLine("other points to " + *other);

    unsafe int* raw = new int(7);
    *raw = *raw * 6;
    Console.WriteLine("raw points to " + *raw);
    delete raw;

    // Address-of always gives an unsafe pointer.
    int value = 10;
    unsafe int* alias = &value;
    *alias = 99;
    Console.WriteLine("value is now " + value);

    if (true) {
        string* name = new string("scoped");
        Console.WriteLine(*name);
    } // `name` is freed here

    return SUCCESS;
}
