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
    public ulong ID => NativeCalls.Native_IDComponentGetID(Entity.ID);

    public string Name
    {
        get => NativeCalls.Native_IDComponentGetName(Entity.ID);
        set => NativeCalls.Native_IDComponentSetName(Entity.ID, value);
    }
}

public class TagComponent : Component
{
    public string Tag
    {
        get => NativeCalls.Native_TagComponentGetTag(Entity.ID);
        set => NativeCalls.Native_TagComponentSetTag(Entity.ID, value);
    }
}

public class TransformComponent : Component
{
	public Vector3f Position
	{
		get 
		{
			NativeCalls.Native_TransfromComponentGetPosition(Entity.ID, out Vector3f position);
			return position;
		}
		set
		{
			NativeCalls.Native_TransfromComponentSetPosition(Entity.ID, ref value);
		}
	}

	public Vector3f Rotation
	{
		get
		{
			NativeCalls.Native_TransfromComponentGetRotation(Entity.ID, out Vector3f rotation);
			return rotation;
		}
		set
		{
			NativeCalls.Native_TransfromComponentSetRotation(Entity.ID, ref value);
		}
	}

	public Vector3f Scale
	{
		get
		{
			NativeCalls.Native_TransfromComponentGetScale(Entity.ID, out Vector3f scale);
			return scale;
		}
		set
		{
			NativeCalls.Native_TransfromComponentSetScale(Entity.ID, ref value);
		}
	}
}

public class Animation2dComponent : Component
{
    public uint CurrentAnimationID
    {
        get => NativeCalls.Native_Animation2dComponentGetCurrentAnimationID(Entity.ID);
        set => NativeCalls.Native_Animation2dComponentSetCurrentAnimationID(Entity.ID, value);
    }

    public ulong CurrentFrameID
    {
        get => NativeCalls.Native_Animation2dComponentGetCurrentFrameID(Entity.ID);
        set => NativeCalls.Native_Animation2dComponentSetCurrentFrameID(Entity.ID, value);
    }

    public float CurrentTimeDuration
    {
        get => NativeCalls.Native_Animation2dComponentGetCurrentTimeDuration(Entity.ID);
        set => NativeCalls.Native_Animation2dComponentSetCurrentTimeDuration(Entity.ID, value);
    }

    public float Speed
    {
        get => NativeCalls.Native_Animation2dComponentGetSpeed(Entity.ID);
        set => NativeCalls.Native_Animation2dComponentSetSpeed(Entity.ID, value);
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
        get => (CameraType)NativeCalls.Native_Camera2dComponentGetType(Entity.ID);
        set => NativeCalls.Native_Camera2dComponentSetType(Entity.ID, (int)value);
    }

    public float Zoom
    {
        get => NativeCalls.Native_Camera2dComponentGetZoom(Entity.ID);
        set => NativeCalls.Native_Camera2dComponentSetZoom(Entity.ID, value);
    }

    public Color Background
    {
        get
        {
            NativeCalls.Native_Camera2dComponentGetBackground(Entity.ID, out Color color);
            return color;
        }
        set => NativeCalls.Native_Camera2dComponentSetBackground(Entity.ID, ref value);
    }
}

public class BoxCollider2dComponent : Component
{
    public Vector2f Offset
    {
        get
        {
            NativeCalls.Native_BoxCollider2dComponentGetOffset(Entity.ID, out Vector2f offset);
            return offset;
        }
        set => NativeCalls.Native_BoxCollider2dComponentSetOffset(Entity.ID, ref value);
    }

    public Vector2f Size
    {
        get
        {
            NativeCalls.Native_BoxCollider2dComponentGetSize(Entity.ID, out Vector2f size);
            return size;
        }
        set => NativeCalls.Native_BoxCollider2dComponentSetSize(Entity.ID, ref value);
    }

    public float Density
    {
        get => NativeCalls.Native_BoxCollider2dComponentGetDensity(Entity.ID);
        set => NativeCalls.Native_BoxCollider2dComponentSetDensity(Entity.ID, value);
    }

    public float Friction
    {
        get => NativeCalls.Native_BoxCollider2dComponentGetFriction(Entity.ID);
        set => NativeCalls.Native_BoxCollider2dComponentSetFriction(Entity.ID, value);
    }

    public float Restitution
    {
        get => NativeCalls.Native_BoxCollider2dComponentGetRestitution(Entity.ID);
        set => NativeCalls.Native_BoxCollider2dComponentSetRestitution(Entity.ID, value);
    }

    public float RestitutionThreshold
    {
        get => NativeCalls.Native_BoxCollider2dComponentGetRestitutionThreshold(Entity.ID);
        set => NativeCalls.Native_BoxCollider2dComponentSetRestitutionThreshold(Entity.ID, value);
    }
}

public class MeshRendererComponent : Component
{
    public int Value
    {
        get => NativeCalls.Native_MeshRendererComponentGetValue(Entity.ID);
        set => NativeCalls.Native_MeshRendererComponentSetValue(Entity.ID, value);
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
        get => (BodyType)NativeCalls.Native_Rigidbody2dComponentGetType(Entity.ID);
        set => NativeCalls.Native_Rigidbody2dComponentSetType(Entity.ID, (int)value);
    }

    public float GravityScale
    {
        get => NativeCalls.Native_Rigidbody2dComponentGetGravityScale(Entity.ID);
        set => NativeCalls.Native_Rigidbody2dComponentSetGravityScale(Entity.ID, value);
    }

    public bool FixedRotation
    {
        get => NativeCalls.Native_Rigidbody2dComponentGetFixedRotation(Entity.ID);
        set => NativeCalls.Native_Rigidbody2dComponentSetFixedRotation(Entity.ID, value);
    }

    public void SetLinearVelocity(Vector2f velocity)
    {
        NativeCalls.Native_Rigidbody2dComponentSetLinearVelocity(Entity.ID, ref velocity);
    }

    public void ApplyForceToCenter(Vector2f force, bool wake = true)
    {
        NativeCalls.Native_Rigidbody2dComponentApplyForceToCenter(Entity.ID, ref force, wake);
    }

    public void ApplyLinearImpulseToCenter(Vector2f impulse, bool wake = true)
    {
        NativeCalls.Native_Rigidbody2dComponentApplyLinearImpulseToCenter(Entity.ID, ref impulse, wake);
    }
}

public class SpriteRendererComponent : Component
{
    public uint TextureID
    {
        get => NativeCalls.Native_SpriteRendererComponentGetTextureID(Entity.ID);
        set => NativeCalls.Native_SpriteRendererComponentSetTextureID(Entity.ID, value);
    }

    public bool Flip
    {
        get => NativeCalls.Native_SpriteRendererComponentGetFlip(Entity.ID);
        set => NativeCalls.Native_SpriteRendererComponentSetFlip(Entity.ID, value);
    }

    public Vector2f Pivot
    {
        get
        {
            NativeCalls.Native_SpriteRendererComponentGetPivot(Entity.ID, out Vector2f pivot);
            return pivot;
        }
        set => NativeCalls.Native_SpriteRendererComponentSetPivot(Entity.ID, ref value);
    }

    public float Depth
    {
        get => NativeCalls.Native_SpriteRendererComponentGetDepth(Entity.ID);
        set => NativeCalls.Native_SpriteRendererComponentSetDepth(Entity.ID, value);
    }

    public Color Color
    {
        get
        {
            NativeCalls.Native_SpriteRendererComponentGetColor(Entity.ID, out Color color);
            return color;
        }
        set => NativeCalls.Native_SpriteRendererComponentSetColor(Entity.ID, ref value);
    }
}

public class TilemapComponent : Component
{
    public uint MapWidth
    {
        get => NativeCalls.Native_TilemapComponentGetMapWidth(Entity.ID);
        set => NativeCalls.Native_TilemapComponentSetMapWidth(Entity.ID, value);
    }

    public uint MapHeight
    {
        get => NativeCalls.Native_TilemapComponentGetMapHeight(Entity.ID);
        set => NativeCalls.Native_TilemapComponentSetMapHeight(Entity.ID, value);
    }

    public uint TileWidth
    {
        get => NativeCalls.Native_TilemapComponentGetTileWidth(Entity.ID);
        set => NativeCalls.Native_TilemapComponentSetTileWidth(Entity.ID, value);
    }

    public uint TileHeight
    {
        get => NativeCalls.Native_TilemapComponentGetTileHeight(Entity.ID);
        set => NativeCalls.Native_TilemapComponentSetTileHeight(Entity.ID, value);
    }
}

public class TileLayerComponent : Component
{
    public string LayerName
    {
        get => NativeCalls.Native_TileLayerComponentGetLayerName(Entity.ID);
        set => NativeCalls.Native_TileLayerComponentSetLayerName(Entity.ID, value);
    }

    public uint LayerWidth
    {
        get => NativeCalls.Native_TileLayerComponentGetLayerWidth(Entity.ID);
        set => NativeCalls.Native_TileLayerComponentSetLayerWidth(Entity.ID, value);
    }

    public uint LayerHeight
    {
        get => NativeCalls.Native_TileLayerComponentGetLayerHeight(Entity.ID);
        set => NativeCalls.Native_TileLayerComponentSetLayerHeight(Entity.ID, value);
    }

    public bool Visible
    {
        get => NativeCalls.Native_TileLayerComponentGetVisible(Entity.ID);
        set => NativeCalls.Native_TileLayerComponentSetVisible(Entity.ID, value);
    }

    public float Opacity
    {
        get => NativeCalls.Native_TileLayerComponentGetOpacity(Entity.ID);
        set => NativeCalls.Native_TileLayerComponentSetOpacity(Entity.ID, value);
    }

    public int OffsetX
    {
        get => NativeCalls.Native_TileLayerComponentGetOffsetX(Entity.ID);
        set => NativeCalls.Native_TileLayerComponentSetOffsetX(Entity.ID, value);
    }

    public int OffsetY
    {
        get => NativeCalls.Native_TileLayerComponentGetOffsetY(Entity.ID);
        set => NativeCalls.Native_TileLayerComponentSetOffsetY(Entity.ID, value);
    }
}
