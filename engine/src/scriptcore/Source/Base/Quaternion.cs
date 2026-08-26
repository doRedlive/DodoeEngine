namespace GreenCake;

using System;

public struct Quaternion
{
    public float x, y, z, w;

    public Quaternion(float x, float y, float z, float w)
    {
        this.x = x; this.y = y; this.z = z; this.w = w;
    }

    public static Quaternion Identity => new(0f, 0f, 0f, 1f);

    public static Quaternion Euler(float xDeg, float yDeg, float zDeg)
    {
        float x = xDeg * 0.5f * (MathF.PI / 180f);
        float y = yDeg * 0.5f * (MathF.PI / 180f);
        float z = zDeg * 0.5f * (MathF.PI / 180f);
        float cx = MathF.Cos(x), sx = MathF.Sin(x);
        float cy = MathF.Cos(y), sy = MathF.Sin(y);
        float cz = MathF.Cos(z), sz = MathF.Sin(z);
        return new Quaternion(
            sx * cy * cz - cx * sy * sz,
            cx * sy * cz + sx * cy * sz,
            cx * cy * sz - sx * sy * cz,
            cx * cy * cz + sx * sy * sz
        );
    }

    public static Quaternion Euler(Vector3f euler) => Euler(euler.x, euler.y, euler.z);
}
