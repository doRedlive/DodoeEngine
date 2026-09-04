namespace GreenCake;

public class Prefab : Object
{
    internal Prefab(int instanceID, int generation = 1) : base(instanceID, generation) { }

    static Prefab()
    {
        Object.RegisterType<Prefab>("Prefab", (id, gen) => new Prefab(id, gen));
    }
}
