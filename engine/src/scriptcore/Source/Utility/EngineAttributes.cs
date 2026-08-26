namespace GreenCake;

using System;

[AttributeUsage(AttributeTargets.Field | AttributeTargets.Property, AllowMultiple = false, Inherited = true)]
public sealed class SerializeFieldAttribute : Attribute
{
}

[AttributeUsage(AttributeTargets.Field | AttributeTargets.Property, AllowMultiple = false, Inherited = true)]
public sealed class HeaderAttribute : Attribute
{
    public string Header { get; }
    public HeaderAttribute(string header) { Header = header; }
}

[AttributeUsage(AttributeTargets.Class, AllowMultiple = true, Inherited = true)]
public sealed class RequireComponentAttribute : Attribute
{
    public Type[] RequiredTypes { get; }
    public RequireComponentAttribute(Type t) { RequiredTypes = new[] { t }; }
    public RequireComponentAttribute(Type t1, Type t2) { RequiredTypes = new[] { t1, t2 }; }
    public RequireComponentAttribute(Type t1, Type t2, Type t3) { RequiredTypes = new[] { t1, t2, t3 }; }
}

[AttributeUsage(AttributeTargets.Class, AllowMultiple = false, Inherited = true)]
public sealed class DisallowMultipleComponentAttribute : Attribute
{
}

[AttributeUsage(AttributeTargets.Class | AttributeTargets.Struct, AllowMultiple = false, Inherited = true)]
public sealed class SerializableAttribute : Attribute
{
}

[AttributeUsage(AttributeTargets.Field, AllowMultiple = false, Inherited = true)]
public sealed class TextAreaAttribute : Attribute
{
    public int MinLines { get; }
    public int MaxLines { get; }
    public TextAreaAttribute() { MinLines = 1; MaxLines = int.MaxValue; }
    public TextAreaAttribute(int minLines, int maxLines) { MinLines = minLines; MaxLines = maxLines; }
}

[AttributeUsage(AttributeTargets.Method, AllowMultiple = false, Inherited = false)]
public sealed class ToolMenuItemAttribute : Attribute
{
    public string Path { get; }
    public ToolMenuItemAttribute(string path) { Path = path; }
}
