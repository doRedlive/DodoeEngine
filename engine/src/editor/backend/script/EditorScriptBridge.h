// do@Redlive
#pragma once
#include "dopch.h"


#include <string>
#include <utility>
#include <vector>

namespace cakery {

class EditorSession;

class EditorScriptBridge {
public:
    static void Initialize(EditorSession* session);
    static void Shutdown();
    [[nodiscard]] static bool IsRegistered();

    [[nodiscard]] static bool GetCustomInspectorUI(const String& typeName, Json& out);
    [[nodiscard]] static bool ListEditorWindows(DynamicArray<Pair<String, String>>& out);
    [[nodiscard]] static bool GetEditorWindowUI(const String& id, Json& out);
    static bool DispatchEditorEvent(const String& owner, const String& ownerId,
                                    const String& controlId, const String& eventName,
                                    const Json& value);
};

} // namespace cakery
