namespace GreenCake;

using System;

public struct LayerMask
{
    public uint value;

    public LayerMask(uint value) { this.value = value; }

    public static implicit operator LayerMask(uint v) => new(v);
    public static implicit operator uint(LayerMask m) => m.value;

    public static LayerMask AllLayers => new LayerMask(uint.MaxValue);
    public static LayerMask None => new LayerMask(0);

    public static int NameToLayer(string name)
    {
        return Math.Clamp(name.GetHashCode() % 32, 0, 31);
    }

    public static LayerMask GetMask(params int[] layerNumbers)
    {
        uint v = 0;
        foreach (var n in layerNumbers)
            if (n >= 0 && n < 32) v |= 1u << n;
        return new LayerMask(v);
    }

    public readonly bool Has(int layerNumber)
    {
        if (layerNumber < 0 || layerNumber >= 32) return false;
        return (value & (1u << layerNumber)) != 0;
    }
}
