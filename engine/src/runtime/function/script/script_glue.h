// do@GreenMuffin

#pragma once

namespace dodoe {

    class ScriptGlue {
    public:
        static void Register();

    private:
        static void RegisterComponents();
        static void RegisterFunctions();
    };

} // dodoe