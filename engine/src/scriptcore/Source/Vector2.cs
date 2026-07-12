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

    public static Vector2f operator +(Vector2f a, Vector2f b)
    {
        return new Vector2f(a.x + b.x, a.y + b.y);
    }

    public static Vector2f operator *(Vector2f a, float b)
    {
        return new Vector2f(a.x * b, a.y * b);
    }
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

    public static Vector2i operator +(Vector2i a, Vector2i b)
    {
        return new Vector2i(a.x + b.x, a.y + b.y);
    }

    public static Vector2i operator *(Vector2i a, int b)
    {
        return new Vector2i(a.x * b, a.y * b);
    }
}
