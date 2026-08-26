namespace GreenCake;

using System;

public struct Vector2f
{
    public float x, y;

    public Vector2f(float scalar)
    {
        x = scalar;
        y = scalar;
    }

    public Vector2f(float x, float y)
    {
        this.x = x;
        this.y = y;
    }

    public static Vector2f Zero => new(0f, 0f);
    public static Vector2f One => new(1f, 1f);
    public static Vector2f Up => new(0f, 1f);
    public static Vector2f Down => new(0f, -1f);
    public static Vector2f Left => new(-1f, 0f);
    public static Vector2f Right => new(1f, 0f);

    public float LengthSquared => x * x + y * y;
    public float Length => MathF.Sqrt(LengthSquared);

    public Vector2f Normalized
    {
        get
        {
            float len = Length;
            if (len <= 1e-8f) return Zero;
            return new Vector2f(x / len, y / len);
        }
    }

    public void Normalize()
    {
        float len = Length;
        if (len <= 1e-8f) { x = 0; y = 0; return; }
        x /= len; y /= len;
    }

    public static float Dot(Vector2f a, Vector2f b) => a.x * b.x + a.y * b.y;
    public static float Distance(Vector2f a, Vector2f b) => (a - b).Length;

    public static Vector2f operator +(Vector2f a, Vector2f b) => new(a.x + b.x, a.y + b.y);
    public static Vector2f operator -(Vector2f a, Vector2f b) => new(a.x - b.x, a.y - b.y);
    public static Vector2f operator -(Vector2f a) => new(-a.x, -a.y);
    public static Vector2f operator *(Vector2f a, float b) => new(a.x * b, a.y * b);
    public static Vector2f operator *(float b, Vector2f a) => new(a.x * b, a.y * b);
    public static Vector2f operator /(Vector2f a, float b) => new(a.x / b, a.y / b);
}

public struct Vector2i
{
    public int x, y;

    public Vector2i(int scalar)
    {
        x = scalar;
        y = scalar;
    }

    public Vector2i(int in_x, int in_y)
    {
        x = in_x;
        y = in_y;
    }

    public static Vector2i operator +(Vector2i a, Vector2i b) => new(a.x + b.x, a.y + b.y);
    public static Vector2i operator -(Vector2i a, Vector2i b) => new(a.x - b.x, a.y - b.y);
    public static Vector2i operator *(Vector2i a, int b) => new(a.x * b, a.y * b);
}
