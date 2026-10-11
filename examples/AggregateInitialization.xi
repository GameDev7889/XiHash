struct TypeStruct {
    int integer;
    float floating;
    uint unsignedFloat;
}

int main()
{
    TypeStruct structA;
    structA.integer = 1;
    structA.floating = 1.0;
    structA.unsignedFloat = 500;

    Console.WriteLine(structA.integer);
    Console.WriteLine(structA.floating);
    Console.WriteLine(structA.unsignedFloat);
    return SUCCESS;
}
