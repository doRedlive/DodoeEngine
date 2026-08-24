namespace GreenCake;

using System;
using System.Collections.Generic;

public sealed class InputActionReference : IDisposable
{
    private readonly List<ulong> _subscriptions = new();

    public string MapName { get; }
    public string ActionName { get; }
    public uint ActionId { get; }

    public InputActionReference(string mapName, string actionName)
    {
        MapName = mapName;
        ActionName = actionName;
        ActionId = Input.FindActionId(mapName, actionName);
    }

    public InputActionReference(string qualifiedName)
    {
        ActionId = Input.FindActionId(qualifiedName);
        int separator = qualifiedName.IndexOf('/');
        if (separator >= 0)
        {
            MapName = qualifiedName.Substring(0, separator);
            ActionName = qualifiedName.Substring(separator + 1);
        }
        else
        {
            MapName = "";
            ActionName = qualifiedName;
        }
    }

    public event Action<InputActionEvent> Started;
    public event Action<InputActionEvent> Performed;
    public event Action<InputActionEvent> Canceled;

    public void Enable()
    {
        if (_subscriptions.Count > 0) return;
        if (Started != null) _subscriptions.Add(Input.Subscribe(ActionId, InputActionPhase.Started, Started));
        if (Performed != null) _subscriptions.Add(Input.Subscribe(ActionId, InputActionPhase.Performed, Performed));
        if (Canceled != null) _subscriptions.Add(Input.Subscribe(ActionId, InputActionPhase.Canceled, Canceled));
    }

    public bool IsDown => Input.IsActionDown(ActionId);
    public bool WasPressedThisFrame => Input.WasActionPressed(ActionId);
    public bool WasReleasedThisFrame => Input.WasActionReleased(ActionId);
    public float ReadValueAsAxis() => Input.GetActionAxis(ActionId);
    public Vector2f ReadValueAsVector2() => Input.GetActionVector2(ActionId);

    public void Dispose()
    {
        foreach (ulong token in _subscriptions) Input.Unsubscribe(token);
        _subscriptions.Clear();
    }
}
