namespace GreenCake;

internal class CakeBehaviourSystem : CakeSystem
{
    public override void OnUpdate()
    {
        var scene = SceneManager.ActiveScene;
        scene?.NotifyUpdate(Time.DeltaTime);
    }
}
