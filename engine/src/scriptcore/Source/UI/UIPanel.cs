// do@Redlive

namespace Dodoe.UI;

using System;

public class UIPanel : UIElement
{
    internal UIPanel(string id, uint handle) : base(id, handle) { }

    public Color? BackgroundColor
    {
        get => ParseColor(NativeCalls.Native_UIGetProperty(_handle, "background_color"));
        set
        {
            if (value.HasValue)
                NativeCalls.Native_UISetProperty(_handle, "background_color",
                    $"{value.Value.r},{value.Value.g},{value.Value.b},{value.Value.a}");
        }
    }

    public bool ClipChildren
    {
        get => ToBool(NativeCalls.Native_UIGetProperty(_handle, "clip_children"));
        set => NativeCalls.Native_UISetProperty(_handle, "clip_children", value ? "true" : "false");
    }
}
