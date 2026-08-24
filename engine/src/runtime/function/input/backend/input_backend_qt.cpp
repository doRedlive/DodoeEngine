// do@Redlive

#include "input_backend_qt.h"

namespace dodoe {

    Bool InputBackendQt::initialize(InputRawState& raw_state, void* native_window) {
        m_host_handle = native_window;
        m_raw_state = &raw_state;
        return true;
    }

    void InputBackendQt::shutdown() {
        m_raw_state = nullptr;
        m_host_handle = nullptr;
    }

    void InputBackendQt::poll(InputRawState& raw_state) {
        (void)raw_state;
    }

} // namespace dodoe
