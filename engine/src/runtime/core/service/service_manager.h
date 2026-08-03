// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/memory/managed.h"

#include <functional>

namespace dodoe {

    struct ServiceManagerCreateInfo {};

    class ServiceManager : public Managed<ServiceManager, ServiceManagerCreateInfo> {
        friend class Managed<ServiceManager, ServiceManagerCreateInfo>;

        struct Service {
            std::function<void()> initialize;
            std::function<void()> shutdown;
        };

        DynamicArray<Service> m_services{};

        Bool initialize(const ServiceManagerCreateInfo& info);
        void shutdown();

    public:
        void registerService(std::function<void()> initialize_fn, std::function<void()> shutdown_fn);
        void initializeAll();
        void shutdownAll();
    };

} // namespace dodoe
