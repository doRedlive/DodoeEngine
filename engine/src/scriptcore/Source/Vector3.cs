namespace GreenCake;

using System;

public struct Vector3f
{
    public float X, Y, Z;

    public Vector3f(float scalar)
    {
        X = scalar;
        Y = scalar;
        Z = scalar;
    }

    public Vector3f(float x, float y, float z)
    {
        X = x;
        Y = y;
        Z = z;
    }

    public static Vector3f operator +(Vector3f a, Vector3f b)
    {
        return new Vector3f(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
    }

    public static Vector3f operator *(Vector3f a, float b)
    {
        return new Vector3f(a.X * b, a.Y * b, a.Z * b);
    }
}
