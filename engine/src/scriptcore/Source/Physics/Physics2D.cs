namespace GreenCake;

using System;
using System.Collections.Generic;

public static class Physics2D
{
    [ThreadStatic]
    private static List<ulong> _contactRegistryKeys;
    [ThreadStatic]
    private static List<List<ulong>> _contactRegistryValues;
    [ThreadStatic]
    private static float[] _rayHitBuffer;
    [ThreadStatic]
    private static uint[] _overlapIdBuffer;

    public static Vector2f gravity
    {
        get => new(0, -9.81f);
        set { }
    }

    private static bool IsCollider(NativeComponent c)
    {
        if (c == null) return false;
        return c is BoxCollider2dComponent or CircleCollider2dComponent;
    }

    private static uint GetColliderLayer(NativeComponent c)
    {
        if (c is BoxCollider2dComponent b) return NativeCalls.BoxCollider2dComponent_Layer_Get(b.Entity.ID);
        if (c is CircleCollider2dComponent ci) return NativeCalls.CircleCollider2dComponent_Layer_Get(ci.Entity.ID);
        return 1u;
    }

    public static Bounds ComputeColliderBounds(NativeComponent c)
    {
        var t = c?.Entity?.GetComponent<TransformComponent>();
        var center = t != null ? new Vector2f(t.Position.x, t.Position.y) : Vector2f.Zero;
        var scale = t != null ? new Vector2f(t.Scale.x, t.Scale.y) : new Vector2f(1, 1);
        if (c is BoxCollider2dComponent b)
        {
            var off = b.Offset;
            var sz = b.Size;
            var cx = center.x + off.x * scale.x;
            var cy = center.y + off.y * scale.y;
            var sx = sz.x * MathF.Abs(scale.x);
            var sy = sz.y * MathF.Abs(scale.y);
            return new Bounds(new Vector2f(cx, cy), new Vector2f(sx, sy));
        }
        if (c is CircleCollider2dComponent ci)
        {
            var off = ci.Offset;
            var r = ci.Radius;
            var s = MathF.Max(MathF.Abs(scale.x), MathF.Abs(scale.y));
            var diameter = r * 2 * s;
            var cx = center.x + off.x * scale.x;
            var cy = center.y + off.y * scale.y;
            return new Bounds(new Vector2f(cx, cy), new Vector2f(diameter, diameter));
        }
        return new Bounds(center, new Vector2f(1, 1));
    }

    private static NativeComponent FindAnyCollider(GameObject go)
    {
        if (go == null) return null;
        var b = go.GetComponent<BoxCollider2dComponent>();
        if (b != null) return b;
        var ci = go.GetComponent<CircleCollider2dComponent>();
        return ci;
    }

    public static RaycastHit2D Raycast(Vector2f origin, Vector2f direction, float distance, uint layerMask = uint.MaxValue)
    {
        var results = new RaycastHit2D[1];
        RaycastNonAlloc(origin, direction, results, distance, layerMask);
        return results[0];
    }

    private static uint RequireUintBytes() { return 1u << 0; }

    public static unsafe int RaycastNonAlloc(Vector2f origin, Vector2f direction, RaycastHit2D[] results, float distance, uint layerMask = uint.MaxValue)
    {
        if (results == null || results.Length == 0) return 0;
        for (int i = 0; i < results.Length; i++) results[i] = default;
        int cap = results.Length;
        if (_rayHitBuffer == null || _rayHitBuffer.Length < cap * 8) _rayHitBuffer = new float[cap * 8];
        var scene = SceneManager.ActiveScene;
        if (scene == null) return 0;
        int n;
        fixed (float* buf = _rayHitBuffer)
        {
            n = NativeCalls.Physics2d_Raycast(origin.x, origin.y, direction.x, direction.y, distance,
                                              1u, layerMask, 0f, buf, cap);
        }
        if (n <= 0) return 0;
        if (n > cap) n = cap;
        int written = 0;
        for (int i = 0; i < n; i++)
        {
            int s = i * 8;
            uint id = BitConverter.ToUInt32(BitConverter.GetBytes(_rayHitBuffer[s + 0]), 0);
            ulong longId = id;
            var go = scene.FindByID(longId);
            if (go == null) continue;
            var col = FindAnyCollider(go);
            results[written] = new RaycastHit2D
            {
                GameObject = go,
                Collider = col,
                Rigidbody = go.GetComponent<Rigidbody2dComponent>(),
                Transform = go.GetComponent<TransformComponent>(),
                Point = new Vector2f(_rayHitBuffer[s + 1], _rayHitBuffer[s + 2]),
                Normal = new Vector2f(_rayHitBuffer[s + 3], _rayHitBuffer[s + 4]),
                Fraction = _rayHitBuffer[s + 5],
                Distance = _rayHitBuffer[s + 5] * distance
            };
            written++;
        }
        return written;
    }

    public static RaycastHit2D BoxCast(Vector2f center, Vector2f size, float angle, Vector2f direction, float distance, uint layerMask = uint.MaxValue)
    {
        var results = new RaycastHit2D[1];
        BoxCastNonAlloc(center, size, angle, direction, results, distance, layerMask);
        return results[0];
    }

    public static unsafe int BoxCastNonAlloc(Vector2f center, Vector2f size, float angle, Vector2f direction, RaycastHit2D[] results, float distance, uint layerMask = uint.MaxValue)
    {
        if (results == null || results.Length == 0) return 0;
        for (int i = 0; i < results.Length; i++) results[i] = default;
        int cap = results.Length;
        if (_rayHitBuffer == null || _rayHitBuffer.Length < cap * 8) _rayHitBuffer = new float[cap * 8];
        var scene = SceneManager.ActiveScene;
        if (scene == null) return 0;
        var half = size * 0.5f;
        int n;
        fixed (float* buf = _rayHitBuffer)
        {
            n = NativeCalls.Physics2d_BoxCast(center.x, center.y, half.x, half.y, angle,
                                              direction.x, direction.y, distance, 1u, layerMask, buf, cap);
        }
        if (n <= 0) return 0;
        if (n > cap) n = cap;
        int written = 0;
        for (int i = 0; i < n; i++)
        {
            int s = i * 8;
            uint id = BitConverter.ToUInt32(BitConverter.GetBytes(_rayHitBuffer[s + 0]), 0);
            ulong longId = id;
            var go = scene.FindByID(longId);
            if (go == null) continue;
            var col = FindAnyCollider(go);
            results[written] = new RaycastHit2D
            {
                GameObject = go,
                Collider = col,
                Rigidbody = go.GetComponent<Rigidbody2dComponent>(),
                Transform = go.GetComponent<TransformComponent>(),
                Point = new Vector2f(_rayHitBuffer[s + 1], _rayHitBuffer[s + 2]),
                Normal = new Vector2f(_rayHitBuffer[s + 3], _rayHitBuffer[s + 4]),
                Fraction = _rayHitBuffer[s + 5],
                Distance = _rayHitBuffer[s + 5] * distance
            };
            written++;
        }
        return written;
    }

    public static int OverlapAABBNonAlloc(Vector2f center, Vector2f halfSize, OverlapHit2D[] results, uint layerMask = uint.MaxValue)
    {
        if (results == null || results.Length == 0) return 0;
        for (int i = 0; i < results.Length; i++) results[i] = default;
        int cap = results.Length;
        if (_overlapIdBuffer == null || _overlapIdBuffer.Length < cap) _overlapIdBuffer = new uint[cap];
        var scene = SceneManager.ActiveScene;
        if (scene == null) return 0;
        int n;
        unsafe
        {
            fixed (uint* buf = _overlapIdBuffer)
            {
                n = NativeCalls.Physics2d_OverlapAABB(center.x, center.y, halfSize.x, halfSize.y,
                                                      1u, layerMask, buf, cap, 1);
            }
        }
        if (n <= 0) return 0;
        if (n > cap) n = cap;
        int written = 0;
        for (int i = 0; i < n; i++)
        {
            ulong id = _overlapIdBuffer[i];
            var go = scene.FindByID(id);
            if (go == null) continue;
            var col = FindAnyCollider(go);
            results[written] = new OverlapHit2D
            {
                GameObject = go,
                Collider = col,
                Rigidbody = go.GetComponent<Rigidbody2dComponent>(),
                Transform = go.GetComponent<TransformComponent>()
            };
            written++;
        }
        return written;
    }

    public static void IgnoreCollision(NativeComponent a, NativeComponent b, bool ignore = true)
    {
        if (a == null || b == null) return;
        NativeCalls.Physics2d_IgnoreCollision(a.Entity.ID, b.Entity.ID, ignore);
    }

    public static int GetColliderContacts(NativeComponent self, NativeComponent[] results)
    {
        if (results == null || results.Length == 0 || self == null) return 0;
        for (int i = 0; i < results.Length; i++) results[i] = null;
        int cap = results.Length;
        if (_overlapIdBuffer == null || _overlapIdBuffer.Length < cap) _overlapIdBuffer = new uint[cap];
        int n;
        unsafe
        {
            fixed (uint* buf = _overlapIdBuffer)
            {
                n = NativeCalls.Physics2d_GetColliderContacts(self.Entity.ID, buf, cap, 1);
            }
        }
        if (n <= 0)
        {
            var ab = ComputeColliderBounds(self);
            int slowN = 0;
            var scene = SceneManager.ActiveScene;
            if (scene == null) return 0;
            var selfEntity = self.Entity?.ID ?? 0;
            foreach (var go in scene.GetAllGameObjects())
            {
                var col = FindAnyCollider(go);
                if (col == null || col == self) continue;
                if ((col.Entity?.ID ?? 0) == selfEntity) continue;
                if (ab.Intersects(ComputeColliderBounds(col)))
                {
                    results[slowN++] = col;
                    if (slowN >= results.Length) break;
                }
            }
            return slowN;
        }
        if (n > cap) n = cap;
        var scn = SceneManager.ActiveScene;
        int written = 0;
        for (int i = 0; i < n; i++)
        {
            ulong id = _overlapIdBuffer[i];
            var go = scn?.FindByID(id);
            if (go == null) continue;
            var col = FindAnyCollider(go);
            if (col == null) continue;
            results[written++] = col;
        }
        return written;
    }

    public static ColliderDistance2D ComputeColliderDistance(NativeComponent a, NativeComponent b)
    {
        if (a == null || b == null) return ColliderDistance2D.Invalid;
        float distOut = 0f;
        int r;
        unsafe
        {
            float d = 0;
            r = NativeCalls.Physics2d_ColliderDistance(a.Entity.ID, b.Entity.ID, &d);
            distOut = d;
        }
        if (r == 0)
        {
            var ba = ComputeColliderBounds(a);
            var bb = ComputeColliderBounds(b);
            bool overlap = ba.Intersects(bb);
            var ca = ba.Center;
            var cb = bb.Center;
            var diff = ca - cb;
            float dist = diff.Length;
            if (overlap) dist = -MathF.Max(ba.Extents.x + bb.Extents.x - MathF.Abs(diff.x), ba.Extents.y + bb.Extents.y - MathF.Abs(diff.y));
            var normal = dist > 1e-6f ? diff / dist : Vector2f.Up;
            return new ColliderDistance2D
            {
                IsValid = true,
                Distance = dist,
                Normal = normal,
                PointA = ca - normal * ba.Extents.x,
                PointB = cb + normal * bb.Extents.x
            };
        }
        var ca2 = ComputeColliderBounds(a).Center;
        var cb2 = ComputeColliderBounds(b).Center;
        var diff2 = ca2 - cb2;
        float len = diff2.Length;
        var normal2 = len > 1e-6f ? diff2 / len : Vector2f.Up;
        return new ColliderDistance2D
        {
            IsValid = true,
            Distance = distOut,
            Normal = normal2,
            PointA = ca2,
            PointB = cb2
        };
    }

    private static bool LayerMatches(uint layer, uint mask)
    {
        if (layer == 0) return true;
        int layerNum = 0;
        uint l = layer;
        while ((l & 1u) == 0 && l != 0) { layerNum++; l >>= 1; }
        if (layerNum >= 32) return true;
        return (mask & (1u << layerNum)) != 0;
    }

    private static bool RayVsBounds(Vector2f origin, Vector2f dir, float dist, Bounds b, out float fraction, out Vector2f normal, out Vector2f point)
    {
        fraction = 0f; normal = Vector2f.Zero; point = origin;
        var invDx = MathF.Abs(dir.x) < 1e-8f ? 1e8f : 1f / dir.x;
        var invDy = MathF.Abs(dir.y) < 1e-8f ? 1e8f : 1f / dir.y;
        var min = b.Min; var max = b.Max;
        float tx1 = (min.x - origin.x) * invDx;
        float tx2 = (max.x - origin.x) * invDx;
        float ty1 = (min.y - origin.y) * invDy;
        float ty2 = (max.y - origin.y) * invDy;
        float tmin = MathF.Max(MathF.Min(tx1, tx2), MathF.Min(ty1, ty2));
        float tmax = MathF.Min(MathF.Max(tx1, tx2), MathF.Max(ty1, ty2));
        if (tmax < 0 || tmin > tmax || tmin > dist) return false;
        fraction = MathF.Max(tmin, 0f);
        point = origin + dir * fraction;
        if (MathF.Min(tx1, tx2) > MathF.Min(ty1, ty2))
            normal = dir.x < 0 ? Vector2f.Right : Vector2f.Left;
        else
            normal = dir.y < 0 ? Vector2f.Up : Vector2f.Down;
        return true;
    }

    internal static event Action<CollisionEventKind, ulong, ulong, bool, Vector2f, Vector2f, float> OnRawCollision;

    public static void ProcessEvents()
    {
    }
}

public enum CollisionEventKind { Enter, Stay, Exit }
