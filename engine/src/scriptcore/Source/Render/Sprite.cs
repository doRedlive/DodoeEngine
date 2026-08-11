namespace GreenCake;

public class Sprite : Object
{
    internal Sprite(int instanceID, int generation = 1) : base(instanceID, generation) { }

    static Sprite()
    {
        Object.RegisterType<Sprite>("Sprite", (id, gen) => new Sprite(id, gen));
    }

    public static Sprite? Load(string path)
    {
        return Resources.Load<Sprite>(path);
    }
}
