// do@Redlive

namespace GreenCake.UI;

using System;

public class UIButton : UIElement
{
    private Action _onClick;

    internal UIButton(string id, uint handle) : base(id, handle) { }

    public string Label
    {
        get => NativeCalls.Native_UIGetProperty(_handle, "label");
        set => NativeCalls.Native_UISetProperty(_handle, "label", value);
    }

    public bool Interactable
    {
        get => ToBool(NativeCalls.Native_UIGetProperty(_handle, "interactable"));
        set => NativeCalls.Native_UISetProperty(_handle, "interactable", value ? "true" : "false");
    }

    public event Action OnClick
    {
        add { _onClick += value; UIDocument.RegisterEventTarget(this); }
        remove { _onClick -= value; }
    }

    internal void PollEvents()
    {
        if (_onClick != null && NativeCalls.Native_UIPollEvent(_handle, "clicked") != 0)
            _onClick.Invoke();
    }
}
