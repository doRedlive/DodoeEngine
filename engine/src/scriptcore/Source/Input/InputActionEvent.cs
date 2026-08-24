namespace GreenCake;

public struct InputActionEvent
{
    public uint ActionId;
    public string MapName;
    public string ActionName;
    public InputActionPhase Phase;
    public InputActionValueType ValueType;
    public bool BoolValue;
    public float FloatValue;
    public Vector2f Vector2Value;

    public bool ReadValueAsButton() => ValueType == InputActionValueType.Button && BoolValue;

    public float ReadValueAsAxis()
    {
        return ValueType switch
        {
            InputActionValueType.Axis1D => FloatValue,
            InputActionValueType.Axis2D => Vector2Value.x,
            _ => BoolValue ? 1.0f : 0.0f,
        };
    }

    public Vector2f ReadValueAsVector2() =>
        ValueType == InputActionValueType.Axis2D ? Vector2Value : new Vector2f(ReadValueAsAxis(), 0.0f);
}
