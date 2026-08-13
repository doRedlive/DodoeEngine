// do@Redlive

#include "service_manager.h"

namespace dodoe {

    Bool ServiceManager::initialize(const ServiceManagerCreateInfo&) {
        initializeAll();
        return true;
    }

    void ServiceManager::shutdown() {
        shutdownAll();
    }

    void ServiceManager::registerService(std::function<void()> initialize_fn, std::function<void()> shutdown_fn) {
        m_services.push_back(Service{ std::move(initialize_fn), std::move(shutdown_fn) });
    }

    void ServiceManager::initializeAll() {
        for (auto& service : m_services) {
            if (service.initialize) {
                service.initialize();
            }
        }
    }

    void ServiceManager::shutdownAll() {
        for (auto it = m_services.rbegin(); it != m_services.rend(); ++it) {
            if (it->shutdown) {
                (it->shutdown)();
            }
        }
        m_services.clear();
    }

} // namespace dodoe
