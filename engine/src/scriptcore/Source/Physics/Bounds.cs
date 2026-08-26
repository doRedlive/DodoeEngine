namespace GreenCake;

using System;

public struct Bounds
{
    public Vector2f Center;
    public Vector2f Size;

    public Vector2f Min
    {
        readonly get => Center - Size * 0.5f;
        set { Center = value + Size * 0.5f; }
    }

    public Vector2f Max
    {
        readonly get => Center + Size * 0.5f;
        set { Center = value - Size * 0.5f; }
    }

    public readonly Vector2f Extents => Size * 0.5f;

    public Bounds(Vector2f center, Vector2f size)
    {
        Center = center;
        Size = new Vector2f(MathF.Abs(size.x), MathF.Abs(size.y));
    }

    public void Encapsulate(Vector2f point)
    {
        var min = Min;
        var max = Max;
        min.x = MathF.Min(min.x, point.x);
        min.y = MathF.Min(min.y, point.y);
        max.x = MathF.Max(max.x, point.x);
        max.y = MathF.Max(max.y, point.y);
        SetMinMax(min, max);
    }

    public void Encapsulate(Bounds other)
    {
        Encapsulate(other.Min);
        Encapsulate(other.Max);
    }

    public void SetMinMax(Vector2f min, Vector2f max)
    {
        Size = new Vector2f(max.x - min.x, max.y - min.y);
        Center = min + Size * 0.5f;
    }

    public readonly bool Contains(Vector2f point)
    {
        var min = Min;
        var max = Max;
        return point.x >= min.x && point.x <= max.x
            && point.y >= min.y && point.y <= max.y;
    }

    public readonly bool Intersects(Bounds other)
    {
        var a = this;
        var b = other;
        return a.Min.x <= b.Max.x && a.Max.x >= b.Min.x
            && a.Min.y <= b.Max.y && a.Max.y >= b.Min.y;
    }

    public readonly float SqrDistance(Vector2f point)
    {
        var dx = MathF.Max(MathF.Max(Min.x - point.x, 0f), point.x - Max.x);
        var dy = MathF.Max(MathF.Max(Min.y - point.y, 0f), point.y - Max.y);
        return dx * dx + dy * dy;
    }
}
