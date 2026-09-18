namespace GreenCake.Editor;

using System;
using System.Collections.Generic;
using System.Text.Json;

public readonly struct EditorEvent
{
    public string Owner { get; }
    public string OwnerId { get; }
    public string ControlId { get; }
    public string Event { get; }
    public string Value { get; }

    public EditorEvent(string owner, string ownerId, string controlId, string @event, string value)
    {
        Owner = owner;
        OwnerId = ownerId;
        ControlId = controlId;
        Event = @event;
        Value = value;
    }
}

public abstract class Editor
{
    public virtual void OnInspectorGUI(EditorGUI gui) { }

    public virtual void OnEvent(EditorEvent e) { }
}

public abstract class EditorWindow
{
    public virtual void OnGUI(EditorGUI gui) { }

    public virtual void OnEvent(EditorEvent e) { }
}

[AttributeUsage(AttributeTargets.Class, AllowMultiple = false, Inherited = false)]
public sealed class CustomEditorAttribute : Attribute
{
    public Type TargetType { get; }

    public CustomEditorAttribute(Type targetType) { TargetType = targetType; }
}

[AttributeUsage(AttributeTargets.Class, AllowMultiple = false, Inherited = false)]
public sealed class EditorWindowAttribute : Attribute
{
    public string Title { get; }

    public EditorWindowAttribute(string title) { Title = title; }
}

public sealed class EditorGUI
{
    private readonly List<Dictionary<string, object?>> _root = new();
    private readonly Stack<List<Dictionary<string, object?>>> _stack = new();

    public EditorGUI()
    {
        _stack.Push(_root);
    }

    private List<Dictionary<string, object?>> Current => _stack.Peek();

    private static Dictionary<string, object?> NewNode(string type, string id)
    {
        var node = new Dictionary<string, object?> { ["type"] = type };
        if (!string.IsNullOrEmpty(id))
            node["id"] = id;
        return node;
    }

    private void Add(string type, string id, Action<Dictionary<string, object?>> fill)
    {
        var node = NewNode(type, id);
        fill(node);
        Current.Add(node);
    }

    private void Push(string type, string id, Action<Dictionary<string, object?>> fill)
    {
        var node = NewNode(type, id);
        var children = new List<Dictionary<string, object?>>();
        node["children"] = children;
        fill(node);
        Current.Add(node);
        _stack.Push(children);
    }

    private void Pop()
    {
        if (_stack.Count > 1)
            _stack.Pop();
    }

    public void Label(string text) => Add("label", string.Empty, n => n["text"] = text);

    public void Button(string id, string text, bool enabled = true)
        => Add("button", id, n => { n["text"] = text; n["enabled"] = enabled; });

    public void Toggle(string id, string text, bool value)
        => Add("toggle", id, n => { n["text"] = text; n["value"] = value; });

    public void Float(string id, string text, float value, float min = 0.0f, float max = 0.0f)
        => Add("float", id, n => { n["text"] = text; n["value"] = value; n["min"] = min; n["max"] = max; });

    public void Int(string id, string text, int value, int min = 0, int max = 0)
        => Add("int", id, n => { n["text"] = text; n["value"] = value; n["min"] = min; n["max"] = max; });

    public void Text(string id, string text, string value)
        => Add("text", id, n => { n["text"] = text; n["value"] = value; });

    public void Vector3(string id, string text, float x, float y, float z)
        => Add("vector3", id, n => { n["text"] = text; n["value"] = new[] { x, y, z }; });

    public void Color(string id, string text, float r, float g, float b, float a)
        => Add("color", id, n => { n["text"] = text; n["value"] = new[] { r, g, b, a }; });

    public void Object(string id, string text, ulong uuid)
        => Add("object", id, n => { n["text"] = text; n["value"] = uuid; });

    public void Asset(string id, string text, ulong uuid)
        => Add("asset", id, n => { n["text"] = text; n["value"] = uuid; });

    public void Property(string name, string label = "")
        => Add("property", "prop:" + name, n =>
        {
            n["name"] = name;
            if (!string.IsNullOrEmpty(label))
                n["text"] = label;
        });

    public void Separator() => Add("separator", string.Empty, _ => { });

    public void Space(float size = 8.0f) => Add("space", string.Empty, n => n["size"] = size);

    public void Help(string text, string level = "info")
        => Add("help", string.Empty, n => { n["text"] = text; n["level"] = level; });

    public void BeginRow() => Push("row", string.Empty, _ => { });

    public void EndRow() => Pop();

    public void BeginColumn() => Push("column", string.Empty, _ => { });

    public void EndColumn() => Pop();

    public void BeginFoldout(string id, string text, bool expanded = true)
        => Push("foldout", id, n => { n["text"] = text; n["expanded"] = expanded; });

    public void EndFoldout() => Pop();

    public string ToJson()
    {
        return JsonSerializer.Serialize(new Dictionary<string, object?>
        {
            ["version"] = 1,
            ["nodes"] = _root,
        });
    }
}
