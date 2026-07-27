// do@Redlive

namespace GreenCake.UI;

using System;

public class UILabel : UIElement
{
    internal UILabel(string id, uint handle) : base(id, handle) { }

    public string Text
    {
        get => NativeCalls.Native_UIGetProperty(_handle, "text");
        set => NativeCalls.Native_UISetProperty(_handle, "text", value);
    }

    public int FontSize
    {
        get => int.TryParse(NativeCalls.Native_UIGetProperty(_handle, "font_size"), out var v) ? v : 16;
        set => NativeCalls.Native_UISetProperty(_handle, "font_size", value.ToString());
    }

    public Color Color
    {
        get => ParseColor(NativeCalls.Native_UIGetProperty(_handle, "color"));
        set => NativeCalls.Native_UISetProperty(_handle, "color", $"{value.r},{value.g},{value.b},{value.a}");
    }
}
