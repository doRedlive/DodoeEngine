using GreenCake;
using System;
using System.Collections;
using System.Collections.Generic;

namespace OnlyOne
{
    [DisallowMultipleComponent]
    public class BrittleTile : CakeBehaviour
    {
        private static readonly List<BrittleTile> AllTiles = new List<BrittleTile>();

        [SerializeField] private BoxCollider2dComponent solidBoxCollider;
        [SerializeField] private CircleCollider2dComponent solidCircleCollider;
        [SerializeField] private SpriteRendererComponent[] visuals;
        [SerializeField] private float breakDelay = 0.4f;
        [SerializeField] private float restoreDelay = 5f;

        private Coroutine breakRoutine;
        private bool triggered;

        public bool IsActive
        {
            get
            {
                if (solidBoxCollider != null) return solidBoxCollider.GetEnabled();
                if (solidCircleCollider != null) return solidCircleCollider.GetEnabled();
                return false;
            }
        }

        private void Awake()
        {
            if (solidBoxCollider == null && solidCircleCollider == null)
            {
                solidBoxCollider = GetComponent<BoxCollider2dComponent>();
                if (solidBoxCollider == null)
                {
                    solidCircleCollider = GetComponent<CircleCollider2dComponent>();
                }
            }

            if (visuals == null || visuals.Length == 0)
            {
                visuals = GetComponentsInChildren<SpriteRendererComponent>(true);
            }
        }

        private void OnEnable()
        {
            if (!AllTiles.Contains(this))
            {
                AllTiles.Add(this);
            }
        }

        private void OnDisable()
        {
            AllTiles.Remove(this);
        }

        public void TriggerBreak()
        {
            if (triggered || !IsActive)
            {
                return;
            }

            triggered = true;
            RestartRoutine(BreakSequence());
        }

        public void BreakNow()
        {
            if (!IsActive)
            {
                return;
            }

            RestartRoutine(BreakAndRestore());
        }

        public void ResetTile()
        {
            triggered = false;
            if (breakRoutine != null)
            {
                StopCoroutine(breakRoutine);
                breakRoutine = null;
            }

            SetTileVisible(true);
        }

        public static void BreakAllActive()
        {
            for (int i = 0; i < AllTiles.Count; i++)
            {
                if (AllTiles[i] != null && AllTiles[i].IsActive)
                {
                    AllTiles[i].BreakNow();
                }
            }
        }

        public static void ResetAll()
        {
            for (int i = 0; i < AllTiles.Count; i++)
            {
                if (AllTiles[i] != null)
                {
                    AllTiles[i].ResetTile();
                }
            }
        }

        private IEnumerator BreakSequence()
        {
            yield return new WaitForSeconds(breakDelay);
            var seq = BreakAndRestore();
            while (seq.MoveNext()) yield return seq.Current;
        }

        private IEnumerator BreakAndRestore()
        {
            triggered = false;
            SetTileVisible(false);
            yield return new WaitForSeconds(restoreDelay);
            SetTileVisible(true);
        }

        private void SetTileVisible(bool active)
        {
            if (solidBoxCollider != null)
            {
                solidBoxCollider.SetEnabled(active);
            }
            if (solidCircleCollider != null)
            {
                solidCircleCollider.SetEnabled(active);
            }

            if (visuals != null)
            {
                for (int i = 0; i < visuals.Length; i++)
                {
                    if (visuals[i] != null)
                    {
                        visuals[i].SetVisible(active);
                    }
                }
            }
        }

        private void RestartRoutine(IEnumerator routine)
        {
            if (breakRoutine != null)
            {
                StopCoroutine(breakRoutine);
            }

            breakRoutine = StartCoroutine(routine);
        }
    }
}
