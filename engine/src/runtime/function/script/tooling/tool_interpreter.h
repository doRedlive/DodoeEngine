// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

    class ToolInterpreter {
    public:
        Bool Initialize();
        void Shutdown();
        Bool Execute(const FsPath& path);

    private:
#ifdef DODOE_PYTHON_ENABLED
        Scope<pybind11::scoped_interpreter> m_guard;
#endif
    };

} // namespace dodoe
