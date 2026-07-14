// do@Redlive

#pragma once

#include "dopch.h"
#include "object.h"

namespace dodoe {
    namespace native_bridge {

        void NotifyDestroyed(InstanceID id);
        void AcquireRef(InstanceID id);
        void ReleaseRef(InstanceID id);
        void DrainReleases();

        void RegisterRoot(InstanceID id);
        void UnregisterRoot(InstanceID id);

        const UnorderedSet<InstanceID>& GetAcquiredSet();

    } // namespace native_bridge
} // namespace dodoe
