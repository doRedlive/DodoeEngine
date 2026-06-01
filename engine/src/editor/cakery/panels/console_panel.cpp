// do@Redlive

#include "console_panel.h"

#include "cakery/helper/lang_config.h"
#include "runtime/function/log/log_system.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace cakery {

	namespace {

		Bool PassLevelFilter(const dodoe::LogLevel level, const Int32 filter) {
			if (filter <= 0) {
				return true;
			}

			return static_cast<Int32>(level) >= (filter - 1);
		}

		Bool ContainsInsensitive(const String& text, const char* filter_text) {
			if (filter_text == nullptr || filter_text[0] == '\0') {
				return true;
			}

			const auto matches = [](const char lhs, const char rhs) {
				return std::tolower(static_cast<unsigned char>(lhs))
					== std::tolower(static_cast<unsigned char>(rhs));
			};

			return std::search(text.begin(), text.end(), filter_text, filter_text + std::strlen(filter_text), matches)
				!= text.end();
		}

		Bool PassTextFilter(const dodoe::LogMessage& message, const char* filter_text) {
			return ContainsInsensitive(message.content, filter_text)
				|| ContainsInsensitive(message.payload, filter_text)
				|| ContainsInsensitive(message.logger_name, filter_text);
		}

		ImVec4 GetLogColor(const dodoe::LogLevel level) {
			switch (level) {
				case dodoe::LogLevel::Trace:
					return {0.70f, 0.70f, 0.70f, 1.0f};
				case dodoe::LogLevel::Debug:
					return {0.80f, 0.80f, 0.80f, 1.0f};
				case dodoe::LogLevel::Info:
					return {1.0f, 1.0f, 1.0f, 1.0f};
				case dodoe::LogLevel::Warn:
					return {1.0f, 0.85f, 0.20f, 1.0f};
				case dodoe::LogLevel::Error:
					return {1.0f, 0.35f, 0.35f, 1.0f};
				case dodoe::LogLevel::Critical:
					return {1.0f, 0.20f, 0.85f, 1.0f};
				default:
					return {1.0f, 1.0f, 1.0f, 1.0f};
			}
		}

		void DrawLogMessageCollapsed(const dodoe::LogMessage& message) {
			ImGui::Separator();

			if (message.repeat_count > 1) {
				ImGui::TextDisabled("[x%u]", message.repeat_count);
				ImGui::SameLine();
			}

			ImGui::PushStyleColor(ImGuiCol_Text, GetLogColor(message.level));
			ImGui::TextUnformatted(message.content.c_str());
			ImGui::PopStyleColor();
		}

		void DrawLogMessageExpanded(const dodoe::LogMessage& message) {
			for (UInt32 i = 0; i < std::max(1u, message.repeat_count); ++i) {
				ImGui::Separator();
				ImGui::PushStyleColor(ImGuiCol_Text, GetLogColor(message.level));
				ImGui::TextUnformatted(message.content.c_str());
				ImGui::PopStyleColor();
			}
		}

	}

	ConsolePanel::ConsolePanel(EditorPanelDescriptor descriptor)
		: EditorPanel(std::move(descriptor)) {
	}

	void ConsolePanel::onDraw(const EditorPanelContext& context) {
		(void)context;
		ImGui::Begin("Output");

		const char* filter_items[] = {
			LangConfig::TR("ALL"),
			LangConfig::TR("TRACE"),
			LangConfig::TR("DEBUG"),
			LangConfig::TR("INFO"),
			LangConfig::TR("WARN"),
			LangConfig::TR("ERROR"),
			LangConfig::TR("CRITICAL"),
		};

		auto logs = dodoe::Log::GetClientLogs();
		auto core_logs = dodoe::Log::GetCoreLogs();
		logs.insert(logs.end(), core_logs.begin(), core_logs.end());
		std::sort(logs.begin(), logs.end(), [](const dodoe::LogMessage& lhs, const dodoe::LogMessage& rhs) {
			return lhs.sequence < rhs.sequence;
		});

		// Count messages by severity (Unity-style badges)
		UInt32 error_count = 0;
		UInt32 warn_count = 0;
		UInt32 info_count = 0;
		for (const auto& message : logs) {
			switch (message.level) {
				case dodoe::LogLevel::Error:
				case dodoe::LogLevel::Critical:
					++error_count;
					break;
				case dodoe::LogLevel::Warn:
					++warn_count;
					break;
				default:
					++info_count;
					break;
			}
		}

		// Toolbar row
		{
			// Message count badges
			ImGui::TextUnformatted(LangConfig::TR("ERROR"));
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%u", error_count);
			ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 2.0f);

			ImGui::TextUnformatted(LangConfig::TR("WARN"));
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.20f, 1.0f), "%u", warn_count);
			ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 2.0f);

			ImGui::TextUnformatted(LangConfig::TR("INFO"));
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%u", info_count);
			ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 3.0f);

			// Clear button
			if (ImGui::Button(LangConfig::TR("BIN_CLEAR"))) {
				dodoe::Log::ClearCoreLogs();
				dodoe::Log::ClearClientLogs();
			}
			ImGui::SameLine();

			// Collapse toggle
			ImGui::Checkbox(LangConfig::TR("COLLAPSE"), &m_collapse_repeats);
			ImGui::SameLine();

			// Clear on Play
			ImGui::Checkbox(LangConfig::TR("CLEAR_ON_PLAY"), &m_clear_on_play);
			ImGui::SameLine();

			// Auto-scroll
			ImGui::Checkbox(LangConfig::TR("AUTO_SCROLL"), &m_auto_scroll);
			ImGui::SameLine();

			// Filter combo
			ImGui::SetNextItemWidth(100.0f);
			ImGui::Combo(LangConfig::TR("FILTER"), &m_filter, filter_items, IM_ARRAYSIZE(filter_items));
			ImGui::SameLine();

			// Search
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::InputTextWithHint("##ConsoleSearch", LangConfig::TR("SEARCH"), m_search_buffer.data(), static_cast<Int32>(m_search_buffer.size()));
		}

		ImGui::BeginChild("LogScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
		const Bool was_at_bottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();

		for (const auto& message : logs) {
			if (!PassLevelFilter(message.level, m_filter) || !PassTextFilter(message, m_search_buffer.data())) {
				continue;
			}

			if (m_collapse_repeats) {
				DrawLogMessageCollapsed(message);
			} else {
				DrawLogMessageExpanded(message);
			}
		}

		if (m_auto_scroll && was_at_bottom) {
			ImGui::SetScrollHereY(1.0f);
		}

		ImGui::EndChild();
		ImGui::End();
	}

} // cakery
