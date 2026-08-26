namespace GreenCake;

using System;
using System.Collections.Generic;

internal class CakeBehaviourSystem : CakeSystem
{
    public override void OnUpdate()
    {
        var scene = SceneManager.ActiveScene;
        BehaviourBinder.BindOrphans(scene);
        if (scene != null)
        {
            scene.NotifyUpdate(Time.DeltaTime);
        }
    }

    public override void OnFixedUpdate()
    {
        var scene = SceneManager.ActiveScene;
        BehaviourBinder.BindOrphans(scene);
        if (scene != null)
        {
            scene.NotifyFixedUpdate(Time.FixedDeltaTime);
        }
        DispatchCollisionEvents(scene);
    }

    private struct PendingCollision
    {
        public ulong EntityA;
        public ulong EntityB;
        public Vector2f Point;
        public Vector2f Normal;
        public float RelativeSpeed;
        public bool IsSensor;
        public int Phase;
    }

    private static readonly List<PendingCollision> _pending = new();
    private static readonly float[] _eventBuffer = new float[9];

    private static unsafe void DispatchCollisionEvents(Scene scene)
    {
        if (scene == null) return;

        int count = NativeCalls.Physics2d_PollEventCount();
        if (count <= 0) return;

        _pending.Clear();
        _pending.Capacity = Math.Max(_pending.Capacity, count);

        for (int i = 0; i < count; i++)
        {
            fixed (float* buf = _eventBuffer)
            {
                if (NativeCalls.Physics2d_GetEvent(i, buf) == 0) continue;
            }
            uint ea = BitConverter.ToUInt32(BitConverter.GetBytes(_eventBuffer[0]), 0);
            uint eb = BitConverter.ToUInt32(BitConverter.GetBytes(_eventBuffer[1]), 0);
            _pending.Add(new PendingCollision
            {
                EntityA = ea,
                EntityB = eb,
                Point = new Vector2f(_eventBuffer[2], _eventBuffer[3]),
                Normal = new Vector2f(_eventBuffer[4], _eventBuffer[5]),
                RelativeSpeed = _eventBuffer[6],
                IsSensor = _eventBuffer[7] != 0f,
                Phase = (int)_eventBuffer[8]
            });
        }

        foreach (var ev in _pending)
        {
            var goA = scene.FindByID(ev.EntityA);
            var goB = scene.FindByID(ev.EntityB);
            if (goA == null && goB == null) continue;

            if (ev.IsSensor)
            {
                DispatchTrigger(goA, goB, ev.Phase);
                DispatchTrigger(goB, goA, ev.Phase);
            }
            else
            {
                var colA = BuildCollision(goA, goB, ev);
                var colB = BuildCollision(goB, goA, ev);
                DispatchCollision(goA, colA, ev.Phase);
                DispatchCollision(goB, colB, ev.Phase);
            }
        }
    }

    private static Collision2D BuildCollision(GameObject self, GameObject other, PendingCollision ev)
    {
        NativeComponent otherCollider = null;
        if (other != null)
        {
            otherCollider = other.GetComponent<BoxCollider2dComponent>();
            if (otherCollider == null) otherCollider = other.GetComponent<CircleCollider2dComponent>();
        }
        var contact = new ContactPoint2D
        {
            Point = ev.Point,
            Normal = ev.Normal,
            RelativeSpeed = ev.RelativeSpeed,
            Separation = 0f
        };
        return new Collision2D
        {
            Collider = otherCollider,
            GameObject = other,
            Rigidbody = other != null ? other.GetComponent<Rigidbody2dComponent>() : null,
            Contacts = new[] { contact },
            ContactCount = 1,
            RelativeVelocity = ev.Normal * ev.RelativeSpeed
        };
    }

    private static void DispatchCollision(GameObject go, Collision2D col, int phase)
    {
        if (go == null) return;
        foreach (var c in go.GetComponents<CakeBehaviour>())
        {
            if (c == null || !c.Enabled) continue;
            try
            {
                switch (phase)
                {
                    case 0: c.OnCollisionEnter2D(col); break;
                    case 1: c.OnCollisionExit2D(col); break;
                    case 2: c.OnCollisionStay2D(col); break;
                }
            }
            catch (Exception e) { Debug.LogError($"Collision dispatch: {e}"); }
        }
    }

    private static void DispatchTrigger(GameObject self, GameObject other, int phase)
    {
        if (self == null || other == null) return;
        NativeComponent otherCollider = other.GetComponent<BoxCollider2dComponent>();
        if (otherCollider == null) otherCollider = other.GetComponent<CircleCollider2dComponent>();
        if (otherCollider == null) return;
        foreach (var c in self.GetComponents<CakeBehaviour>())
        {
            if (c == null || !c.Enabled) continue;
            try
            {
                switch (phase)
                {
                    case 0: c.OnTriggerEnter2D(otherCollider); break;
                    case 1: c.OnTriggerExit2D(otherCollider); break;
                    case 2: c.OnTriggerStay2D(otherCollider); break;
                }
            }
            catch (Exception e) { Debug.LogError($"Trigger dispatch: {e}"); }
        }
    }
}
