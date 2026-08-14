namespace GreenCake;

public struct Quaternion
{
    public float x, y, z, w;

    public Quaternion(float x, float y, float z, float w)
    {
        this.x = x;
        this.y = y;
        this.z = z;
        this.w = w;
    }

    public static Quaternion Identity => new Quaternion(0f, 0f, 0f, 1f);
}
