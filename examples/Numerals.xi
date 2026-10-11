import XiHash.Numerals;

int main()
{
    Vector2 position = Numerals.Vector2(3.0, 4.0);
    Vector2 direction = position.normalize();
    float magnitude = position.length();
    float projection = Vector2.Dot(position, direction);
    Vector2 reflected = direction.Add(position);
    reflected.xy = position.yx;

    Vector3 color = Numerals.Vector3(0.2, 0.5, 0.8);
    color.xy = position;
    float colorLength = color.length();
    float colorDot = Vector3.Dot(color, color);

    Vector3i whole = Numerals.Vector3i(1, 2, 3);
    Vector3i doubled = whole.Scale(2);

    Console.WriteLine("length = " + magnitude);
    Console.WriteLine("projection = " + projection);
    Console.WriteLine("color length = " + colorLength);
    Console.WriteLine("color dot = " + colorDot);
    Console.WriteLine("color green = " + color.y);
    Console.WriteLine("doubled x = " + doubled[0]);
    return SUCCESS;
}
