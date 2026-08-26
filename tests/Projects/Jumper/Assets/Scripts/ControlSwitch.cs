using GreenCake;
using System;
using System.Collections.Generic;

namespace OnlyOne
{
    [RequireComponent(typeof(BoxCollider2dComponent))]
    [DisallowMultipleComponent]
    public class ControlSwitch : CakeBehaviour
    {
        [SerializeField] private string saveId;
        [SerializeField] private List<ControlTile> controlledTiles = new List<ControlTile>();
        [SerializeField] private bool startOn;

        private readonly HashSet<NativeComponent> insideColliders = new HashSet<NativeComponent>();
        private bool isOn;

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
            isOn = LoadState(startOn);
        }

        private void OnTriggerEnter2D(NativeComponent other)
        {
            if (!IsValidTrigger(other) || insideColliders.Contains(other))
            {
                return;
            }

            insideColliders.Add(other);
            Toggle();
        }

        private void OnTriggerExit2D(NativeComponent other)
        {
            insideColliders.Remove(other);
        }

        public void Toggle()
        {
            isOn = !isOn;

            for (int i = 0; i < controlledTiles.Count; i++)
            {
                if (controlledTiles[i] != null)
                {
                    controlledTiles[i].Toggle();
                }
            }

            SaveState();
        }

        public void SetState(bool active)
        {
            isOn = active;

            for (int i = 0; i < controlledTiles.Count; i++)
            {
                if (controlledTiles[i] != null)
                {
                    controlledTiles[i].SetState(active);
                }
            }

            SaveState();
        }

        private bool IsValidTrigger(NativeComponent other)
        {
            if (other == null)
            {
                return false;
            }

            var go = other.GetGameObject();
            if (go == null) return false;

            if (go.GetComponent<PlatformerPlayerController>() != null)
            {
                return true;
            }

            return go.GetComponent<PatrolMonster>() != null;
        }

        private bool LoadState(bool fallback)
        {
            if (string.IsNullOrWhiteSpace(saveId))
            {
                return fallback;
            }

            string key = "control_switch_" + saveId;
            bool loadedValue = PlayerPrefs.GetInt(key, fallback ? 1 : 0) == 1;

            for (int i = 0; i < controlledTiles.Count; i++)
            {
                if (controlledTiles[i] != null)
                {
                    controlledTiles[i].SetState(loadedValue);
                }
            }

            return loadedValue;
        }

        private void SaveState()
        {
            if (string.IsNullOrWhiteSpace(saveId))
            {
                return;
            }

            PlayerPrefs.SetInt("control_switch_" + saveId, isOn ? 1 : 0);
            PlayerPrefs.Save();
        }
    }
}
