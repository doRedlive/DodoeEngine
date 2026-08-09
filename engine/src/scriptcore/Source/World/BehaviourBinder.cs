namespace GreenCake;

using System.Collections.Generic;

internal static class BehaviourBinder
{
    private static readonly List<CakeBehaviour> _pending = new();

    public static void Track(CakeBehaviour mb)
    {
        if (!_pending.Contains(mb))
            _pending.Add(mb);
    }

    public static void BindOrphans(Scene scene)
    {
        if (scene == null || _pending.Count == 0)
            return;

        for (int i = _pending.Count - 1; i >= 0; i--)
        {
            var mb = _pending[i];
            if (mb._destroyed || mb.GameObject != null)
            {
                _pending.RemoveAt(i);
                continue;
            }
            var go = scene.FindByID(mb.Entity.ID) ?? scene.RegisterEntity(mb.Entity.ID);
            if (go == null)
                continue;
            mb.GameObject = go;
            scene.QueueAwake(mb);
            _pending.RemoveAt(i);
        }
    }
}
