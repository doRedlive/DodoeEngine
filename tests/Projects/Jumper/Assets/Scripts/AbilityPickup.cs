using GreenCake;
using System;

namespace OnlyOne
{
    [RequireComponent(typeof(BoxCollider2dComponent))]
    [DisallowMultipleComponent]
    public class AbilityPickup : CakeBehaviour
    {
        public enum AbilityType
        {
            Dash,
            Shockwave
        }

        [SerializeField] private string pickupId;
        [SerializeField] private AbilityType abilityType;

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

        private void Start()
        {
            if (!string.IsNullOrWhiteSpace(pickupId) && PlayerPrefs.GetInt("ability_pickup_" + pickupId, 0) == 1)
            {
                if (GameObject != null) GameObject.ActiveSelf = false;
            }
        }

        private void OnTriggerEnter2D(NativeComponent other)
        {
            var go = other.GetGameObject();
            var player = go != null ? go.GetComponent<PlatformerPlayerController>() : null;
            if (player == null)
            {
                return;
            }

            if (abilityType == AbilityType.Dash)
            {
                player.UnlockDash();
            }
            else
            {
                player.UnlockShockwave();
            }

            if (!string.IsNullOrWhiteSpace(pickupId))
            {
                PlayerPrefs.SetInt("ability_pickup_" + pickupId, 1);
                PlayerPrefs.Save();
            }

            if (GameObject != null) GameObject.ActiveSelf = false;
        }
    }
}
