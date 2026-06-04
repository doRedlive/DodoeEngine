namespace GreenCake;

internal class MonoBehaviourSystem : DoSystem
{
    public void Update()
    {
        GameObjectManager.ProcessLifecycle(Time.DeltaTime);
    }
}
