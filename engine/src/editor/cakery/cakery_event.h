// do->GreenMuffin

#pragma once

#include "runtime/function/world/entity.h"

namespace cakery {

	struct SelectEntityEvent {
		dodoe::Entity select;
		explicit SelectEntityEvent(dodoe::Entity in_select) : select(in_select) { }
	};

	struct NonSelectEntityEvent { };

} // cakery
