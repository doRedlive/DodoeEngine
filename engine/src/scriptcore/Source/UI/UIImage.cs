// do@Redlive

namespace GreenCake.UI;

using System;

public class UIImage : UIElement
{
    internal UIImage(string id, uint handle) : base(id, handle) { }

    public Color Tint
    {
        get => ParseColor(NativeCalls.Native_UIGetProperty(_handle, "color"));
        set => NativeCalls.Native_UISetProperty(_handle, "color", $"{value.r},{value.g},{value.b},{value.a}");
    }

    public bool PreserveAspect
    {
        get => ToBool(NativeCalls.Native_UIGetProperty(_handle, "preserve_aspect"));
        set => NativeCalls.Native_UISetProperty(_handle, "preserve_aspect", value ? "true" : "false");
    }

    public bool FlipH
    {
        set => NativeCalls.Native_UISetProperty(_handle, "flip_h", value ? "true" : "false");
    }

    public bool FlipV
    {
        set => NativeCalls.Native_UISetProperty(_handle, "flip_v", value ? "true" : "false");
    }

    public void SetFlip(bool horizontal, bool vertical)
    {
        NativeCalls.Native_UISetProperty(_handle, "flip", $"{(horizontal ? "true" : "false")},{(vertical ? "true" : "false")}");
    }
}
