//
// Created by Redlive on 2026/3/19.
//

#ifndef DODOE_RENDER_DRAWER_H
#define DODOE_RENDER_DRAWER_H

#include "dopch.h"

#include "runtime/core/utils/util.h"

namespace dodoe {

	class RenderDrawer {
	public:
		static void clear_color(const Color& color);
		static void draw_elements(ui32 index_count);
	};

} // dodoe

#endif // DODOE_RENDER_DRAWER_H
