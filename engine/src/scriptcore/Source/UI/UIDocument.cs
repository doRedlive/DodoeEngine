// do@Redlive

namespace GreenCake.UI;

using System;
using System.Collections.Generic;

public static class UIDocument
{
    private static readonly Dictionary<Type, string> TypeToNativeName = new()
    {
        { typeof(UIButton), "Button" },
        { typeof(UILabel), "Label" },
        { typeof(UIImage), "Image" },
        { typeof(UIPanel), "Panel" },
        { typeof(UIElement), "" },
    };

    private static readonly List<UIElement> _eventTargets = new();

    public static bool LoadLayout(string filePath)
    {
        return NativeCalls.Native_UILoadLayout(filePath);
    }

    public static void ClearAll()
    {
        _eventTargets.Clear();
        NativeCalls.Native_UIClearAll();
    }

    public static T Find<T>(string elementId) where T : UIElement
    {
        if (string.IsNullOrEmpty(elementId)) return null;
        string typeName = TypeToNativeName.TryGetValue(typeof(T), out var n) ? n : "";
        uint handle = NativeCalls.Native_UIFindElement(elementId, typeName);
        if (handle == 0) return null;
        return (T)Activator.CreateInstance(typeof(T), elementId, handle);
    }

    public static UIElement Find(string elementId)
    {
        return Find<UIElement>(elementId);
    }

    public static T Create<T>(string id, string parentId = "") where T : UIElement
    {
        if (string.IsNullOrEmpty(id)) return null;
        string typeName = TypeToNativeName.TryGetValue(typeof(T), out var n) ? n : typeof(T).Name;
        uint handle = NativeCalls.Native_UICreateElement(typeName, id, parentId ?? "");
        if (handle == 0) return null;
        return (T)Activator.CreateInstance(typeof(T), id, handle);
    }

    public static void DispatchEvents()
    {
        foreach (var target in _eventTargets)
        {
            if (target is UIButton btn) btn.PollEvents();
        }
    }

    internal static void RegisterEventTarget(UIElement element)
    {
        if (!_eventTargets.Contains(element))
            _eventTargets.Add(element);
    }
}
