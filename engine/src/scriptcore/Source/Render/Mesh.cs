namespace GreenCake;

public class Mesh : Object
{
    internal Mesh(int instanceID, int generation = 1) : base(instanceID, generation) { }

    static Mesh()
    {
        Object.RegisterType<Mesh>("Mesh", (id, gen) => new Mesh(id, gen));
    }

    public static Mesh? Load(string path)
    {
        return Resources.Load<Mesh>(path);
    }
}
