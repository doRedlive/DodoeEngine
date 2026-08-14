namespace GreenCake;

using System;

internal interface ICakeComponent
{
}

public abstract class CakeComponent : ICakeComponent
{
    public Entity Entity { get; internal set; } = null!;
}

public abstract class NativeComponent : CakeComponent
{
}

public class IDComponent : NativeComponent
{
    public ulong ID => NativeCalls.Native_IDComponentGetID(Entity.ID);

    public string Name
    {
        get => NativeCalls.Native_IDComponentGetName(Entity.ID);
        set => NativeCalls.Native_IDComponentSetName(Entity.ID, value);
    }
}

public sealed class Rigidbody2D : NativeComponent
{
    static Rigidbody2D()
    {
        NativeProxyFactory.Register<Rigidbody2D>(e => new Rigidbody2D { Entity = e });
    }

    public float GravityScale
    {
        get => NativeCalls.Rigidbody2dComponent_GravityScale_Get(Entity.ID);
        set => NativeCalls.Rigidbody2dComponent_GravityScale_Set(Entity.ID, value);
    }

    public bool FixedRotation
    {
        get => NativeCalls.Rigidbody2dComponent_FixedRotation_Get(Entity.ID);
        set => NativeCalls.Rigidbody2dComponent_FixedRotation_Set(Entity.ID, value);
    }

    public void SetVelocity(Vector2f value) => NativeCalls.Native_Rigidbody2D_SetVelocity(Entity.ID, value.x, value.y);

    public void ApplyForce(Vector2f force) => NativeCalls.Native_Rigidbody2D_ApplyForce(Entity.ID, force.x, force.y);

    public void ApplyImpulse(Vector2f impulse) => NativeCalls.Native_Rigidbody2D_ApplyImpulse(Entity.ID, impulse.x, impulse.y);
}

public sealed class Animator : NativeComponent
{
    static Animator()
    {
        NativeProxyFactory.Register<Animator>(e => new Animator { Entity = e });
    }

    public float Speed
    {
        get => NativeCalls.AnimatorComponent_Speed_Get(Entity.ID);
        set => NativeCalls.AnimatorComponent_Speed_Set(Entity.ID, value);
    }

    public bool PlayOnAwake
    {
        get => NativeCalls.AnimatorComponent_PlayOnAwake_Get(Entity.ID);
        set => NativeCalls.AnimatorComponent_PlayOnAwake_Set(Entity.ID, value);
    }

    public void Play(string name) => NativeCalls.Native_Animator_Play(Entity.ID, name);

    public void Stop() => NativeCalls.Native_Animator_Stop(Entity.ID);

    public void Resume() => NativeCalls.Native_Animator_Resume(Entity.ID);
}

public sealed class Rigidbody : NativeComponent
{
    static Rigidbody()
    {
        NativeProxyFactory.Register<Rigidbody>(e => new Rigidbody { Entity = e });
    }

    public float GravityScale
    {
        get => NativeCalls.RigidbodyComponent_GravityScale_Get(Entity.ID);
        set => NativeCalls.RigidbodyComponent_GravityScale_Set(Entity.ID, value);
    }

    public float LinearDamping
    {
        get => NativeCalls.RigidbodyComponent_LinearDamping_Get(Entity.ID);
        set => NativeCalls.RigidbodyComponent_LinearDamping_Set(Entity.ID, value);
    }

    public float AngularDamping
    {
        get => NativeCalls.RigidbodyComponent_AngularDamping_Get(Entity.ID);
        set => NativeCalls.RigidbodyComponent_AngularDamping_Set(Entity.ID, value);
    }

    public float MassOverride
    {
        get => NativeCalls.RigidbodyComponent_MassOverride_Get(Entity.ID);
        set => NativeCalls.RigidbodyComponent_MassOverride_Set(Entity.ID, value);
    }

    public bool LockRotation
    {
        get => NativeCalls.RigidbodyComponent_LockRotation_Get(Entity.ID);
        set => NativeCalls.RigidbodyComponent_LockRotation_Set(Entity.ID, value);
    }

    public bool IsBullet
    {
        get => NativeCalls.RigidbodyComponent_IsBullet_Get(Entity.ID);
        set => NativeCalls.RigidbodyComponent_IsBullet_Set(Entity.ID, value);
    }

    public bool Enabled
    {
        get => NativeCalls.RigidbodyComponent_Enabled_Get(Entity.ID);
        set => NativeCalls.RigidbodyComponent_Enabled_Set(Entity.ID, value);
    }

    public void SetVelocity(Vector3f value) => NativeCalls.Native_Rigidbody_SetVelocity(Entity.ID, value.x, value.y, value.z);

    public void ApplyForce(Vector3f force) => NativeCalls.Native_Rigidbody_ApplyForce(Entity.ID, force.x, force.y, force.z);

    public void ApplyImpulse(Vector3f impulse) => NativeCalls.Native_Rigidbody_ApplyImpulse(Entity.ID, impulse.x, impulse.y, impulse.z);

    public void Teleport(Vector3f position, Quaternion rotation) => NativeCalls.Native_Rigidbody_Teleport(Entity.ID, position.x, position.y, position.z, rotation.x, rotation.y, rotation.z, rotation.w);
}

public sealed class BoxCollider : NativeComponent
{
    static BoxCollider()
    {
        NativeProxyFactory.Register<BoxCollider>(e => new BoxCollider { Entity = e });
    }

    public Vector3f Offset
    {
        get => NativeCalls.BoxColliderComponent_Offset_Get(Entity.ID);
        set => NativeCalls.BoxColliderComponent_Offset_Set(Entity.ID, ref value);
    }

    public Vector3f Rotation
    {
        get => NativeCalls.BoxColliderComponent_Rotation_Get(Entity.ID);
        set => NativeCalls.BoxColliderComponent_Rotation_Set(Entity.ID, ref value);
    }

    public Vector3f Size
    {
        get => NativeCalls.BoxColliderComponent_Size_Get(Entity.ID);
        set => NativeCalls.BoxColliderComponent_Size_Set(Entity.ID, ref value);
    }

    public bool IsSensor
    {
        get => NativeCalls.BoxColliderComponent_IsSensor_Get(Entity.ID);
        set => NativeCalls.BoxColliderComponent_IsSensor_Set(Entity.ID, value);
    }

    public uint Layer
    {
        get => NativeCalls.BoxColliderComponent_Layer_Get(Entity.ID);
        set => NativeCalls.BoxColliderComponent_Layer_Set(Entity.ID, value);
    }

    public uint Mask
    {
        get => NativeCalls.BoxColliderComponent_Mask_Get(Entity.ID);
        set => NativeCalls.BoxColliderComponent_Mask_Set(Entity.ID, value);
    }

    public float Density
    {
        get => NativeCalls.BoxColliderComponent_Density_Get(Entity.ID);
        set => NativeCalls.BoxColliderComponent_Density_Set(Entity.ID, value);
    }

    public float Friction
    {
        get => NativeCalls.BoxColliderComponent_Friction_Get(Entity.ID);
        set => NativeCalls.BoxColliderComponent_Friction_Set(Entity.ID, value);
    }

    public float Restitution
    {
        get => NativeCalls.BoxColliderComponent_Restitution_Get(Entity.ID);
        set => NativeCalls.BoxColliderComponent_Restitution_Set(Entity.ID, value);
    }
}

public sealed class SphereCollider : NativeComponent
{
    static SphereCollider()
    {
        NativeProxyFactory.Register<SphereCollider>(e => new SphereCollider { Entity = e });
    }

    public Vector3f Offset
    {
        get => NativeCalls.SphereColliderComponent_Offset_Get(Entity.ID);
        set => NativeCalls.SphereColliderComponent_Offset_Set(Entity.ID, ref value);
    }

    public Vector3f Rotation
    {
        get => NativeCalls.SphereColliderComponent_Rotation_Get(Entity.ID);
        set => NativeCalls.SphereColliderComponent_Rotation_Set(Entity.ID, ref value);
    }

    public float Radius
    {
        get => NativeCalls.SphereColliderComponent_Radius_Get(Entity.ID);
        set => NativeCalls.SphereColliderComponent_Radius_Set(Entity.ID, value);
    }

    public bool IsSensor
    {
        get => NativeCalls.SphereColliderComponent_IsSensor_Get(Entity.ID);
        set => NativeCalls.SphereColliderComponent_IsSensor_Set(Entity.ID, value);
    }

    public uint Layer
    {
        get => NativeCalls.SphereColliderComponent_Layer_Get(Entity.ID);
        set => NativeCalls.SphereColliderComponent_Layer_Set(Entity.ID, value);
    }

    public uint Mask
    {
        get => NativeCalls.SphereColliderComponent_Mask_Get(Entity.ID);
        set => NativeCalls.SphereColliderComponent_Mask_Set(Entity.ID, value);
    }

    public float Density
    {
        get => NativeCalls.SphereColliderComponent_Density_Get(Entity.ID);
        set => NativeCalls.SphereColliderComponent_Density_Set(Entity.ID, value);
    }

    public float Friction
    {
        get => NativeCalls.SphereColliderComponent_Friction_Get(Entity.ID);
        set => NativeCalls.SphereColliderComponent_Friction_Set(Entity.ID, value);
    }

    public float Restitution
    {
        get => NativeCalls.SphereColliderComponent_Restitution_Get(Entity.ID);
        set => NativeCalls.SphereColliderComponent_Restitution_Set(Entity.ID, value);
    }
}

public sealed class CapsuleCollider : NativeComponent
{
    static CapsuleCollider()
    {
        NativeProxyFactory.Register<CapsuleCollider>(e => new CapsuleCollider { Entity = e });
    }

    public Vector3f Offset
    {
        get => NativeCalls.CapsuleColliderComponent_Offset_Get(Entity.ID);
        set => NativeCalls.CapsuleColliderComponent_Offset_Set(Entity.ID, ref value);
    }

    public Vector3f Rotation
    {
        get => NativeCalls.CapsuleColliderComponent_Rotation_Get(Entity.ID);
        set => NativeCalls.CapsuleColliderComponent_Rotation_Set(Entity.ID, ref value);
    }

    public float Radius
    {
        get => NativeCalls.CapsuleColliderComponent_Radius_Get(Entity.ID);
        set => NativeCalls.CapsuleColliderComponent_Radius_Set(Entity.ID, value);
    }

    public float HalfHeight
    {
        get => NativeCalls.CapsuleColliderComponent_HalfHeight_Get(Entity.ID);
        set => NativeCalls.CapsuleColliderComponent_HalfHeight_Set(Entity.ID, value);
    }

    public bool IsSensor
    {
        get => NativeCalls.CapsuleColliderComponent_IsSensor_Get(Entity.ID);
        set => NativeCalls.CapsuleColliderComponent_IsSensor_Set(Entity.ID, value);
    }

    public uint Layer
    {
        get => NativeCalls.CapsuleColliderComponent_Layer_Get(Entity.ID);
        set => NativeCalls.CapsuleColliderComponent_Layer_Set(Entity.ID, value);
    }

    public uint Mask
    {
        get => NativeCalls.CapsuleColliderComponent_Mask_Get(Entity.ID);
        set => NativeCalls.CapsuleColliderComponent_Mask_Set(Entity.ID, value);
    }

    public float Density
    {
        get => NativeCalls.CapsuleColliderComponent_Density_Get(Entity.ID);
        set => NativeCalls.CapsuleColliderComponent_Density_Set(Entity.ID, value);
    }

    public float Friction
    {
        get => NativeCalls.CapsuleColliderComponent_Friction_Get(Entity.ID);
        set => NativeCalls.CapsuleColliderComponent_Friction_Set(Entity.ID, value);
    }

    public float Restitution
    {
        get => NativeCalls.CapsuleColliderComponent_Restitution_Get(Entity.ID);
        set => NativeCalls.CapsuleColliderComponent_Restitution_Set(Entity.ID, value);
    }
}
