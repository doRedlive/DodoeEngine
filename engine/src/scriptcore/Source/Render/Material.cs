namespace GreenCake;

public class Material : Object
{
    internal Material(int instanceID, int generation = 1) : base(instanceID, generation) { }

    static Material()
    {
        Object.RegisterType<Material>("Material", (id, gen) => new Material(id, gen));
    }

    public static Material? Load(string path)
    {
        return Resources.Load<Material>(path);
    }
}
