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

    int var = 5;
    int varSize = var->sizeOf();
    int structSize = structA->sizeOf();
    TypeStruct uninitialized;
    int uninitializedSize = uninitialized->sizeOf();

    readonly uint snapshot = structA.unsignedFloat;
    if (structA.integer == 1 && structA.unsignedFloat != 0 && 500 in [500]) {
        Console.WriteLine(snapshot);
    }

    Console.WriteLine(structA.integer);
    Console.WriteLine(structA.floating);
    Console.WriteLine(structA.unsignedFloat);
    Console.WriteLine(varSize);
    Console.WriteLine(structSize);
    Console.WriteLine(uninitializedSize);
    return SUCCESS;
}
