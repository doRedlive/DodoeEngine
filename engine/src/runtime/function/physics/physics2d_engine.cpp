//
// Created by GreenMuffin on 2026/2/20.
//

#include "physics2d_engine.h"

#include "runtime/core/world/scene.h"

namespace dodoe {

	b2WorldId Physics2dEngine::world_id_ = b2WorldId();

	void Physics2dEngine::start(Scene* scene) {
		(void)scene;

		if (B2_IS_NON_NULL(world_id_)) {
			return;
		}

		b2WorldDef world_def = b2DefaultWorldDef();
		world_def.gravity.x = 0.0f;
		world_def.gravity.y = -9.8f;
		world_id_ = b2CreateWorld(&world_def);
	}

	void Physics2dEngine::stop() {
		if (B2_IS_NON_NULL(world_id_)) {
			b2DestroyWorld(world_id_);
		}

		world_id_ = b2_nullWorldId;
	}
}