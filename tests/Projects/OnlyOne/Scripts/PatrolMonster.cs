using GreenCake;
using System;
using System.Collections.Generic;

namespace OnlyOne
{
    [RequireComponent(typeof(Rigidbody2dComponent))]
    [RequireComponent(typeof(BoxCollider2dComponent))]
    [DisallowMultipleComponent]
    public class PatrolMonster : CakeBehaviour
    {
        private static readonly List<PatrolMonster> AllMonsters = new List<PatrolMonster>();

        [SerializeField] private Vector2f gravityDirection = new Vector2f(0f, -1f);
        [SerializeField] private float moveSpeed = 1.45f;
        [SerializeField] private float wallCheckDistance = 0.08f;
        [SerializeField] private float groundCheckDistance = 0.18f;
        [SerializeField] private float probePadding = 0.08f;
        [SerializeField] private LayerMask groundMask;

        private Rigidbody2dComponent rb;
        private BoxCollider2dComponent bodyBox;
        private CircleCollider2dComponent bodyCircle;
        private int direction = 1;
        private Vector2f startPosition;

        public Vector2f GravityDirection
        {
            get { return Cardinalize(gravityDirection); }
        }

        public Vector2f CurrentVelocity { get; private set; }

        private void Awake()
        {
            rb = GetComponent<Rigidbody2dComponent>();
            bodyBox = GetComponent<BoxCollider2dComponent>();
            bodyCircle = GetComponent<CircleCollider2dComponent>();

            if (rb != null)
            {
                rb.GravityScale = 0f;
                rb.FixedRotation = true;
                rb.SetBodyType(RigidbodyType2D.Kinematic);
                startPosition = new Vector2f(Transform.Position.x, Transform.Position.y);
            }
        }

        private void OnEnable()
        {
            if (!AllMonsters.Contains(this))
            {
                AllMonsters.Add(this);
            }
        }

        private void OnDisable()
        {
            AllMonsters.Remove(this);
        }

        private void FixedUpdate()
        {
            Vector2f gravity = GravityDirection;
            Vector2f tangent = new Vector2f(-gravity.y, gravity.x);

            if (ShouldTurn(gravity, tangent))
            {
                direction *= -1;
            }

            CurrentVelocity = tangent * (direction * moveSpeed);
            if (rb != null)
            {
                var curPos = new Vector2f(Transform.Position.x, Transform.Position.y);
                rb.MovePosition(curPos + CurrentVelocity * Time.FixedDeltaTime);
            }
        }

        public void ResetMonster()
        {
            direction = 1;
            CurrentVelocity = Vector2f.Zero;
            if (rb != null)
            {
                rb.MovePosition(startPosition);
                rb.SetVelocity(Vector2f.Zero);
            }
        }

        public static void ResetAll()
        {
            for (int i = 0; i < AllMonsters.Count; i++)
            {
                if (AllMonsters[i] != null)
                {
                    AllMonsters[i].ResetMonster();
                }
            }
        }

        private bool ShouldTurn(Vector2f gravity, Vector2f tangent)
        {
            Bounds bounds = GetBodyBounds();
            Vector2f moveDir = tangent * direction;
            Vector2f center = bounds.Center;
            Vector2f size = bounds.Size;

            RaycastHit2D wallHit = Physics2D.BoxCast(center, size * 0.95f, 0f, moveDir, wallCheckDistance, groundMask.value);
            if (wallHit.Collider != null && wallHit.Collider != (NativeComponent)bodyBox && wallHit.Collider != (NativeComponent)bodyCircle)
            {
                return true;
            }

            float tangentExtent = MathF.Abs(tangent.x) * bounds.Extents.x + MathF.Abs(tangent.y) * bounds.Extents.y;
            float gravityExtent = MathF.Abs(gravity.x) * bounds.Extents.x + MathF.Abs(gravity.y) * bounds.Extents.y;
            Vector2f probeOrigin = center + moveDir * (tangentExtent + probePadding) + gravity * (gravityExtent + probePadding);

            RaycastHit2D groundHit = Physics2D.Raycast(probeOrigin, gravity, groundCheckDistance, groundMask.value);
            return groundHit.Collider == null;
        }

        private Bounds GetBodyBounds()
        {
            if (bodyBox != null)
            {
                var center = new Vector2f(Transform.Position.x, Transform.Position.y) + new Vector2f(bodyBox.Offset.x, bodyBox.Offset.y);
                var size = new Vector2f(bodyBox.Size.x, bodyBox.Size.y);
                return new Bounds(center, size);
            }
            if (bodyCircle != null)
            {
                var center = new Vector2f(Transform.Position.x, Transform.Position.y) + new Vector2f(bodyCircle.Offset.x, bodyCircle.Offset.y);
                var d = bodyCircle.Radius * 2f;
                return new Bounds(center, new Vector2f(d, d));
            }
            return new Bounds(new Vector2f(Transform.Position.x, Transform.Position.y), new Vector2f(1f, 1f));
        }

        private static Vector2f Cardinalize(Vector2f value)
        {
            if (MathF.Abs(value.x) > MathF.Abs(value.y))
            {
                return value.x >= 0f ? Vector2f.Right : Vector2f.Left;
            }

            return value.y >= 0f ? Vector2f.Up : Vector2f.Down;
        }
    }
}
