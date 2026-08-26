namespace GreenCake;

using System;
using System.Collections;
using System.Collections.Generic;

public class Coroutine
{
    internal IEnumerator Routine;
    internal CakeBehaviour Owner;
    internal object Current;
    internal float WaitUntilTime;
    internal bool Finished;
    internal bool Paused;
}

public abstract class YieldInstruction { }

public class WaitForSeconds : YieldInstruction
{
    public float Seconds { get; }
    public WaitForSeconds(float seconds) { Seconds = seconds; }
}

public class WaitForFixedUpdate : YieldInstruction { }

public class WaitForEndOfFrame : YieldInstruction { }

internal static class CoroutineManager
{
    private static readonly List<Coroutine> _running = new();
    private static readonly List<Coroutine> _toAdd = new();
    private static readonly HashSet<Coroutine> _toStop = new();

    public static Coroutine Start(CakeBehaviour owner, IEnumerator routine)
    {
        var c = new Coroutine { Owner = owner, Routine = routine };
        _toAdd.Add(c);
        return c;
    }

    public static void Stop(Coroutine c)
    {
        if (c != null) _toStop.Add(c);
    }

    public static void StopAll(CakeBehaviour owner)
    {
        foreach (var c in _running)
            if (c.Owner == owner) _toStop.Add(c);
        foreach (var c in _toAdd)
            if (c.Owner == owner) _toStop.Add(c);
    }

    public static void Tick(float dt)
    {
        if (_toAdd.Count > 0)
        {
            _running.AddRange(_toAdd);
            _toAdd.Clear();
        }

        for (int i = 0; i < _running.Count; i++)
        {
            var c = _running[i];
            if (_toStop.Contains(c)) continue;
            if (c.Finished) continue;
            if (c.Owner == null || c.Owner._destroyed || !c.Owner.Enabled) continue;

            if (c.Current is WaitForSeconds wfs)
            {
                c.WaitUntilTime -= dt;
                if (c.WaitUntilTime > 0f) continue;
            }
            else if (c.Current is WaitForFixedUpdate)
            {
                continue;
            }

            bool moved;
            try { moved = c.Routine.MoveNext(); }
            catch (Exception e) { Debug.LogError($"Coroutine error: {e}"); moved = false; }

            if (!moved) { c.Finished = true; continue; }

            c.Current = c.Routine.Current;
            if (c.Current is WaitForSeconds wfs2)
            {
                c.WaitUntilTime = wfs2.Seconds;
            }
        }

        _running.RemoveAll(c => c.Finished || _toStop.Contains(c));
        _toStop.Clear();
    }

    public static void TickFixed()
    {
        for (int i = 0; i < _running.Count; i++)
        {
            var c = _running[i];
            if (c.Current is WaitForFixedUpdate)
            {
                bool moved;
                try { moved = c.Routine.MoveNext(); }
                catch (Exception e) { Debug.LogError($"Coroutine error: {e}"); moved = false; }
                if (!moved) { c.Finished = true; continue; }
                c.Current = c.Routine.Current;
            }
        }
    }
}
