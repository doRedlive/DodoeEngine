namespace GreenCake;

internal class CakeBehaviourSystem : CakeSystem
{
    public void Update()
    {
        GameObjectManager.ProcessLifecycle(Time.DeltaTime);
    }
}
