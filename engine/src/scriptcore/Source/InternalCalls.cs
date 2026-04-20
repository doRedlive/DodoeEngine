namespace GreenCake;

using System;
using System.Runtime.CompilerServices;

internal static class InternalCalls
{
	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_Log(string message);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static bool Native_EntityHasComponent(ulong entityId, Type componentType);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_EntityAddComponent(ulong entityId, Component component);
	
	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_EntityRemoveComponent(ulong entityId, Type componentType);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static bool Native_IsKeyDown(KeyCode keycode);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static bool Native_ComponentExists(ulong entityId, Type componentType);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static float Native_TimeGetDeltaTime();

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_TransfromComponentGetPosition(ulong entityId, out Vector3f position);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_TransfromComponentSetPosition(ulong entityId, ref Vector3f position);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_TransfromComponentGetRotation(ulong entityId, out Vector3f rotation);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_TransfromComponentSetRotation(ulong entityId, ref Vector3f rotation);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_TransfromComponentGetScale(ulong entityId, out Vector3f scale);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_TransfromComponentSetScale(ulong entityId, ref Vector3f scale);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static ulong Native_IDComponentGetID(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static string Native_IDComponentGetName(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_IDComponentSetName(ulong entityId, string name);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static string Native_TagComponentGetTag(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_TagComponentSetTag(ulong entityId, string tag);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static uint Native_Animation2dComponentGetCurrentAnimationID(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_Animation2dComponentSetCurrentAnimationID(ulong entityId, uint animationId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static ulong Native_Animation2dComponentGetCurrentFrameID(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_Animation2dComponentSetCurrentFrameID(ulong entityId, ulong frameId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static float Native_Animation2dComponentGetCurrentTimeDuration(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_Animation2dComponentSetCurrentTimeDuration(ulong entityId, float duration);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static float Native_Animation2dComponentGetSpeed(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_Animation2dComponentSetSpeed(ulong entityId, float speed);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static int Native_Camera2dComponentGetType(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_Camera2dComponentSetType(ulong entityId, int cameraType);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static float Native_Camera2dComponentGetZoom(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_Camera2dComponentSetZoom(ulong entityId, float zoom);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_Camera2dComponentGetBackground(ulong entityId, out Color color);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_Camera2dComponentSetBackground(ulong entityId, ref Color color);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_BoxCollider2dComponentGetOffset(ulong entityId, out Vector2f offset);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_BoxCollider2dComponentSetOffset(ulong entityId, ref Vector2f offset);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_BoxCollider2dComponentGetSize(ulong entityId, out Vector2f size);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_BoxCollider2dComponentSetSize(ulong entityId, ref Vector2f size);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static float Native_BoxCollider2dComponentGetDensity(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_BoxCollider2dComponentSetDensity(ulong entityId, float density);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static float Native_BoxCollider2dComponentGetFriction(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_BoxCollider2dComponentSetFriction(ulong entityId, float friction);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static float Native_BoxCollider2dComponentGetRestitution(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_BoxCollider2dComponentSetRestitution(ulong entityId, float restitution);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static float Native_BoxCollider2dComponentGetRestitutionThreshold(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_BoxCollider2dComponentSetRestitutionThreshold(ulong entityId, float restitutionThreshold);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static int Native_MeshComponentGetValue(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_MeshComponentSetValue(ulong entityId, int value);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static uint Native_ModelRendererComponentGetModelID(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_ModelRendererComponentSetModelID(ulong entityId, uint modelId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_ModelRendererComponentGetColor(ulong entityId, out Color color);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_ModelRendererComponentSetColor(ulong entityId, ref Color color);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static int Native_Rigidbody2dComponentGetType(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_Rigidbody2dComponentSetType(ulong entityId, int bodyType);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static float Native_Rigidbody2dComponentGetGravityScale(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_Rigidbody2dComponentSetGravityScale(ulong entityId, float gravityScale);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static bool Native_Rigidbody2dComponentGetFixedRotation(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_Rigidbody2dComponentSetFixedRotation(ulong entityId, bool fixedRotation);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_Rigidbody2dComponentSetLinearVelocity(ulong entityId, ref Vector2f velocity);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_Rigidbody2dComponentApplyForceToCenter(ulong entityId, ref Vector2f force, bool wake);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_Rigidbody2dComponentApplyLinearImpulseToCenter(ulong entityId, ref Vector2f impulse, bool wake);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static uint Native_SpriteRendererComponentGetTextureID(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_SpriteRendererComponentSetTextureID(ulong entityId, uint textureId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static bool Native_SpriteRendererComponentGetFlip(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_SpriteRendererComponentSetFlip(ulong entityId, bool flip);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_SpriteRendererComponentGetPivot(ulong entityId, out Vector2f pivot);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_SpriteRendererComponentSetPivot(ulong entityId, ref Vector2f pivot);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static float Native_SpriteRendererComponentGetDepth(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_SpriteRendererComponentSetDepth(ulong entityId, float depth);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_SpriteRendererComponentGetColor(ulong entityId, out Color color);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_SpriteRendererComponentSetColor(ulong entityId, ref Color color);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static ulong Native_CreateEntity(string name);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_DestroyEntity(ulong entityId);
}
