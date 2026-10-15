// do@Redlive

#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace cakery::mcp {

inline constexpr const char* kProtocolVersion = "2025-06-18";
inline constexpr const char* kServerName = "cakery-editor";
inline constexpr const char* kServerVersion = "1.0.0";

inline bool IsSupportedProtocolVersion(const std::string& version) {
    return version == "2024-11-05" || version == "2025-03-26" || version == "2025-06-18";
}

inline nlohmann::json RpcError(const nlohmann::json& id, int code, const std::string& message) {
    return nlohmann::json{{"jsonrpc", "2.0"}, {"id", id},
                          {"error", {{"code", code}, {"message", message}}}};
}

inline nlohmann::json RpcResult(const nlohmann::json& id, nlohmann::json result) {
    return nlohmann::json{{"jsonrpc", "2.0"}, {"id", id}, {"result", std::move(result)}};
}

inline nlohmann::json BuildInitializeResult(const std::string& requestedVersion) {
    nlohmann::json result;
    result["protocolVersion"] =
        IsSupportedProtocolVersion(requestedVersion) ? requestedVersion : kProtocolVersion;
    result["capabilities"] = nlohmann::json{
        {"tools", nlohmann::json::object()},
        {"resources", nlohmann::json{{"subscribe", false}, {"listChanged", false}}},
        {"prompts", nlohmann::json{{"listChanged", false}}}};
    result["serverInfo"] = nlohmann::json{{"name", kServerName}, {"version", kServerVersion}};
    return result;
}

inline nlohmann::json BuildToolCatalog(const nlohmann::json& toolSchema) {
    nlohmann::json tools = nlohmann::json::array();
    if (toolSchema.is_array()) {
        for (const auto& entry : toolSchema) {
            nlohmann::json tool;
            tool["name"] = entry["name"];
            tool["description"] = entry.value("description", std::string());
            tool["inputSchema"] = entry.value("parameters", nlohmann::json::object());
            tools.push_back(std::move(tool));
        }
    }
    return nlohmann::json{{"tools", std::move(tools)}};
}

inline nlohmann::json ToolCallResult(const std::string& text, bool isError) {
    return nlohmann::json{
        {"content", nlohmann::json::array({nlohmann::json{{"type", "text"}, {"text", text}}})},
        {"isError", isError}};
}

} // namespace cakery::mcp
