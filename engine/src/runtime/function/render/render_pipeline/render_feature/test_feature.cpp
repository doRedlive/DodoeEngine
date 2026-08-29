// do@Redlive

#include "test_feature.h"

namespace dodoe {

    void TestFeature::collectPasses(PassCollector& collector) {
        collector.addPass<TestPass>();
    }

} // namespace dodoe
