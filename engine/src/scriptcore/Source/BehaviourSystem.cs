namespace GreenCake;

internal class BehaviourSystem : DoSystem
{
    public void Update()
    {
        GameObjectManager.ProcessLifecycle(Time.DeltaTime);
    }
}
