#include "system.h"

namespace dodoe {

    System::~System() = default;

    void System::start(Registry& reg) {
        (void)reg;
    }

    void System::update(Registry& reg, float dt) {
        (void)reg;
        (void)dt;
    }

    void System::finalize(Registry& reg) {
        (void)reg;
    }

} // dodoe
