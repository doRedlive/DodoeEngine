using GreenCake;
using System;

namespace OnlyOne
{
    [RequireComponent(typeof(Rigidbody2dComponent))]
    [DisallowMultipleComponent]
    public class PlatformerPlayerController : CakeBehaviour
    {
        [GreenCake.Serializable]
        private class SaveData
        {
            public bool dashUnlocked;
            public bool shockwaveUnlocked;
            public bool hasCheckpoint;
            public string checkpointId;
            public Vector2f checkpointPosition;
            public Vector2f checkpointGravity = Vector2f.Down;
        }

        [Header("References")]
        [SerializeField] private TransformComponent visualRoot;
        [SerializeField] private TransformComponent spawnPoint;

        [Header("Collision")]
        [SerializeField] private LayerMask groundMask;
        [SerializeField] private float groundProbeDistance = 0.08f;

        [Header("Movement")]
        [SerializeField] private float moveSpeed = 3.78f;
        [SerializeField] private float gravityAccel = 15f;
        [SerializeField] private float jumpHeight = 3.2f;
        [SerializeField] private float slamAccel = 30f;

        [Header("Dash")]
        [SerializeField] private float dashDistance = 3f;
        [SerializeField] private float dashSpeed = 18f;
        [SerializeField] private int maxDashCount = 1;

        [Header("Black Tile Phase")]
        [SerializeField] private float phaseFallHeight = 2.5f;

        [Header("Monster Bounce")]
        [SerializeField] private float monsterBounceHeight = 5.6f;
        [SerializeField] private float monsterKnockbackSpeed = 2.6f;
        [SerializeField] private float monsterKnockbackUpSpeed = 1.4f;
        [SerializeField] private float monsterBounceControlSpeed = 3.78f;
        [SerializeField] private float monsterBounceLockTime = 0.2f;
        [SerializeField] private float monsterHeadContactSlop = 0.08f;
        [SerializeField] private float monsterSeparationSlop = 0.015f;
        [SerializeField] private float monsterHitCooldown = 0.1f;

        [Header("Input")]
        [SerializeField] private KeyCode jumpKey = KeyCode.Space;
        [SerializeField] private KeyCode dashKey = KeyCode.LeftShift;
        [SerializeField] private KeyCode shockwaveKey = KeyCode.E;
        [SerializeField] private KeyCode respawnKey = KeyCode.R;

        [Header("Save")]
        [SerializeField] private bool startWithDash;
        [SerializeField] private bool startWithShockwave;
        [SerializeField] private string saveKey = "platformer_player_state";

        [Header("Debug")]
        [TextArea(2, 4)]
        [SerializeField] private string lastMessage;

        private readonly NativeComponent[] overlapBuffer = new NativeComponent[16];
        private readonly RaycastHit2D[] groundHits = new RaycastHit2D[16];

        private Rigidbody2dComponent rb;
        private NativeComponent bodyCollider;
        private BoxCollider2dComponent bodyBox;
        private CircleCollider2dComponent bodyCircle;
        private ContactFilter2D groundFilter;

        private Vector2f gravityDir = Vector2f.Down;
        private Vector2f facingDir = Vector2f.Right;
        private Vector2f defaultSpawnPosition;
        private Vector2f checkpointPosition;
        private Vector2f checkpointGravity = Vector2f.Down;
        private string checkpointId;

        private bool hasCheckpoint;
        private bool onGround;
        private bool phaseActive;
        private bool preserveDashVelocityInPhase;
        private bool dashActive;
        private bool dashUnlocked;
        private bool shockwaveUnlocked;
        private bool monsterBounceActive;

        private int dashCount;

        private float dashRemaining;
        private float monsterBounceLock;
        private float monsterHitCooldownTimer;
        private float moveInput;

        private Vector2f dashDirection;
        private Vector2f monsterBounceDir = Vector2f.Up;

        private bool jumpRequested;
        private bool dashRequested;
        private bool shockwaveRequested;
        private bool respawnRequested;
        private bool slamHeld;

        private BlackTilePhaseGroup activePhaseGroup;

        public Vector2f GravityDirection
        {
            get { return gravityDir; }
        }

        private void Awake()
        {
            rb = GetComponent<Rigidbody2dComponent>();
            bodyBox = GetComponent<BoxCollider2dComponent>();
            bodyCircle = GetComponent<CircleCollider2dComponent>();
            bodyCollider = (NativeComponent)bodyBox ?? (NativeComponent)bodyCircle;

            if (rb != null)
            {
                rb.GravityScale = 0f;
                rb.FixedRotation = true;
                rb.SetBodyType(RigidbodyType2D.Dynamic);
            }

            if (spawnPoint != null)
            {
                defaultSpawnPosition = new Vector2f(spawnPoint.Position.x, spawnPoint.Position.y);
            }
            else if (Transform != null)
            {
                defaultSpawnPosition = new Vector2f(Transform.Position.x, Transform.Position.y);
            }

            groundFilter = new ContactFilter2D();
            groundFilter.UseLayerMask = true;
            groundFilter.LayerMask = groundMask;
            groundFilter.UseTriggers = false;
        }

        private void Start()
        {
            dashUnlocked = startWithDash;
            shockwaveUnlocked = startWithShockwave;
            dashCount = maxDashCount;

            LoadState();
            RespawnInternal(false);
        }

        private void Update()
        {
            moveInput = 0f;
            if (Input.GetKey(KeyCode.A) || Input.GetKey(KeyCode.LeftArrow))
            {
                moveInput -= 1f;
            }
            if (Input.GetKey(KeyCode.D) || Input.GetKey(KeyCode.RightArrow))
            {
                moveInput += 1f;
            }

            if (Input.GetKeyDown(jumpKey))
            {
                jumpRequested = true;
            }

            if (dashUnlocked && (Input.GetKeyDown(dashKey) || Input.GetKeyDown(KeyCode.RightShift)))
            {
                dashRequested = true;
            }

            if (shockwaveUnlocked && Input.GetKeyDown(shockwaveKey))
            {
                shockwaveRequested = true;
            }

            slamHeld = Input.GetKey(KeyCode.S) || Input.GetKey(KeyCode.DownArrow);

            if (Input.GetKeyDown(respawnKey))
            {
                respawnRequested = true;
            }
        }

        private void FixedUpdate()
        {
            float dt = Time.FixedDeltaTime;
            monsterHitCooldownTimer = MathF.Max(0f, monsterHitCooldownTimer - dt);
            monsterBounceLock = MathF.Max(0f, monsterBounceLock - dt);

            if (respawnRequested)
            {
                respawnRequested = false;
                Respawn();
                ClearTransientInput();
                return;
            }

            if (shockwaveRequested)
            {
                shockwaveRequested = false;
                BrittleTile.BreakAllActive();
                lastMessage = "Shockwave broke brittle tiles.";
            }

            RefreshGroundedState();
            UpdatePhaseExit();

            Vector2f tangent = new Vector2f(-gravityDir.y, gravityDir.x);
            if (MathF.Abs(moveInput) > 0.001f)
            {
                facingDir = tangent * (moveInput > 0f ? 1f : -1f);
            }

            Vector2f velocity = rb != null ? rb.GetVelocity() : Vector2f.Zero;

            if (monsterBounceActive && Vector2f.Dot(velocity, monsterBounceDir) <= 0f)
            {
                monsterBounceActive = false;
                monsterBounceLock = 0f;
            }

            bool bounceLocked = monsterBounceActive || monsterBounceLock > 0f;

            if (dashRequested && !bounceLocked)
            {
                StartDash();
            }
            dashRequested = false;

            float normalVelocity = Vector2f.Dot(velocity, gravityDir);
            float desiredTangentVelocity = moveInput * moveSpeed;

            if (dashActive)
            {
                velocity = dashDirection * dashSpeed;
                dashRemaining -= dashSpeed * dt;
                if (dashRemaining <= 0f)
                {
                    dashActive = false;
                    velocity = Vector2f.Zero;
                }
            }
            else if (bounceLocked)
            {
                float bounceNormalVelocity = Vector2f.Dot(velocity, monsterBounceDir);
                if (monsterBounceLock > 0f)
                {
                    velocity = monsterBounceDir * bounceNormalVelocity;
                }
                else
                {
                    velocity = tangent * (moveInput * monsterBounceControlSpeed) + monsterBounceDir * bounceNormalVelocity;
                }
            }
            else if (phaseActive && preserveDashVelocityInPhase)
            {
                if (MathF.Abs(moveInput) > 0.001f)
                {
                    velocity = tangent * desiredTangentVelocity + gravityDir * normalVelocity;
                }
            }
            else
            {
                velocity = tangent * desiredTangentVelocity + gravityDir * normalVelocity;
            }

            if (jumpRequested && onGround && !phaseActive && !bounceLocked)
            {
                velocity -= gravityDir * JumpSpeed;
                onGround = false;
            }
            jumpRequested = false;

            if (!phaseActive && !dashActive)
            {
                velocity += gravityDir * gravityAccel * dt;
            }

            if (slamHeld && !phaseActive && !dashActive && !bounceLocked)
            {
                velocity += gravityDir * slamAccel * dt;
            }

            if (rb != null)
            {
                rb.SetVelocity(velocity);
            }
            ApplyVisualRotation();
        }

        private void OnCollisionEnter2D(Collision2D collision)
        {
            HandleCollision(collision);
        }

        private void OnCollisionStay2D(Collision2D collision)
        {
            HandleCollision(collision);
        }

        public void UnlockDash()
        {
            dashUnlocked = true;
            lastMessage = "Dash unlocked.";
            SaveState();
        }

        public void UnlockShockwave()
        {
            shockwaveUnlocked = true;
            lastMessage = "Shockwave unlocked.";
            SaveState();
        }

        public void ActivateCheckpoint(CheckpointTrigger checkpoint)
        {
            if (checkpoint == null)
            {
                return;
            }

            hasCheckpoint = true;
            checkpointId = checkpoint.CheckpointId;
            checkpointPosition = checkpoint.RespawnPosition;
            checkpointGravity = gravityDir;
            lastMessage = "Checkpoint activated.";
            SaveState();
        }

        public void Respawn()
        {
            RespawnInternal(true);
        }

        private void RespawnInternal(bool announce)
        {
            if (phaseActive && activePhaseGroup != null && bodyCollider != null)
            {
                activePhaseGroup.SetIgnored(bodyCollider, false);
            }

            phaseActive = false;
            activePhaseGroup = null;
            preserveDashVelocityInPhase = false;
            dashActive = false;
            dashRemaining = 0f;
            dashCount = maxDashCount;
            monsterBounceActive = false;
            monsterBounceLock = 0f;
            monsterBounceDir = Vector2f.Up;
            onGround = false;

            Vector2f respawnPosition = defaultSpawnPosition;
            Vector2f respawnGravity = Vector2f.Down;
            if (hasCheckpoint)
            {
                respawnPosition = checkpointPosition;
                respawnGravity = checkpointGravity;
            }

            gravityDir = Cardinalize(respawnGravity);
            facingDir = new Vector2f(-gravityDir.y, gravityDir.x);

            if (rb != null)
            {
                rb.MovePosition(respawnPosition);
                rb.SetVelocity(Vector2f.Zero);
            }
            else if (Transform != null)
            {
                Transform.Position = new Vector3f(respawnPosition.x, respawnPosition.y, Transform.Position.z);
            }

            PatrolMonster.ResetAll();
            BrittleTile.ResetAll();
            ApplyVisualRotation();

            if (announce)
            {
                lastMessage = hasCheckpoint ? "Respawned at checkpoint." : "Respawned at default spawn.";
            }
        }

        private void HandleCollision(Collision2D collision)
        {
            if (collision.Collider == null)
            {
                return;
            }

            GameObject otherGo = collision.Collider.GetGameObject();
            if (otherGo != null)
            {
                PatrolMonster monster = otherGo.GetComponent<PatrolMonster>();
                if (monster != null)
                {
                    HandleMonsterCollision(monster);
                    return;
                }

                BrittleTile brittleTile = otherGo.GetComponent<BrittleTile>();
                if (brittleTile != null)
                {
                    for (int i = 0; i < collision.ContactCount; i++)
                    {
                        Vector2f normal = collision.Contacts != null ? collision.Contacts[i].Normal : Vector2f.Zero;
                        if (Vector2f.Dot(normal, -gravityDir) > 0.65f)
                        {
                            brittleTile.TriggerBreak();
                            break;
                        }
                    }
                }
            }

            if (phaseActive)
            {
                return;
            }

            BlackTilePhaseGroup blackGroup = otherGo != null ? otherGo.GetComponentInParent<BlackTilePhaseGroup>() : null;
            if (blackGroup == null)
            {
                return;
            }

            Vector2f velocity = rb != null ? rb.GetVelocity() : Vector2f.Zero;
            for (int i = 0; i < collision.ContactCount; i++)
            {
                Vector2f normal = collision.Contacts != null ? collision.Contacts[i].Normal : Vector2f.Zero;
                float normalSpeed = -Vector2f.Dot(velocity, normal);
                if (normalSpeed > PhaseTriggerSpeed && IsTouchingOnlyBlackTiles())
                {
                    EnterPhase(blackGroup, normalSpeed);
                    break;
                }
            }
        }

        private void HandleMonsterCollision(PatrolMonster monster)
        {
            if (monsterHitCooldownTimer > 0f)
            {
                return;
            }

            Bounds playerBounds = GetBodyBounds();
            Bounds monsterBounds = GetMonsterBounds(monster);

            Vector2f contactNormal;
            float contactDepth;
            if (!TryComputeContact(playerBounds, monsterBounds, out contactNormal, out contactDepth))
            {
                return;
            }

            Vector2f monsterGravity = monster.GravityDirection;
            Vector2f headDir = -monsterGravity;
            bool contactFromHeadSide = Vector2f.Dot(contactNormal, headDir) > 0.65f;
            bool contactFromPlayerFootSide = Vector2f.Dot(contactNormal, -gravityDir) > 0.65f;

            float playerMinHead, playerMaxHead;
            float monsterMinHead, monsterMaxHead;
            ProjectBounds(playerBounds, headDir, out playerMinHead, out playerMaxHead);
            ProjectBounds(monsterBounds, headDir, out monsterMinHead, out monsterMaxHead);

            bool wasOutsideHeadSide = playerMinHead >= monsterMaxHead - monsterHeadContactSlop;
            bool sweptAcrossHeadSide = playerMaxHead >= monsterMinHead - monsterHeadContactSlop;
            Vector2f currentV = rb != null ? rb.GetVelocity() : Vector2f.Zero;
            bool movingTowardMonster = Vector2f.Dot(currentV - monster.CurrentVelocity, headDir) < 1.55f;
            float gravityDot = Vector2f.Dot(gravityDir, monsterGravity);
            float gravityAlignment = MathF.Abs(gravityDot);

            if (contactFromHeadSide && contactFromPlayerFootSide && wasOutsideHeadSide && sweptAcrossHeadSide && movingTowardMonster)
            {
                ApplyMonsterBounce(contactNormal, contactDepth, headDir, gravityAlignment < 0.65f);
                monsterHitCooldownTimer = monsterHitCooldown;
                return;
            }

            if (contactFromHeadSide && !contactFromPlayerFootSide && gravityDot < -0.65f && wasOutsideHeadSide && sweptAcrossHeadSide && movingTowardMonster)
            {
                ApplyMonsterBounce(contactNormal, contactDepth, headDir, false);
                monsterHitCooldownTimer = monsterHitCooldown;
                return;
            }

            if (contactFromHeadSide && !contactFromPlayerFootSide && gravityAlignment < 0.65f && wasOutsideHeadSide && sweptAcrossHeadSide && movingTowardMonster)
            {
                ApplyMonsterBounce(contactNormal, contactDepth, headDir, true);
                monsterHitCooldownTimer = monsterHitCooldown;
                return;
            }

            if (rb != null)
            {
                Vector2f pos = new Vector2f(Transform.Position.x, Transform.Position.y);
                rb.MovePosition(pos + contactNormal * (contactDepth + monsterSeparationSlop));
                rb.SetVelocity(contactNormal * monsterKnockbackSpeed - gravityDir * monsterKnockbackUpSpeed);
            }
            dashActive = false;
            dashRemaining = 0f;
            preserveDashVelocityInPhase = false;
            monsterBounceLock = 0f;
            monsterBounceActive = false;
            onGround = false;
            monsterHitCooldownTimer = monsterHitCooldown;
            lastMessage = "Hit monster from the side.";
        }

        private void ApplyMonsterBounce(Vector2f contactNormal, float contactDepth, Vector2f bounceDir, bool lockControl)
        {
            if (rb != null)
            {
                Vector2f pos = new Vector2f(Transform.Position.x, Transform.Position.y);
                rb.MovePosition(pos + contactNormal * (contactDepth + monsterSeparationSlop));
                float bounceSpeed = MathF.Sqrt(2f * gravityAccel * monsterBounceHeight);
                rb.SetVelocity(bounceDir * bounceSpeed);
            }
            dashActive = false;
            dashRemaining = 0f;
            preserveDashVelocityInPhase = false;
            monsterBounceLock = lockControl ? monsterBounceLockTime : 0f;
            monsterBounceActive = true;
            monsterBounceDir = bounceDir;
            onGround = false;
            lastMessage = lockControl ? "Monster bounce with control lock." : "Monster bounce.";
        }

        private void RefreshGroundedState()
        {
            onGround = false;
            if (bodyCollider == null) return;

            Bounds bounds = GetBodyBounds();
            Vector2f center = bounds.Center;
            Vector2f size = bounds.Size * 0.9f;
            int hitCount = Physics2D.BoxCastNonAlloc(center, size, 0f, gravityDir, groundHits, groundProbeDistance + 0.0001f, groundMask.value);
            for (int i = 0; i < hitCount; i++)
            {
                if (groundHits[i].Collider == null || groundHits[i].Collider == bodyCollider)
                {
                    continue;
                }

                GameObject hitGo = groundHits[i].Collider.GetGameObject();
                BlackTilePhaseGroup hitGroup = hitGo != null ? hitGo.GetComponentInParent<BlackTilePhaseGroup>() : null;
                if (phaseActive && activePhaseGroup != null && hitGroup == activePhaseGroup)
                {
                    continue;
                }

                if (Vector2f.Dot(groundHits[i].Normal, -gravityDir) > 0.65f)
                {
                    onGround = true;
                    dashCount = maxDashCount;
                    return;
                }
            }
        }

        private void StartDash()
        {
            if (dashActive || phaseActive || dashCount <= 0)
            {
                return;
            }

            dashDirection = facingDir.LengthSquared > 0.001f ? facingDir.Normalized : Vector2f.Right;
            dashActive = true;
            dashRemaining = dashDistance;
            dashCount--;
            onGround = false;
            lastMessage = "Dash started.";
        }

        private void EnterPhase(BlackTilePhaseGroup blackGroup, float normalSpeed)
        {
            if (blackGroup == null || phaseActive || bodyCollider == null)
            {
                return;
            }

            activePhaseGroup = blackGroup;
            phaseActive = true;

            if (dashActive)
            {
                dashActive = false;
                dashRemaining = 0f;
                preserveDashVelocityInPhase = true;
            }
            else
            {
                preserveDashVelocityInPhase = false;
            }

            activePhaseGroup.SetIgnored(bodyCollider, true);
            lastMessage = "Entered black-tile phase at speed " + normalSpeed.ToString("F2") + ".";
        }

        private void UpdatePhaseExit()
        {
            if (!phaseActive || activePhaseGroup == null || bodyCollider == null)
            {
                return;
            }

            if (activePhaseGroup.IsOverlapping(bodyCollider))
            {
                return;
            }

            Bounds playerBounds = GetBodyBounds();
            Vector2f velocity = rb != null ? rb.GetVelocity() : Vector2f.Zero;
            Vector2f exitGravity = activePhaseGroup.GetExitGravity(playerBounds, velocity, gravityDir);
            float normalVelocity = Vector2f.Dot(velocity, exitGravity);
            if (rb != null)
            {
                rb.SetVelocity(velocity - exitGravity * (normalVelocity * 0.5f));
            }

            activePhaseGroup.SetIgnored(bodyCollider, false);
            activePhaseGroup = null;
            phaseActive = false;
            preserveDashVelocityInPhase = false;
            onGround = false;
            gravityDir = Cardinalize(exitGravity);
            facingDir = new Vector2f(-gravityDir.y, gravityDir.x);
            ApplyVisualRotation();
            lastMessage = "Exited black-tile phase.";
        }

        private bool IsTouchingOnlyBlackTiles()
        {
            if (bodyCollider == null) return false;
            int count = Physics2D.GetColliderContacts(bodyCollider, overlapBuffer);
            bool touchedBlack = false;

            for (int i = 0; i < count; i++)
            {
                NativeComponent other = overlapBuffer[i];
                if (other == null)
                {
                    continue;
                }

                bool isTrigger = false;
                if (other is BoxCollider2dComponent b) isTrigger = b.GetIsSensor();
                else if (other is CircleCollider2dComponent c) isTrigger = c.GetIsSensor();
                if (isTrigger)
                {
                    continue;
                }

                GameObject go = other.GetGameObject();
                BlackTilePhaseGroup group = go != null ? go.GetComponentInParent<BlackTilePhaseGroup>() : null;
                if (group == null)
                {
                    return false;
                }

                touchedBlack = true;
            }

            return touchedBlack;
        }

        private void ApplyVisualRotation()
        {
            if (visualRoot == null)
            {
                return;
            }

            float angle = MathF.Atan2(-gravityDir.x, -gravityDir.y) * (180f / MathF.PI);
            visualRoot.Rotation = new Vector3f(0f, 0f, angle);
        }

        private void SaveState()
        {
            SaveData data = new SaveData();
            data.dashUnlocked = dashUnlocked;
            data.shockwaveUnlocked = shockwaveUnlocked;
            data.hasCheckpoint = hasCheckpoint;
            data.checkpointId = checkpointId;
            data.checkpointPosition = checkpointPosition;
            data.checkpointGravity = checkpointGravity;

            PlayerPrefs.SetString(saveKey, SerializeSaveData(data));
            PlayerPrefs.Save();
        }

        private void LoadState()
        {
            if (!PlayerPrefs.HasKey(saveKey))
            {
                return;
            }

            SaveData data = DeserializeSaveData(PlayerPrefs.GetString(saveKey));
            if (data == null)
            {
                return;
            }

            dashUnlocked = startWithDash || data.dashUnlocked;
            shockwaveUnlocked = startWithShockwave || data.shockwaveUnlocked;
            hasCheckpoint = data.hasCheckpoint;
            checkpointId = data.checkpointId;
            checkpointPosition = data.checkpointPosition;
            checkpointGravity = Cardinalize(data.checkpointGravity);
        }

        private void ClearTransientInput()
        {
            jumpRequested = false;
            dashRequested = false;
            shockwaveRequested = false;
            slamHeld = false;
        }

        private Bounds GetBodyBounds()
        {
            if (bodyBox != null)
            {
                Vector2f center = new Vector2f(Transform.Position.x, Transform.Position.y) + new Vector2f(bodyBox.Offset.x, bodyBox.Offset.y);
                Vector2f size = new Vector2f(bodyBox.Size.x, bodyBox.Size.y);
                return new Bounds(center, size);
            }
            if (bodyCircle != null)
            {
                Vector2f center = new Vector2f(Transform.Position.x, Transform.Position.y) + new Vector2f(bodyCircle.Offset.x, bodyCircle.Offset.y);
                float d = bodyCircle.Radius * 2f;
                return new Bounds(center, new Vector2f(d, d));
            }
            return new Bounds(new Vector2f(Transform.Position.x, Transform.Position.y), new Vector2f(1f, 1f));
        }

        private static Bounds GetMonsterBounds(PatrolMonster m)
        {
            if (m == null || m.Transform == null) return new Bounds(Vector2f.Zero, Vector2f.One);
            BoxCollider2dComponent mb = m.GetComponent<BoxCollider2dComponent>();
            CircleCollider2dComponent mc = m.GetComponent<CircleCollider2dComponent>();
            if (mb != null)
            {
                Vector2f center = new Vector2f(m.Transform.Position.x, m.Transform.Position.y) + new Vector2f(mb.Offset.x, mb.Offset.y);
                Vector2f size = new Vector2f(mb.Size.x, mb.Size.y);
                return new Bounds(center, size);
            }
            if (mc != null)
            {
                Vector2f center = new Vector2f(m.Transform.Position.x, m.Transform.Position.y) + new Vector2f(mc.Offset.x, mc.Offset.y);
                float d = mc.Radius * 2f;
                return new Bounds(center, new Vector2f(d, d));
            }
            return new Bounds(new Vector2f(m.Transform.Position.x, m.Transform.Position.y), new Vector2f(1f, 1f));
        }

        private static bool TryComputeContact(Bounds a, Bounds b, out Vector2f normal, out float depth)
        {
            normal = Vector2f.Zero;
            depth = 0f;

            float overlapX = MathF.Min(a.Max.x, b.Max.x) - MathF.Max(a.Min.x, b.Min.x);
            float overlapY = MathF.Min(a.Max.y, b.Max.y) - MathF.Max(a.Min.y, b.Min.y);
            if (overlapX <= 0f || overlapY <= 0f)
            {
                return false;
            }

            if (overlapX < overlapY)
            {
                normal = a.Center.x < b.Center.x ? Vector2f.Left : Vector2f.Right;
                depth = overlapX;
            }
            else
            {
                normal = a.Center.y < b.Center.y ? Vector2f.Down : Vector2f.Up;
                depth = overlapY;
            }

            return true;
        }

        private static void ProjectBounds(Bounds bounds, Vector2f axis, out float min, out float max)
        {
            Vector2f p1 = new Vector2f(bounds.Min.x, bounds.Min.y);
            Vector2f p2 = new Vector2f(bounds.Min.x, bounds.Max.y);
            Vector2f p3 = new Vector2f(bounds.Max.x, bounds.Min.y);
            Vector2f p4 = new Vector2f(bounds.Max.x, bounds.Max.y);

            float d1 = Vector2f.Dot(p1, axis);
            float d2 = Vector2f.Dot(p2, axis);
            float d3 = Vector2f.Dot(p3, axis);
            float d4 = Vector2f.Dot(p4, axis);

            min = MathF.Min(MathF.Min(d1, d2), MathF.Min(d3, d4));
            max = MathF.Max(MathF.Max(d1, d2), MathF.Max(d3, d4));
        }

        private static Vector2f Cardinalize(Vector2f value)
        {
            if (value.LengthSquared < 0.001f)
            {
                return Vector2f.Down;
            }

            if (MathF.Abs(value.x) > MathF.Abs(value.y))
            {
                return value.x >= 0f ? Vector2f.Right : Vector2f.Left;
            }

            return value.y >= 0f ? Vector2f.Up : Vector2f.Down;
        }

        private float JumpSpeed
        {
            get { return MathF.Sqrt(2f * gravityAccel * jumpHeight); }
        }

        private float PhaseTriggerSpeed
        {
            get { return MathF.Sqrt(2f * gravityAccel * phaseFallHeight); }
        }

        private static string SerializeSaveData(SaveData d)
        {
            string cpPos = d.checkpointPosition.x.ToString("R") + "," + d.checkpointPosition.y.ToString("R");
            string cpGrav = d.checkpointGravity.x.ToString("R") + "," + d.checkpointGravity.y.ToString("R");
            return "{\"dashUnlocked\":" + (d.dashUnlocked ? "true" : "false")
                + ",\"shockwaveUnlocked\":" + (d.shockwaveUnlocked ? "true" : "false")
                + ",\"hasCheckpoint\":" + (d.hasCheckpoint ? "true" : "false")
                + ",\"checkpointId\":\"" + EscapeJson(d.checkpointId ?? "") + "\""
                + ",\"checkpointPosition\":\"" + cpPos + "\""
                + ",\"checkpointGravity\":\"" + cpGrav + "\"}";
        }

        private static SaveData DeserializeSaveData(string json)
        {
            if (string.IsNullOrEmpty(json)) return null;
            SaveData d = new SaveData();
            d.dashUnlocked = FindBool(json, "dashUnlocked");
            d.shockwaveUnlocked = FindBool(json, "shockwaveUnlocked");
            d.hasCheckpoint = FindBool(json, "hasCheckpoint");
            d.checkpointId = FindString(json, "checkpointId");
            Vector2f cpPos = FindVector2f(json, "checkpointPosition");
            d.checkpointPosition = cpPos;
            Vector2f cpGrav = FindVector2f(json, "checkpointGravity");
            if (cpGrav.LengthSquared < 0.0001f) cpGrav = Vector2f.Down;
            d.checkpointGravity = cpGrav;
            return d;
        }

        private static string EscapeJson(string s)
        {
            return s.Replace("\\", "\\\\").Replace("\"", "\\\"");
        }

        private static bool FindBool(string json, string key)
        {
            string pat = "\"" + key + "\":";
            int i = json.IndexOf(pat);
            if (i < 0) return false;
            int start = i + pat.Length;
            return json.IndexOf("true", start, StringComparison.Ordinal) == start;
        }

        private static string FindString(string json, string key)
        {
            string pat = "\"" + key + "\":\"";
            int i = json.IndexOf(pat);
            if (i < 0) return "";
            int start = i + pat.Length;
            int end = json.IndexOf('"', start);
            if (end < 0) return "";
            string raw = json.Substring(start, end - start);
            return raw.Replace("\\\"", "\"").Replace("\\\\", "\\");
        }

        private static Vector2f FindVector2f(string json, string key)
        {
            string s = FindString(json, key);
            if (string.IsNullOrEmpty(s)) return Vector2f.Zero;
            int comma = s.IndexOf(',');
            if (comma < 0) return Vector2f.Zero;
            float x, y;
            if (float.TryParse(s.Substring(0, comma), System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out x)
                && float.TryParse(s.Substring(comma + 1), System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out y))
            {
                return new Vector2f(x, y);
            }
            return Vector2f.Zero;
        }
    }
}
