// do@Redlive

#pragma once

#include "dopch.h"

#include <functional>

namespace dodoe {

    class TaskScheduler;

    class TaskGraph {
    public:
        using NodeWork = std::function<void()>;
        using NodeId = uint32_t;

        static constexpr NodeId kInvalidNodeId = UINT32_MAX;

        NodeId addNode(String name, NodeWork work);
        void addEdge(NodeId before, NodeId after);
        void compile();
        void execute(TaskScheduler& scheduler);
        void reset();

        [[nodiscard]] bool isCompiled() const { return m_compiled; }
        [[nodiscard]] Size_t nodeCount() const { return m_nodes.size(); }
        [[nodiscard]] Size_t levelCount() const { return m_levels.size(); }
        [[nodiscard]] const DynamicArray<DynamicArray<Size_t>>& getEdges() const { return m_edges; }
        [[nodiscard]] const DynamicArray<DynamicArray<Size_t>>& getLevels() const { return m_levels; }
        [[nodiscard]] const String& nodeName(NodeId id) const { return m_nodes[id].name; }

    private:
        struct Node {
            String name;
            NodeWork work;
        };

        DynamicArray<Node> m_nodes{};
        DynamicArray<DynamicArray<Size_t>> m_edges{};
        DynamicArray<DynamicArray<Size_t>> m_levels{};
        bool m_compiled{false};
    };

} // namespace dodoe