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
	internal extern static int Native_MeshRendererComponentGetValue(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_MeshRendererComponentSetValue(ulong entityId, int value);

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
	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_TilemapSetData(ulong entityId, int mapWidth, int mapHeight, int tileWidth, int tileHeight);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_TilemapAddTileset(ulong entityId, string tilesetJson);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_TileLayerSetData(ulong entityId, uint[] tiles, int width, int height, string name, bool visible, float opacity, int offsetX, int offsetY);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_EntitySetParent(ulong childEntityId, ulong parentEntityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static uint Native_TilemapComponentGetMapWidth(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_TilemapComponentSetMapWidth(ulong entityId, uint value);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static uint Native_TilemapComponentGetMapHeight(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_TilemapComponentSetMapHeight(ulong entityId, uint value);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static uint Native_TilemapComponentGetTileWidth(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_TilemapComponentSetTileWidth(ulong entityId, uint value);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static uint Native_TilemapComponentGetTileHeight(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_TilemapComponentSetTileHeight(ulong entityId, uint value);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static string Native_TileLayerComponentGetLayerName(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_TileLayerComponentSetLayerName(ulong entityId, string value);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static uint Native_TileLayerComponentGetLayerWidth(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_TileLayerComponentSetLayerWidth(ulong entityId, uint value);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static uint Native_TileLayerComponentGetLayerHeight(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_TileLayerComponentSetLayerHeight(ulong entityId, uint value);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static bool Native_TileLayerComponentGetVisible(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_TileLayerComponentSetVisible(ulong entityId, bool value);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static float Native_TileLayerComponentGetOpacity(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_TileLayerComponentSetOpacity(ulong entityId, float value);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static int Native_TileLayerComponentGetOffsetX(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_TileLayerComponentSetOffsetX(ulong entityId, int value);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static int Native_TileLayerComponentGetOffsetY(ulong entityId);

	[MethodImplAttribute(MethodImplOptions.InternalCall)]
	internal extern static void Native_TileLayerComponentSetOffsetY(ulong entityId, int value);
}
