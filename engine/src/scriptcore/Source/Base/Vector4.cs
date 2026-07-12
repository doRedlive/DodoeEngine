namespace GreenCake;

using System;

public struct Vector4f
{
    public float x, y, z, w;

    public Vector4f(float scalar)
    {
        x = scalar;
        y = scalar;
        z = scalar;
        w = scalar;
    }

    public Vector4f(float x, float y, float z, float w)
    {
        this.x = x;
        this.y = y;
        this.z = z;
        this.w = w;
    }

    public static Vector4f operator +(Vector4f a, Vector4f b)
    {
        return new Vector4f(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
    }

    public static Vector4f operator *(Vector4f a, float b)
    {
        return new Vector4f(a.x * b, a.y * b, a.z * b, a.w * b);
    }
}

public struct Vector4i
{
    public int x, y, z, w;

    public Vector4i(int scalar)
    {
        x = scalar;
        y = scalar;
        z = scalar;
        w = scalar;
    }

    public Vector4i(int x, int y, int z, int w)
    {
        this.x = x;
        this.y = y;
        this.z = z;
        this.w = w;
    }

    public static Vector4i operator +(Vector4i a, Vector4i b)
    {
        return new Vector4i(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
    }

    public static Vector4i operator *(Vector4i a, int b)
    {
        return new Vector4i(a.x * b, a.y * b, a.z * b, a.w * b);
    }
}
