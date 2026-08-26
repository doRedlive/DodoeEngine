using GreenCake;
using System;
using System.Collections.Generic;

namespace OnlyOne
{
    [DisallowMultipleComponent]
    public class ControlTile : CakeBehaviour
    {
        [SerializeField] private BoxCollider2dComponent[] boxCollidersToToggle;
        [SerializeField] private CircleCollider2dComponent[] circleCollidersToToggle;
        [SerializeField] private SpriteRendererComponent[] visualsToToggle;
        [SerializeField] private bool startActive = true;

        public bool IsActive { get; private set; }

        private void Awake()
        {
            bool boxesEmpty = boxCollidersToToggle == null || boxCollidersToToggle.Length == 0;
            bool circlesEmpty = circleCollidersToToggle == null || circleCollidersToToggle.Length == 0;
            if (boxesEmpty && circlesEmpty)
            {
                var boxes = new List<BoxCollider2dComponent>(GetComponentsInChildren<BoxCollider2dComponent>(true));
                var circles = new List<CircleCollider2dComponent>(GetComponentsInChildren<CircleCollider2dComponent>(true));
                if (boxes.Count > 0) boxCollidersToToggle = boxes.ToArray();
                if (circles.Count > 0) circleCollidersToToggle = circles.ToArray();
            }

            if (visualsToToggle == null || visualsToToggle.Length == 0)
            {
                visualsToToggle = GetComponentsInChildren<SpriteRendererComponent>(true);
            }
        }

        private void Start()
        {
            SetState(startActive);
        }

        public void SetState(bool active)
        {
            IsActive = active;

            if (boxCollidersToToggle != null)
            {
                for (int i = 0; i < boxCollidersToToggle.Length; i++)
                {
                    if (boxCollidersToToggle[i] != null)
                    {
                        boxCollidersToToggle[i].SetEnabled(active);
                    }
                }
            }

            if (circleCollidersToToggle != null)
            {
                for (int i = 0; i < circleCollidersToToggle.Length; i++)
                {
                    if (circleCollidersToToggle[i] != null)
                    {
                        circleCollidersToToggle[i].SetEnabled(active);
                    }
                }
            }

            if (visualsToToggle != null)
            {
                for (int i = 0; i < visualsToToggle.Length; i++)
                {
                    if (visualsToToggle[i] != null)
                    {
                        visualsToToggle[i].SetVisible(active);
                    }
                }
            }
        }

        public void Toggle()
        {
            SetState(!IsActive);
        }
    }
}
