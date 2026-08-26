using GreenCake;
using System;

namespace OnlyOne
{
    [RequireComponent(typeof(BoxCollider2dComponent))]
    [DisallowMultipleComponent]
    public class CheckpointTrigger : CakeBehaviour
    {
        [SerializeField] private string checkpointId;
        [SerializeField] private TransformComponent respawnPoint;

        public string CheckpointId
        {
            get { return checkpointId; }
        }

        public Vector2f RespawnPosition
        {
            get
            {
                if (respawnPoint != null)
                {
                    return new Vector2f(respawnPoint.Position.x, respawnPoint.Position.y);
                }
                return new Vector2f(Transform.Position.x, Transform.Position.y);
            }
        }

        private void Awake()
        {
            var trigger = GetComponent<BoxCollider2dComponent>();
            if (trigger == null)
            {
                var cc = GetComponent<CircleCollider2dComponent>();
                if (cc != null) cc.SetIsSensor(true);
            }
            else
            {
                trigger.SetIsSensor(true);
            }
        }

        private void OnTriggerEnter2D(NativeComponent other)
        {
            var go = other.GetGameObject();
            var player = go != null ? go.GetComponent<PlatformerPlayerController>() : null;
            if (player != null)
            {
                player.ActivateCheckpoint(this);
            }
        }
    }
}
