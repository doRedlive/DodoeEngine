// do@Redlive
#pragma once
#include "dopch.h"

#include <string>

namespace cakery::mcp {

inline constexpr const char* kProtocolVersion = "2025-06-18";
inline constexpr const char* kServerName = "cakery-editor";
inline constexpr const char* kServerVersion = "1.0.0";

inline bool IsSupportedProtocolVersion(const String& version) {
    return version == "2024-11-05" || version == "2025-03-26" || version == "2025-06-18";
}

inline Json RpcError(const Json& id, int code, const String& message) {
    return Json{{"jsonrpc", "2.0"}, {"id", id},
                          {"error", {{"code", code}, {"message", message}}}};
}

inline Json RpcResult(const Json& id, Json result) {
    return Json{{"jsonrpc", "2.0"}, {"id", id}, {"result", std::move(result)}};
}

inline Json BuildInitializeResult(const String& requestedVersion) {
    Json result;
    result["protocolVersion"] =
        IsSupportedProtocolVersion(requestedVersion) ? requestedVersion : kProtocolVersion;
    result["capabilities"] = Json{
        {"tools", Json::object()},
        {"resources", Json{{"subscribe", false}, {"listChanged", false}}},
        {"prompts", Json{{"listChanged", false}}}};
    result["serverInfo"] = Json{{"name", kServerName}, {"version", kServerVersion}};
    return result;
}

inline Json BuildToolCatalog(const Json& toolSchema) {
    Json tools = Json::array();
    if (toolSchema.is_array()) {
        for (const auto& entry : toolSchema) {
            Json tool;
            tool["name"] = entry["name"];
            tool["description"] = entry.value("description", String());
            tool["inputSchema"] = entry.value("parameters", Json::object());
            tools.push_back(std::move(tool));
        }
    }
    return Json{{"tools", std::move(tools)}};
}

inline Json ToolCallResult(const String& text, bool isError) {
    return Json{
        {"content", Json::array({Json{{"type", "text"}, {"text", text}}})},
        {"isError", isError}};
}

} // namespace cakery::mcp
