#include <gtest/gtest.h>

#include "core/console/mcp/McpHttp.h"
#include "core/console/mcp/McpRpc.h"

namespace {

using cakery::mcp::HttpBuffer;
using cakery::mcp::HttpRequestHead;
using cakery::mcp::ParseRequestHead;
using cakery::mcp::ReadBody;

TEST(McpHttp, ParsesPostWithHeadersAndBody) {
    const std::string raw =
        "POST /mcp HTTP/1.1\r\n"
        "Host: 127.0.0.1\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 15\r\n"
        "Mcp-Session-Id: abc123\r\n"
        "\r\n"
        R"({"jsonrpc":"2.0"})";
    HttpBuffer buffer;
    buffer.feed(raw.data(), raw.size());
    HttpRequestHead head;
    bool fed = false;
    ASSERT_TRUE(ParseRequestHead(buffer, head, [&fed](HttpBuffer&) {
        fed = true;
        return fed;
    }));
    EXPECT_EQ(head.method, "POST");
    EXPECT_EQ(head.target, "/mcp");
    EXPECT_EQ(head.content_length, 15u);
    EXPECT_EQ(head.session_id, "abc123");
    EXPECT_TRUE(head.valid);

    std::string body;
    EXPECT_TRUE(ReadBody(buffer, head.content_length, body, [](HttpBuffer&) { return false; }));
    EXPECT_EQ(body, R"({"jsonrpc":"2.0"})");
}

TEST(McpHttp, NeedsMoreDataUntilFed) {
    HttpBuffer buffer;
    buffer.feed("POST /mcp HTTP/1.1\r\n", 20);
    HttpRequestHead head;
    EXPECT_FALSE(ParseRequestHead(buffer, head, [](HttpBuffer&) { return false; }));
    buffer.feed("Content-Length: 2\r\n\r\nok", 23);
    bool fed = false;
    ASSERT_TRUE(ParseRequestHead(buffer, head, [&fed](HttpBuffer&) {
        fed = true;
        return true;
    }));
    EXPECT_EQ(head.content_length, 2u);
}

TEST(McpHttp, StripsQueryFromTarget) {
    HttpBuffer buffer;
    buffer.feed("GET /mcp?session=1 HTTP/1.1\r\n\r\n", 31);
    HttpRequestHead head;
    ASSERT_TRUE(ParseRequestHead(buffer, head, [](HttpBuffer&) { return false; }));
    EXPECT_EQ(head.target, "/mcp");
    EXPECT_EQ(head.method, "GET");
}

TEST(McpHttp, RejectsOversizedBody) {
    HttpBuffer buffer;
    buffer.feed("POST /mcp HTTP/1.1\r\nContent-Length: 5\r\n\r\n", 42);
    HttpRequestHead head;
    ASSERT_TRUE(ParseRequestHead(buffer, head, [](HttpBuffer&) { return false; }));
    std::string body;
    EXPECT_FALSE(ReadBody(buffer, cakery::mcp::kMaxBodyBytes + 1, body,
                          [](HttpBuffer&) { return false; }));
}

TEST(McpHttp, HeaderSizeLimitReturnsInvalid) {
    std::string raw = "POST /mcp HTTP/1.1\r\n";
    raw.append(static_cast<std::size_t>(cakery::mcp::kMaxHeaderBytes) + 16, 'x');
    raw += "\r\n\r\n";
    HttpBuffer buffer;
    buffer.feed(raw.data(), raw.size());
    HttpRequestHead head;
    EXPECT_FALSE(ParseRequestHead(buffer, head, [](HttpBuffer&) { return false; }));
}

TEST(McpRpc, ErrorAndResultShapes) {
    const nlohmann::json id = 7;
    const nlohmann::json error = cakery::mcp::RpcError(id, -32601, "Method not found");
    EXPECT_EQ(error["jsonrpc"], "2.0");
    EXPECT_EQ(error["id"], 7);
    EXPECT_EQ(error["error"]["code"], -32601);
    EXPECT_EQ(error["error"]["message"], "Method not found");

    const nlohmann::json result = cakery::mcp::RpcResult(id, nlohmann::json::object());
    EXPECT_EQ(result["result"], nlohmann::json::object());
}

TEST(McpRpc, InitializeResultNegotiatesVersion) {
    const nlohmann::json negotiated =
        cakery::mcp::BuildInitializeResult("2025-03-26");
    EXPECT_EQ(negotiated["protocolVersion"], "2025-03-26");
    EXPECT_TRUE(negotiated["capabilities"].contains("tools"));
    EXPECT_TRUE(negotiated["serverInfo"].contains("name"));

    const nlohmann::json fallback = cakery::mcp::BuildInitializeResult("1999-01-01");
    EXPECT_STREQ(fallback["protocolVersion"].get<std::string>().c_str(),
                 cakery::mcp::kProtocolVersion);
}

TEST(McpRpc, ToolCatalogMapsInputSchema) {
    nlohmann::json schema = nlohmann::json::array();
    schema.push_back(nlohmann::json{
        {"name", "entity.create"},
        {"description", "Create a new GameObject"},
        {"parameters", nlohmann::json{{"type", "object"},
                                      {"properties", nlohmann::json::object()}}}});
    const nlohmann::json catalog = cakery::mcp::BuildToolCatalog(schema);
    ASSERT_TRUE(catalog.contains("tools"));
    ASSERT_EQ(catalog["tools"].size(), 1u);
    EXPECT_EQ(catalog["tools"][0]["name"], "entity.create");
    EXPECT_TRUE(catalog["tools"][0].contains("inputSchema"));
    EXPECT_FALSE(catalog["tools"][0].contains("parameters"));
}

TEST(McpRpc, ToolCallResultWrapsText) {
    const nlohmann::json result = cakery::mcp::ToolCallResult(R"({"ok":true})", false);
    ASSERT_TRUE(result["content"].is_array());
    EXPECT_EQ(result["content"][0]["type"], "text");
    EXPECT_EQ(result["content"][0]["text"], R"({"ok":true})");
    EXPECT_FALSE(result["isError"]);
}

} // namespace
