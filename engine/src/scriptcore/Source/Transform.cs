namespace GreenCake;

public class Transform : Component
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
        get { return EcsTransform.Position; }
        set { EcsTransform.Position = value; }
    }

    public Vector3f Rotation
    {
        get { return EcsTransform.Rotation; }
        set { EcsTransform.Rotation = value; }
    }

    public Vector3f Scale
    {
        get { return EcsTransform.Scale; }
        set { EcsTransform.Scale = value; }
    }

    public GameObject GameObject { get; internal set; }

    public Transform Parent
    {
        get { return GameObjectManager.GetParentTransform(Entity.ID); }
        set
        {
            if (value == null)
            {
                InternalCalls.Native_EntitySetParent(Entity.ID, 0);
                GameObjectManager.SetParent(Entity.ID, null);
            }
            else
            {
                InternalCalls.Native_EntitySetParent(Entity.ID, value.Entity.ID);
                GameObjectManager.SetParent(Entity.ID, value.GameObject);
            }
        }
    }

    public int ChildCount { get { return GameObjectManager.GetChildCount(Entity.ID); } }

    public Transform GetChild(int index)
    {
        return GameObjectManager.GetChild(Entity.ID, index);
    }

    public void Translate(Vector3f delta)
    {
        Position = new Vector3f(Position.X + delta.X, Position.Y + delta.Y, Position.Z + delta.Z);
    }

    public void Rotate(Vector3f delta)
    {
        Rotation = new Vector3f(Rotation.X + delta.X, Rotation.Y + delta.Y, Rotation.Z + delta.Z);
    }

    public override string ToString()
    {
        return string.Format("Transform(Pos={0}, Rot={1}, Scale={2})", Position, Rotation, Scale);
    }
}

internal static class EntityTransformExtensions
{
    public static Transform GetTransform(this Entity entity)
    {
        if (entity == null) return null;
        var go = GameObjectManager.FindByID(entity.ID);
        return go != null ? go.Transform : null;
    }
}
