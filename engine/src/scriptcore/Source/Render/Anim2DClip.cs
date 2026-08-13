namespace GreenCake;

public class Anim2DClip : Object
{
    internal Anim2DClip(int instanceID, int generation = 1) : base(instanceID, generation) { }

    static Anim2DClip()
    {
        Object.RegisterType<Anim2DClip>("Anim2DClip", (id, gen) => new Anim2DClip(id, gen));
    }

    public static Anim2DClip? Load(string path)
    {
        return Resources.Load<Anim2DClip>(path);
    }
}
