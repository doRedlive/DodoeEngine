namespace GreenCake;

using System;

public struct Vector3f
{
    public float x, y, z;

    public Vector3f(float scalar)
    {
        x = scalar;
        y = scalar;
        z = scalar;
    }

    public Vector3f(float x, float y, float z)
    {
        this.x = x;
        this.y = y;
        this.z = z;
    }

    public static Vector3f operator +(Vector3f a, Vector3f b)
    {
        return new Vector3f(a.x + b.x, a.y + b.y, a.z + b.z);
    }

    public static Vector3f operator *(Vector3f a, float b)
    {
        return new Vector3f(a.x * b, a.y * b, a.z * b);
    }
}

public struct Vector3i
{
    public int x, y, z;

    public Vector3i(int scalar)
    {
        x = scalar;
        y = scalar;
        z = scalar;
    }

    public Vector3i(int x, int y, int z)
    {
        this.x = x;
        this.y = y;
        this.z = z;
    }

    public static Vector3i operator +(Vector3i a, Vector3i b)
    {
        return new Vector3i(a.x + b.x, a.y + b.y, a.z + b.z);
    }

    public static Vector3i operator *(Vector3i a, int b)
    {
        return new Vector3i(a.x * b, a.y * b, a.z * b);
    }
}
