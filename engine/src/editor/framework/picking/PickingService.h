// do@Redlive

#pragma once

#include "runtime/core/utils/uuid.h"
#include "runtime/core/math/math.h"
#include <optional>
#include <vector>

namespace cakery {

class EditorContext;

class PickingService {
public:
    explicit PickingService(EditorContext& ctx) : m_ctx(ctx) {}

    std::optional<dodoe::UUID> pick(float screenX, float screenY);
    std::vector<dodoe::UUID> pickRect(float x0, float y0, float x1, float y1);

private:
    EditorContext& m_ctx;
};

} // namespace cakery
