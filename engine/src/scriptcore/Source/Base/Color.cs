namespace GreenCake;

public struct Color
{
    public float r, g, b, a;

    public Color(float r, float g, float b, float a = 1.0f)
    {
        this.r = r;
        this.g = g;
        this.b = b;
        this.a = a;
    }

    public static Color White => new(1.0f, 1.0f, 1.0f, 1.0f);
    public static Color Black => new(0.0f, 0.0f, 0.0f, 1.0f);
    public static Color Red => new(1.0f, 0.0f, 0.0f, 1.0f);
    public static Color Green => new(0.0f, 1.0f, 0.0f, 1.0f);
    public static Color Blue => new(0.0f, 0.0f, 1.0f, 1.0f);
}
