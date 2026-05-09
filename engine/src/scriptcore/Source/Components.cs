namespace GreenCake;

using System;

internal interface IComponent
{
}

public abstract class Component : IComponent
{
	public Entity Entity { get; internal set; } = null!;
}

public class IDComponent : Component
{
    public ulong ID => InternalCalls.Native_IDComponentGetID(Entity.ID);

    public string Name
    {
        get => InternalCalls.Native_IDComponentGetName(Entity.ID);
        set => InternalCalls.Native_IDComponentSetName(Entity.ID, value);
    }
}

public class TagComponent : Component
{
    public string Tag
    {
        get => InternalCalls.Native_TagComponentGetTag(Entity.ID);
        set => InternalCalls.Native_TagComponentSetTag(Entity.ID, value);
    }
}

public class TransformComponent : Component
{
	public Vector3f Position
	{
		get 
		{
			InternalCalls.Native_TransfromComponentGetPosition(Entity.ID, out Vector3f position);
			return position;
		}
		set
		{
			InternalCalls.Native_TransfromComponentSetPosition(Entity.ID, ref value);
		}
	}

	public Vector3f Rotation
	{
		get
		{
			InternalCalls.Native_TransfromComponentGetRotation(Entity.ID, out Vector3f rotation);
			return rotation;
		}
		set
		{
			InternalCalls.Native_TransfromComponentSetRotation(Entity.ID, ref value);
		}
	}

	public Vector3f Scale
	{
		get
		{
			InternalCalls.Native_TransfromComponentGetScale(Entity.ID, out Vector3f scale);
			return scale;
		}
		set
		{
			InternalCalls.Native_TransfromComponentSetScale(Entity.ID, ref value);
		}
	}
}

public class Animation2dComponent : Component
{
    public uint CurrentAnimationID
    {
        get => InternalCalls.Native_Animation2dComponentGetCurrentAnimationID(Entity.ID);
        set => InternalCalls.Native_Animation2dComponentSetCurrentAnimationID(Entity.ID, value);
    }

    public ulong CurrentFrameID
    {
        get => InternalCalls.Native_Animation2dComponentGetCurrentFrameID(Entity.ID);
        set => InternalCalls.Native_Animation2dComponentSetCurrentFrameID(Entity.ID, value);
    }

    public float CurrentTimeDuration
    {
        get => InternalCalls.Native_Animation2dComponentGetCurrentTimeDuration(Entity.ID);
        set => InternalCalls.Native_Animation2dComponentSetCurrentTimeDuration(Entity.ID, value);
    }

    public float Speed
    {
        get => InternalCalls.Native_Animation2dComponentGetSpeed(Entity.ID);
        set => InternalCalls.Native_Animation2dComponentSetSpeed(Entity.ID, value);
    }
}

public class Camera2dComponent : Component
{
    public enum CameraType
    {
        None = 0,
        Perspective = 1,
        Orthographic = 2
    }

    public CameraType Type
    {
        get => (CameraType)InternalCalls.Native_Camera2dComponentGetType(Entity.ID);
        set => InternalCalls.Native_Camera2dComponentSetType(Entity.ID, (int)value);
    }

    public float Zoom
    {
        get => InternalCalls.Native_Camera2dComponentGetZoom(Entity.ID);
        set => InternalCalls.Native_Camera2dComponentSetZoom(Entity.ID, value);
    }

    public Color Background
    {
        get
        {
            InternalCalls.Native_Camera2dComponentGetBackground(Entity.ID, out Color color);
            return color;
        }
        set => InternalCalls.Native_Camera2dComponentSetBackground(Entity.ID, ref value);
    }
}

public class BoxCollider2dComponent : Component
{
    public Vector2f Offset
    {
        get
        {
            InternalCalls.Native_BoxCollider2dComponentGetOffset(Entity.ID, out Vector2f offset);
            return offset;
        }
        set => InternalCalls.Native_BoxCollider2dComponentSetOffset(Entity.ID, ref value);
    }

    public Vector2f Size
    {
        get
        {
            InternalCalls.Native_BoxCollider2dComponentGetSize(Entity.ID, out Vector2f size);
            return size;
        }
        set => InternalCalls.Native_BoxCollider2dComponentSetSize(Entity.ID, ref value);
    }

    public float Density
    {
        get => InternalCalls.Native_BoxCollider2dComponentGetDensity(Entity.ID);
        set => InternalCalls.Native_BoxCollider2dComponentSetDensity(Entity.ID, value);
    }

    public float Friction
    {
        get => InternalCalls.Native_BoxCollider2dComponentGetFriction(Entity.ID);
        set => InternalCalls.Native_BoxCollider2dComponentSetFriction(Entity.ID, value);
    }

    public float Restitution
    {
        get => InternalCalls.Native_BoxCollider2dComponentGetRestitution(Entity.ID);
        set => InternalCalls.Native_BoxCollider2dComponentSetRestitution(Entity.ID, value);
    }

    public float RestitutionThreshold
    {
        get => InternalCalls.Native_BoxCollider2dComponentGetRestitutionThreshold(Entity.ID);
        set => InternalCalls.Native_BoxCollider2dComponentSetRestitutionThreshold(Entity.ID, value);
    }
}

public class MeshRendererComponent : Component
{
    public int Value
    {
        get => InternalCalls.Native_MeshRendererComponentGetValue(Entity.ID);
        set => InternalCalls.Native_MeshRendererComponentSetValue(Entity.ID, value);
    }
}

public class Rigidbody2dComponent : Component
{
    public enum BodyType
    {
        Static = 0,
        Dynamic = 1,
        Kinematic = 2
    }

    public BodyType Type
    {
        get => (BodyType)InternalCalls.Native_Rigidbody2dComponentGetType(Entity.ID);
        set => InternalCalls.Native_Rigidbody2dComponentSetType(Entity.ID, (int)value);
    }

    public float GravityScale
    {
        get => InternalCalls.Native_Rigidbody2dComponentGetGravityScale(Entity.ID);
        set => InternalCalls.Native_Rigidbody2dComponentSetGravityScale(Entity.ID, value);
    }

    public bool FixedRotation
    {
        get => InternalCalls.Native_Rigidbody2dComponentGetFixedRotation(Entity.ID);
        set => InternalCalls.Native_Rigidbody2dComponentSetFixedRotation(Entity.ID, value);
    }

    public void SetLinearVelocity(Vector2f velocity)
    {
        InternalCalls.Native_Rigidbody2dComponentSetLinearVelocity(Entity.ID, ref velocity);
    }

    public void ApplyForceToCenter(Vector2f force, bool wake = true)
    {
        InternalCalls.Native_Rigidbody2dComponentApplyForceToCenter(Entity.ID, ref force, wake);
    }

    public void ApplyLinearImpulseToCenter(Vector2f impulse, bool wake = true)
    {
        InternalCalls.Native_Rigidbody2dComponentApplyLinearImpulseToCenter(Entity.ID, ref impulse, wake);
    }
}

public class SpriteRendererComponent : Component
{
    public uint TextureID
    {
        get => InternalCalls.Native_SpriteRendererComponentGetTextureID(Entity.ID);
        set => InternalCalls.Native_SpriteRendererComponentSetTextureID(Entity.ID, value);
    }

    public bool Flip
    {
        get => InternalCalls.Native_SpriteRendererComponentGetFlip(Entity.ID);
        set => InternalCalls.Native_SpriteRendererComponentSetFlip(Entity.ID, value);
    }

    public Vector2f Pivot
    {
        get
        {
            InternalCalls.Native_SpriteRendererComponentGetPivot(Entity.ID, out Vector2f pivot);
            return pivot;
        }
        set => InternalCalls.Native_SpriteRendererComponentSetPivot(Entity.ID, ref value);
    }

    public float Depth
    {
        get => InternalCalls.Native_SpriteRendererComponentGetDepth(Entity.ID);
        set => InternalCalls.Native_SpriteRendererComponentSetDepth(Entity.ID, value);
    }

    public Color Color
    {
        get
        {
            InternalCalls.Native_SpriteRendererComponentGetColor(Entity.ID, out Color color);
            return color;
        }
        set => InternalCalls.Native_SpriteRendererComponentSetColor(Entity.ID, ref value);
    }
}
