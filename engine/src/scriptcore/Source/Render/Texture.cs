namespace GreenCake;

public class Texture : Object
{
    internal Texture(int instanceID, int generation = 1) : base(instanceID, generation) { }

    static Texture()
    {
        Object.RegisterType<Texture>("Texture2D", (id, gen) => new Texture(id, gen));
    }

    public static Texture? Load(string path)
    {
        return Resources.Load<Texture>(path);
    }
}
