namespace GreenCake;

public class TextureCubemap : Object
{
    internal TextureCubemap(int instanceID, int generation = 1) : base(instanceID, generation) { }

    static TextureCubemap()
    {
        Object.RegisterType<TextureCubemap>("TextureCubemap", (id, gen) => new TextureCubemap(id, gen));
    }
}
