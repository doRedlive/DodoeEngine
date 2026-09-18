namespace GreenCake.Editor;

using System;
using System.Runtime.InteropServices;
using System.Text;

internal static unsafe class EditorNativeCalls
{
    [StructLayout(LayoutKind.Sequential)]
    public struct EditorBindings
    {
        public delegate* unmanaged<int> is_ready;
        public delegate* unmanaged<ulong*, int, int> get_selection;
        public delegate* unmanaged<ulong*, int, int> set_selection;
        public delegate* unmanaged<int> get_entity_count;
        public delegate* unmanaged<int, ulong*, ulong*, byte*, int, int> get_entity;
        public delegate* unmanaged<byte*, int, int> get_scene_name;
        public delegate* unmanaged<byte*, int, int> get_project_root;
        public delegate* unmanaged<int> get_play_state;
        public delegate* unmanaged<byte*, byte*, int> execute_command;
        public delegate* unmanaged<int> undo;
        public delegate* unmanaged<int> redo;
        public delegate* unmanaged<byte*, ulong*, int> create_entity;
        public delegate* unmanaged<ulong, int> delete_entity;
        public delegate* unmanaged<ulong, byte*, int> rename_entity;
        public delegate* unmanaged<ulong, ulong, int> reparent_entity;
    }

    private static EditorBindings s_bindings;
    private static bool s_bound;

    internal static bool IsBound => s_bound;

    internal static void Bind(EditorBindings* bindings)
    {
        if (bindings == null)
        {
            s_bound = false;
            return;
        }
        s_bindings = *bindings;
        s_bound = true;
    }

    internal static bool IsReady()
        => s_bound && s_bindings.is_ready != null && s_bindings.is_ready() != 0;

    internal static ulong[] GetSelectionArray()
    {
        if (!s_bound || s_bindings.get_selection == null)
            return Array.Empty<ulong>();
        int count = s_bindings.get_selection(null, 0);
        if (count <= 0)
            return Array.Empty<ulong>();
        ulong[] result = new ulong[count];
        fixed (ulong* p = result)
            s_bindings.get_selection(p, count);
        return result;
    }

    internal static bool SetSelection(ulong[] uuids)
    {
        if (!s_bound || s_bindings.set_selection == null)
            return false;
        if (uuids == null || uuids.Length == 0)
            return s_bindings.set_selection(null, 0) != 0;
        fixed (ulong* p = uuids)
            return s_bindings.set_selection(p, uuids.Length) != 0;
    }

    internal static int GetEntityCount()
    {
        if (!s_bound || s_bindings.get_entity_count == null)
            return 0;
        return s_bindings.get_entity_count();
    }

    internal static bool GetEntity(int index, out ulong uuid, out ulong parent, out string name)
    {
        uuid = 0;
        parent = 0;
        name = string.Empty;
        if (!s_bound || s_bindings.get_entity == null)
            return false;

        int length = s_bindings.get_entity(index, null, null, null, 0);
        if (length < 0)
            return false;

        ulong localUuid = 0;
        ulong localParent = 0;
        byte[] buffer = new byte[length + 1];
        fixed (byte* p = buffer)
            s_bindings.get_entity(index, &localUuid, &localParent, p, buffer.Length);

        uuid = localUuid;
        parent = localParent;
        name = Encoding.UTF8.GetString(buffer, 0, length);
        return true;
    }

    internal static string GetSceneName()
    {
        if (!s_bound || s_bindings.get_scene_name == null)
            return string.Empty;
        return ReadString(s_bindings.get_scene_name);
    }

    internal static string GetProjectRoot()
    {
        if (!s_bound || s_bindings.get_project_root == null)
            return string.Empty;
        return ReadString(s_bindings.get_project_root);
    }

    internal static int GetPlayState()
    {
        if (!s_bound || s_bindings.get_play_state == null)
            return 0;
        return s_bindings.get_play_state();
    }

    internal static bool ExecuteCommand(string name, string payload)
    {
        if (!s_bound || s_bindings.execute_command == null)
            return false;
        fixed (byte* pName = Utf8Z(name))
        fixed (byte* pPayload = Utf8Z(payload))
            return s_bindings.execute_command(pName, pPayload) != 0;
    }

    internal static bool Undo()
    {
        if (!s_bound || s_bindings.undo == null)
            return false;
        return s_bindings.undo() != 0;
    }

    internal static bool Redo()
    {
        if (!s_bound || s_bindings.redo == null)
            return false;
        return s_bindings.redo() != 0;
    }

    internal static bool CreateEntity(string name, out ulong uuid)
    {
        uuid = 0;
        if (!s_bound || s_bindings.create_entity == null)
            return false;
        ulong localUuid = 0;
        fixed (byte* pName = Utf8Z(name))
        {
            if (s_bindings.create_entity(pName, &localUuid) == 0)
                return false;
        }
        uuid = localUuid;
        return true;
    }

    internal static bool DeleteEntity(ulong uuid)
    {
        if (!s_bound || s_bindings.delete_entity == null)
            return false;
        return s_bindings.delete_entity(uuid) != 0;
    }

    internal static bool RenameEntity(ulong uuid, string name)
    {
        if (!s_bound || s_bindings.rename_entity == null)
            return false;
        fixed (byte* pName = Utf8Z(name))
            return s_bindings.rename_entity(uuid, pName) != 0;
    }

    internal static bool ReparentEntity(ulong uuid, ulong parentUuid)
    {
        if (!s_bound || s_bindings.reparent_entity == null)
            return false;
        return s_bindings.reparent_entity(uuid, parentUuid) != 0;
    }

    private static string ReadString(delegate* unmanaged<byte*, int, int> fn)
    {
        int length = fn(null, 0);
        if (length <= 0)
            return string.Empty;
        byte[] buffer = new byte[length + 1];
        fixed (byte* p = buffer)
            fn(p, buffer.Length);
        return Encoding.UTF8.GetString(buffer, 0, length);
    }

    private static byte[] Utf8Z(string value)
    {
        byte[] bytes = Encoding.UTF8.GetBytes(value ?? string.Empty);
        Array.Resize(ref bytes, bytes.Length + 1);
        return bytes;
    }
}
