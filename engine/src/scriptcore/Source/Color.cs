namespace GreenCake;

public struct Color
{
    public float R, G, B, A;

    public Color(float r, float g, float b, float a = 1.0f)
    {
        R = r;
        G = g;
        B = b;
        A = a;
    }

    public static Color White => new(1.0f, 1.0f, 1.0f, 1.0f);
    public static Color Black => new(0.0f, 0.0f, 0.0f, 1.0f);
    public static Color Red => new(1.0f, 0.0f, 0.0f, 1.0f);
    public static Color Green => new(0.0f, 1.0f, 0.0f, 1.0f);
    public static Color Blue => new(0.0f, 0.0f, 1.0f, 1.0f);
}
