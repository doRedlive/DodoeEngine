using GreenCake;
using System;
using System.Collections.Generic;

namespace OnlyOne
{
    [DisallowMultipleComponent]
    public class BlackTilePhaseGroup : CakeBehaviour
    {
        [SerializeField] private BoxCollider2dComponent[] boxCollidersInGroup;
        [SerializeField] private CircleCollider2dComponent[] circleCollidersInGroup;

        private NativeComponent[] _cachedColliders;

        public NativeComponent[] Colliders
        {
            get
            {
                if (_cachedColliders == null || _cachedColliders.Length == 0)
                {
                    var boxes = boxCollidersInGroup ?? Array.Empty<BoxCollider2dComponent>();
                    var circles = circleCollidersInGroup ?? Array.Empty<CircleCollider2dComponent>();
                    if (boxes.Length == 0 && circles.Length == 0)
                    {
                        var b = GetComponentsInChildren<BoxCollider2dComponent>(true);
                        var c = GetComponentsInChildren<CircleCollider2dComponent>(true);
                        boxes = b;
                        circles = c;
                    }
                    var list = new List<NativeComponent>(boxes.Length + circles.Length);
                    foreach (var x in boxes) if (x != null) list.Add(x);
                    foreach (var x in circles) if (x != null) list.Add(x);
                    _cachedColliders = list.ToArray();
                }
                return _cachedColliders;
            }
        }

        public void SetIgnored(NativeComponent target, bool ignored)
        {
            if (target == null)
            {
                return;
            }

            NativeComponent[] groupColliders = Colliders;
            for (int i = 0; i < groupColliders.Length; i++)
            {
                if (groupColliders[i] != null)
                {
                    Physics2D.IgnoreCollision(target, groupColliders[i], ignored);
                }
            }
        }

        public bool IsOverlapping(NativeComponent target)
        {
            if (target == null)
            {
                return false;
            }

            NativeComponent[] groupColliders = Colliders;
            for (int i = 0; i < groupColliders.Length; i++)
            {
                NativeComponent groupCollider = groupColliders[i];
                if (groupCollider == null)
                {
                    continue;
                }

                ColliderDistance2D distance = Physics2D.ComputeColliderDistance(target, groupCollider);
                if (distance.IsOverlapped)
                {
                    return true;
                }
            }

            return false;
        }

        public Vector2f GetExitGravity(Bounds playerBounds, Vector2f velocity, Vector2f currentGravity)
        {
            Bounds bounds = GetWorldBounds();

            Vector2f bestNormal = GetExitGravityFromVelocity(velocity, currentGravity);
            float bestDistance = float.NegativeInfinity;
            float bestAlignment = float.NegativeInfinity;

            TryCandidate(playerBounds.Max.x <= bounds.Min.x, Vector2f.Right, bounds.Min.x - playerBounds.Max.x, velocity, ref bestNormal, ref bestDistance, ref bestAlignment);
            TryCandidate(playerBounds.Min.x >= bounds.Max.x, Vector2f.Left, playerBounds.Min.x - bounds.Max.x, velocity, ref bestNormal, ref bestDistance, ref bestAlignment);
            TryCandidate(playerBounds.Max.y <= bounds.Min.y, Vector2f.Up, bounds.Min.y - playerBounds.Max.y, velocity, ref bestNormal, ref bestDistance, ref bestAlignment);
            TryCandidate(playerBounds.Min.y >= bounds.Max.y, Vector2f.Down, playerBounds.Min.y - bounds.Max.y, velocity, ref bestNormal, ref bestDistance, ref bestAlignment);

            return bestNormal;
        }

        public Bounds GetWorldBounds()
        {
            NativeComponent[] groupColliders = Colliders;
            Bounds bounds = new Bounds(new Vector2f(Transform.Position.x, Transform.Position.y), new Vector2f(0f, 0f));
            bool hasBounds = false;

            for (int i = 0; i < groupColliders.Length; i++)
            {
                NativeComponent groupCollider = groupColliders[i];
                if (groupCollider == null)
                {
                    continue;
                }

                Bounds cb = Physics2D.ComputeColliderBounds(groupCollider);
                if (!hasBounds)
                {
                    bounds = cb;
                    hasBounds = true;
                }
                else
                {
                    bounds.Encapsulate(cb.Min);
                    bounds.Encapsulate(cb.Max);
                }
            }

            return bounds;
        }

        private static void TryCandidate(bool valid, Vector2f normal, float distance, Vector2f velocity, ref Vector2f bestNormal, ref float bestDistance, ref float bestAlignment)
        {
            if (!valid)
            {
                return;
            }

            float alignment = Vector2f.Dot(velocity, normal);
            if (distance > bestDistance + 0.001f || (MathF.Abs(distance - bestDistance) <= 0.001f && alignment > bestAlignment))
            {
                bestNormal = normal;
                bestDistance = distance;
                bestAlignment = alignment;
            }
        }

        private static Vector2f GetExitGravityFromVelocity(Vector2f velocity, Vector2f currentGravity)
        {
            float sqrMag = velocity.x * velocity.x + velocity.y * velocity.y;
            if (sqrMag < 0.0025f)
            {
                return new Vector2f(-currentGravity.x, -currentGravity.y);
            }

            if (MathF.Abs(velocity.x) > MathF.Abs(velocity.y))
            {
                return velocity.x > 0f ? Vector2f.Left : Vector2f.Right;
            }

            return velocity.y > 0f ? Vector2f.Down : Vector2f.Up;
        }
    }
}
