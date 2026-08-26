namespace GreenCake;

using System;

public struct ContactFilter2D
{
    public bool UseLayerMask;
    public LayerMask LayerMask;
    public bool UseTriggers;
    public bool UseDepth;
    public float MinDepth;
    public float MaxDepth;
    public bool UseOutsideDepth;
    public float MinNormalAngle;
    public float MaxNormalAngle;

    public static ContactFilter2D Default =>
        new() { UseLayerMask = true, LayerMask = LayerMask.AllLayers, UseTriggers = true };

    public readonly bool IsFiltering =>
        UseLayerMask || UseTriggers || UseDepth || MinNormalAngle != 0f || MaxNormalAngle != 360f;

    public void SetLayerMask(LayerMask mask)
    {
        UseLayerMask = true;
        LayerMask = mask;
    }

    public void ClearLayerMask()
    {
        UseLayerMask = false;
        LayerMask = LayerMask.None;
    }
}
