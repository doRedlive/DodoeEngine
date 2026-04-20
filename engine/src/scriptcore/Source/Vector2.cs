namespace GreenCake;

using System;

public struct Vector2f
{
    public float X, Y;

    public Vector2f(float scalar)
    {
        X = scalar;
        Y = scalar;
    }

    public Vector2f(float x, float y)
    {
        X = x;
        Y = y;
    }

    public static Vector2f operator +(Vector2f a, Vector2f b)
    {
        return new Vector2f(a.X + b.X, a.Y + b.Y);
    }

    public static Vector2f operator *(Vector2f a, float b)
    {
        return new Vector2f(a.X * b, a.Y * b);
    }
}
