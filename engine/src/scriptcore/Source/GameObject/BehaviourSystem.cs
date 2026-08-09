namespace GreenCake;

internal class CakeBehaviourSystem : CakeSystem
{
    public override void OnUpdate()
    {
        var scene = SceneManager.ActiveScene;
        BehaviourBinder.BindOrphans(scene);
        scene?.NotifyUpdate(Time.DeltaTime);
    }
}
