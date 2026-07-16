namespace GreenCake;

internal class CakeBehaviourSystem : CakeSystem
{
    public override void OnUpdate()
    {
        GameObjectManager.ProcessLifecycle(Time.DeltaTime);
    }
}
