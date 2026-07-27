namespace GreenCake;

public class Sprite : Object
{
    internal Sprite(int instanceID) : base(instanceID) { }

    public static Sprite? Load(string path)
    {
        return Resources.Load<Sprite>(path);
    }
}
