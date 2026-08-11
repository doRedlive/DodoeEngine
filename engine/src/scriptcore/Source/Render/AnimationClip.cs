namespace GreenCake;

public class AnimationClip : Object
{
    internal AnimationClip(int instanceID, int generation = 1) : base(instanceID, generation) { }

    static AnimationClip()
    {
        Object.RegisterType<AnimationClip>("AnimationClip", (id, gen) => new AnimationClip(id, gen));
    }

    public static AnimationClip? Load(string path)
    {
        return Resources.Load<AnimationClip>(path);
    }
}
