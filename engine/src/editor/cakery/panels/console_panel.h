// do@Redlive

#pragma once

#include "dopch.h"

#include "cakery/framework/editor_panel.h"

#include <array>

namespace cakery {

	class ConsolePanel : public EditorPanel {
	private:
		Bool m_auto_scroll{false};
		Bool m_collapse_repeats{true};
		Bool m_clear_on_play{false};
		Int32 m_filter{0};
		std::array<char, 128> m_search_buffer{};

	public:
		explicit ConsolePanel(EditorPanelDescriptor descriptor);
		void onDraw(const EditorPanelContext& context) override;
	};

} // cakery
