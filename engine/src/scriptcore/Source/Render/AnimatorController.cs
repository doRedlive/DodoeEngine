namespace GreenCake;

public class AnimatorController : Object
{
    internal AnimatorController(int instanceID, int generation = 1) : base(instanceID, generation) { }

    static AnimatorController()
    {
        Object.RegisterType<AnimatorController>("AnimatorController", (id, gen) => new AnimatorController(id, gen));
    }

    public static AnimatorController? Load(string path)
    {
        return Resources.Load<AnimatorController>(path);
    }
}
