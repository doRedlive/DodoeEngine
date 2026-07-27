// do@Redlive

namespace Dodoe.UI;

using System;

public class UIElement
{
    internal uint _handle;
    internal string _id;

    internal UIElement(string id, uint handle)
    {
        _id = id;
        _handle = handle;
    }

    public string Id => _id;

    public bool Visible
    {
        get => NativeCalls.Native_UIGetProperty(_handle, "visible");
        set => NativeCalls.Native_UISetProperty(_handle, "visible", value ? "true" : "false");
    }

    public float Depth
    {
        get => NativeCalls.Native_UIGetProperty(_handle, "depth");
        set => NativeCalls.Native_UISetProperty(_handle, "depth", value.ToString());
    }

    public Vector2f Position
    {
        get => ParseVector2(NativeCalls.Native_UIGetProperty(_handle, "position"));
        set => NativeCalls.Native_UISetProperty(_handle, "position", $"{value.x},{value.y}");
    }

    public Vector2f Size
    {
        get => ParseVector2(NativeCalls.Native_UIGetProperty(_handle, "size"));
        set => NativeCalls.Native_UISetProperty(_handle, "size", $"{value.x},{value.y}");
    }

    protected static Vector2f ParseVector2(string raw)
    {
        if (string.IsNullOrEmpty(raw)) return new Vector2f();
        var parts = raw.Split(',');
        if (parts.Length >= 2 &&
            float.TryParse(parts[0], out var x) &&
            float.TryParse(parts[1], out var y))
            return new Vector2f(x, y);
        return new Vector2f();
    }

    protected static Color ParseColor(string raw)
    {
        if (string.IsNullOrEmpty(raw)) return Color.White;
        var parts = raw.Split(',');
        if (parts.Length >= 4 &&
            float.TryParse(parts[0], out var r) &&
            float.TryParse(parts[1], out var g) &&
            float.TryParse(parts[2], out var b) &&
            float.TryParse(parts[3], out var a))
            return new Color(r, g, b, a);
        return Color.White;
    }
}
