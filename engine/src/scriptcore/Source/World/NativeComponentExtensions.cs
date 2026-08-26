namespace GreenCake;

using System;

public static class NativeComponentExtensions
{
    public static GameObject GetGameObject(this NativeComponent c)
    {
        if (c?.Entity == null) return null;
        return SceneManager.ActiveScene?.FindByID(c.Entity.ID);
    }

    public static bool GetIsSensor(this BoxCollider2dComponent c)
    {
        if (c?.Entity == null) return false;
        return NativeCalls.BoxCollider2dComponent_IsSensor_Get(c.Entity.ID);
    }

    public static void SetIsSensor(this BoxCollider2dComponent c, bool v)
    {
        if (c?.Entity == null) return;
        NativeCalls.BoxCollider2dComponent_IsSensor_Set(c.Entity.ID, v);
    }

    public static uint GetLayer(this BoxCollider2dComponent c)
    {
        if (c?.Entity == null) return 0u;
        return NativeCalls.BoxCollider2dComponent_Layer_Get(c.Entity.ID);
    }

    public static void SetLayer(this BoxCollider2dComponent c, uint v)
    {
        if (c?.Entity == null) return;
        NativeCalls.BoxCollider2dComponent_Layer_Set(c.Entity.ID, v);
    }

    public static uint GetMask(this BoxCollider2dComponent c)
    {
        if (c?.Entity == null) return 0u;
        return NativeCalls.BoxCollider2dComponent_Mask_Get(c.Entity.ID);
    }

    public static void SetMask(this BoxCollider2dComponent c, uint v)
    {
        if (c?.Entity == null) return;
        NativeCalls.BoxCollider2dComponent_Mask_Set(c.Entity.ID, v);
    }

    public static bool GetEnabled(this BoxCollider2dComponent c)
    {
        return c != null;
    }

    public static void SetEnabled(this BoxCollider2dComponent c, bool v)
    {
    }

    public static bool GetIsSensor(this CircleCollider2dComponent c)
    {
        if (c?.Entity == null) return false;
        return NativeCalls.CircleCollider2dComponent_IsSensor_Get(c.Entity.ID);
    }

    public static void SetIsSensor(this CircleCollider2dComponent c, bool v)
    {
        if (c?.Entity == null) return;
        NativeCalls.CircleCollider2dComponent_IsSensor_Set(c.Entity.ID, v);
    }

    public static uint GetLayer(this CircleCollider2dComponent c)
    {
        if (c?.Entity == null) return 0u;
        return NativeCalls.CircleCollider2dComponent_Layer_Get(c.Entity.ID);
    }

    public static void SetLayer(this CircleCollider2dComponent c, uint v)
    {
        if (c?.Entity == null) return;
        NativeCalls.CircleCollider2dComponent_Layer_Set(c.Entity.ID, v);
    }

    public static uint GetMask(this CircleCollider2dComponent c)
    {
        if (c?.Entity == null) return 0u;
        return NativeCalls.CircleCollider2dComponent_Mask_Get(c.Entity.ID);
    }

    public static void SetMask(this CircleCollider2dComponent c, uint v)
    {
        if (c?.Entity == null) return;
        NativeCalls.CircleCollider2dComponent_Mask_Set(c.Entity.ID, v);
    }

    public static bool GetEnabled(this CircleCollider2dComponent c)
    {
        return c != null;
    }

    public static void SetEnabled(this CircleCollider2dComponent c, bool v)
    {
    }

    public static RigidbodyType2D GetBodyType(this Rigidbody2dComponent c)
    {
        if (c?.Entity == null) return RigidbodyType2D.Static;
        return (RigidbodyType2D)NativeCalls.Rigidbody2dComponent_Type_Get(c.Entity.ID);
    }

    public static void SetBodyType(this Rigidbody2dComponent c, RigidbodyType2D v)
    {
        if (c?.Entity == null) return;
        NativeCalls.Rigidbody2dComponent_Type_Set(c.Entity.ID, (int)v);
    }

    public static Vector2f GetVelocity(this Rigidbody2dComponent c)
    {
        if (c?.Entity == null) return Vector2f.Zero;
        return NativeCalls.Rigidbody2dComponent_Velocity_Get(c.Entity.ID);
    }

    public static void SetVelocity(this Rigidbody2dComponent c, Vector2f v)
    {
        if (c?.Entity == null) return;
        NativeCalls.Rigidbody2dComponent_SetVelocity(c.Entity.ID, v.x, v.y);
    }

    public static void MovePosition(this Rigidbody2dComponent c, Vector2f pos)
    {
        if (c?.Entity == null) return;
        NativeCalls.Rigidbody2dComponent_MovePosition(c.Entity.ID, pos.x, pos.y);
    }

    public static void ApplyForce(this Rigidbody2dComponent c, Vector2f force)
    {
        if (c?.Entity == null) return;
        NativeCalls.Rigidbody2dComponent_ApplyForce(c.Entity.ID, force.x, force.y);
    }

    public static void ApplyImpulse(this Rigidbody2dComponent c, Vector2f impulse)
    {
        if (c?.Entity == null) return;
        NativeCalls.Rigidbody2dComponent_ApplyImpulse(c.Entity.ID, impulse.x, impulse.y);
    }

    public static bool GetVisible(this SpriteRendererComponent c)
    {
        return c != null;
    }

    public static void SetVisible(this SpriteRendererComponent c, bool v)
    {
    }

    public static void Play(this AudioSourceComponent c)
    {
        if (c?.Entity == null) return;
        NativeCalls.Native_AudioSource_Play(c.Entity.ID);
    }

    public static void Stop(this AudioSourceComponent c)
    {
        if (c?.Entity == null) return;
        NativeCalls.Native_AudioSource_Stop(c.Entity.ID);
    }

    public static void Pause(this AudioSourceComponent c)
    {
        if (c?.Entity == null) return;
        NativeCalls.Native_AudioSource_Pause(c.Entity.ID);
    }

    public static void UnPause(this AudioSourceComponent c)
    {
        if (c?.Entity == null) return;
        NativeCalls.Native_AudioSource_UnPause(c.Entity.ID);
    }

    public static bool GetIsPlaying(this AudioSourceComponent c)
    {
        if (c?.Entity == null) return false;
        return NativeCalls.Native_AudioSource_IsPlaying(c.Entity.ID);
    }
}
