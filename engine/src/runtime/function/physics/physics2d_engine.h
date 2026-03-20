//
// Created by GreenMuffin on 2026/2/20.
//

#ifndef DODOE_PHYSICS2D_H
#define DODOE_PHYSICS2D_H

#include "dopch.h"

#include "box2d/box2d.h"
#include "box2d/types.h"

namespace dodoe {

	class Scene;

	class Physics2dEngine {
	public:
		static void start(Scene* context);
		static void stop();

	private:
		static b2WorldId world_id_;
		
	};
}

#endif//!DODOE_PHYSICS2D_H
