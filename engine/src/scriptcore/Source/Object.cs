namespace GreenCake;

public class Object
{
    public int InstanceID { get; }

    public Object(int instanceID) => InstanceID = instanceID;

    public bool IsValid => InstanceID != 0;

    public override bool Equals(object? obj) =>
        obj is Object other && InstanceID == other.InstanceID;

    public override int GetHashCode() => InstanceID;

    public static bool operator ==(Object? a, Object? b) =>
        ReferenceEquals(a, b) || (a is not null && b is not null && a.InstanceID == b.InstanceID);

    public static bool operator !=(Object? a, Object? b) => !(a == b);
}
