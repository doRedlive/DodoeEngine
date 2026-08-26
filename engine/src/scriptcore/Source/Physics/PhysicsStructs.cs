namespace GreenCake;

using System;

public struct ContactPoint2D
{
    public Vector2f Point;
    public Vector2f Normal;
    public float RelativeSpeed;
    public float Separation;
}

public struct Collision2D
{
    public NativeComponent Collider;
    public GameObject GameObject;
    public ContactPoint2D[] Contacts;
    public int ContactCount;
    public Vector2f RelativeVelocity;
    public Rigidbody2dComponent Rigidbody;
}

public struct RaycastHit2D
{
    public GameObject GameObject;
    public NativeComponent Collider;
    public Rigidbody2dComponent Rigidbody;
    public TransformComponent Transform;
    public Vector2f Point;
    public Vector2f Normal;
    public Vector2f Centroid;
    public float Fraction;
    public float Distance;
}

public struct OverlapHit2D
{
    public GameObject GameObject;
    public NativeComponent Collider;
    public Rigidbody2dComponent Rigidbody;
    public TransformComponent Transform;
}

public struct ColliderDistance2D
{
    public Vector2f PointA;
    public Vector2f PointB;
    public Vector2f Normal;
    public float Distance;
    public bool IsOverlapped => Distance < 0f;
    public bool IsValid;

    public static ColliderDistance2D Invalid => new() { IsValid = false };
}

public enum RigidbodyType2D
{
    Dynamic = 1,
    Kinematic = 2,
    Static = 0
}
