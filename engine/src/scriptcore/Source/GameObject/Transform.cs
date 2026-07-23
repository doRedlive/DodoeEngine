namespace GreenCake;

public class Transform : CakeComponent
{
    private TransformComponent EcsTransform
    {
        get
        {
            if (Entity == null)
                throw new System.InvalidOperationException("Transform has no Entity reference.");
            return Entity.GetComponent<TransformComponent>();
        }
    }

    public Vector3f Position
    {
        get => EcsTransform.Position;
        set => EcsTransform.Position = value;
    }

    public Vector3f Rotation
    {
        get => EcsTransform.Rotation;
        set => EcsTransform.Rotation = value;
    }

    public Vector3f Scale
    {
        get => EcsTransform.Scale;
        set => EcsTransform.Scale = value;
    }

    public GameObject GameObject { get; internal set; }

    public Transform Parent
    {
        get
        {
            var scene = GameObject != null ? GameObject.Scene : null;
            return scene != null ? scene.GetParentTransform(Entity.ID) : null;
        }
        set
        {
            if (value == null)
            {
                NativeCalls.Native_EntitySetParent(Entity.ID, 0);
                GameObject?.Scene?.SetParent(Entity.ID, null);
            }
            else
            {
                NativeCalls.Native_EntitySetParent(Entity.ID, value.Entity.ID);
                GameObject?.Scene?.SetParent(Entity.ID, value.GameObject);
            }
        }
    }

    public int ChildCount
    {
        get
        {
            var scene = GameObject != null ? GameObject.Scene : null;
            return scene != null ? scene.GetChildCount(Entity.ID) : 0;
        }
    }

    public Transform GetChild(int index)
    {
        var scene = GameObject != null ? GameObject.Scene : null;
        return scene != null ? scene.GetChild(Entity.ID, index) : null;
    }

    public void Translate(Vector3f delta)
    {
        Position = new Vector3f(Position.x + delta.x, Position.y + delta.y, Position.z + delta.z);
    }

    public void Rotate(Vector3f delta)
    {
        Rotation = new Vector3f(Rotation.x + delta.x, Rotation.y + delta.y, Rotation.z + delta.z);
    }

    public override string ToString()
    {
        return $"Transform(Pos={Position}, Rot={Rotation}, Scale={Scale})";
    }
}

internal static class EntityTransformExtensions
{
    public static Transform GetTransform(this Entity entity)
    {
        if (entity == null) return null;
        var go = GameObject.FindByID(entity.ID);
        return go != null ? go.Transform : null;
    }
}
