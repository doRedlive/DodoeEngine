namespace GreenCake;

public class AudioClip : Object
{
    internal AudioClip(int instanceID, int generation = 1) : base(instanceID, generation) { }

    static AudioClip()
    {
        Object.RegisterType<AudioClip>("AudioClip", (id, gen) => new AudioClip(id, gen));
    }

    public static AudioClip? Load(string path)
    {
        return Resources.Load<AudioClip>(path);
    }
}
