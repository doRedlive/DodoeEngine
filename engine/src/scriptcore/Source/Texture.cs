namespace GreenCake;

public class Texture : Object
{
    internal Texture(int instanceID) : base(instanceID) { }

    public static Texture? Load(string path)
    {
        return Resources.Load<Texture>(path);
    }
}
