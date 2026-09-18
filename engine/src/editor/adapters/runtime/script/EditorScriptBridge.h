// do@Redlive

#pragma once

#include <nlohmann/json.hpp>

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

    [[nodiscard]] static bool GetCustomInspectorUI(const std::string& typeName, nlohmann::json& out);
    [[nodiscard]] static bool ListEditorWindows(std::vector<std::pair<std::string, std::string>>& out);
    [[nodiscard]] static bool GetEditorWindowUI(const std::string& id, nlohmann::json& out);
    static bool DispatchEditorEvent(const std::string& owner, const std::string& ownerId,
                                    const std::string& controlId, const std::string& eventName,
                                    const nlohmann::json& value);
};

} // namespace cakery
