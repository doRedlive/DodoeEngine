namespace GreenCake;

using System;
using System.Collections.Generic;

internal static class InputEventHub
{
    private sealed class Entry
    {
        public uint ActionId;
        public InputActionPhase Phase;
        public Action<InputActionEvent> Callback;
    }

    private static readonly Dictionary<ulong, Entry> ByToken = new();
    private static readonly Dictionary<uint, List<Entry>> ByAction = new();

    public static void Register(ulong token, uint actionId, InputActionPhase phase, Action<InputActionEvent> callback)
    {
        if (token == 0) return;
        var entry = new Entry { ActionId = actionId, Phase = phase, Callback = callback };
        ByToken[token] = entry;
        if (actionId != 0) AddToAction(actionId, entry);
    }

    public static void Unsubscribe(ulong token)
    {
        if (!ByToken.Remove(token, out var entry)) return;
        if (entry.ActionId == 0) return;
        if (ByAction.TryGetValue(entry.ActionId, out var list))
        {
            list.RemoveAll(e => ReferenceEquals(e, entry));
            if (list.Count == 0) ByAction.Remove(entry.ActionId);
        }
    }

    private static void AddToAction(uint actionId, Entry entry)
    {
        if (!ByAction.TryGetValue(actionId, out var list))
        {
            list = new List<Entry>();
            ByAction[actionId] = list;
        }
        list.Add(entry);
    }

    public static void Dispatch(uint actionId, int phase, int valueType, int boolValue, float v0, float v1)
    {
        if (!ByAction.TryGetValue(actionId, out var list)) return;
        var ev = new InputActionEvent
        {
            ActionId = actionId,
            Phase = (InputActionPhase)phase,
            ValueType = (InputActionValueType)valueType,
            BoolValue = boolValue != 0,
            FloatValue = v0,
            Vector2Value = new Vector2f(v0, v1),
        };
        for (int i = list.Count - 1; i >= 0; i--)
        {
            var entry = list[i];
            if (entry.Phase == ev.Phase) entry.Callback(ev);
        }
    }
}
