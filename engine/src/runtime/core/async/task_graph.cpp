// do@Redlive

#include "task_graph.h"
#include "task_scheduler.h"

namespace dodoe {

    TaskGraph::NodeId TaskGraph::addNode(String name, NodeWork work) {
        DO_ASSERT(!m_compiled, "Cannot add node after compile");
        NodeId id = static_cast<NodeId>(m_nodes.size());
        m_nodes.push_back({std::move(name), std::move(work)});
        return id;
    }

    void TaskGraph::addEdge(NodeId before, NodeId after) {
        DO_ASSERT(!m_compiled, "Cannot add edge after compile");
        DO_ASSERT(before < m_nodes.size() && after < m_nodes.size(), "TaskGraph::addEdge: invalid node id");

        if (m_edges.size() < m_nodes.size()) {
            m_edges.resize(m_nodes.size());
        }
        m_edges[before].push_back(after);
    }

    void TaskGraph::compile() {
        m_levels.clear();
        m_compiled = false;

        const Size_t node_count = m_nodes.size();
        if (node_count == 0) {
            m_compiled = true;
            return;
        }

        m_edges.resize(node_count);

        DynamicArray<Int32> indegree(node_count, 0);
        for (Size_t i = 0; i < node_count; i++) {
            for (const auto to : m_edges[i]) {
                indegree[to] += 1;
            }
        }

        DynamicArray<Size_t> current_level{};
        current_level.reserve(node_count);
        for (Size_t i = 0; i < node_count; i++) {
            if (indegree[i] == 0) {
                current_level.push_back(i);
            }
        }

        Size_t processed = 0;
        while (processed < node_count) {
            if (current_level.empty()) {
                String remaining;
                for (Size_t i = 0; i < node_count; i++) {
                    if (indegree[i] > 0) {
                        remaining += String("node[") + std::to_string(i).c_str() + "] indegree=" + std::to_string(indegree[i]).c_str() + " ";
                    }
                }
                DO_ERROR("TaskGraph has cyclic dependency, remaining: {}", remaining);
                DO_ASSERT(false, "TaskGraph has cyclic dependency");
            }

            DynamicArray<Size_t> next_level{};
            DynamicArray<Size_t> execution_level{};

            for (const auto node : current_level) {
                execution_level.push_back(node);

                for (const auto to : m_edges[node]) {
                    indegree[to] -= 1;
                    if (indegree[to] == 0) {
                        next_level.push_back(to);
                    }
                }
            }

            if (!execution_level.empty()) {
                m_levels.push_back(std::move(execution_level));
            }

            processed += current_level.size();
            current_level = std::move(next_level);
        }

        m_compiled = true;
    }

    void TaskGraph::execute(TaskScheduler& scheduler) {
        DO_ASSERT(m_compiled, "TaskGraph must be compiled before execute");

        for (const auto& level : m_levels) {
            if (level.size() == 1) {
                const auto& node = m_nodes[level[0]];
                if (node.work) {
                    node.work();
                }
            }
            else {
                std::atomic<Size_t> completed{0};
                const Size_t count = level.size();

                for (const auto node_index : level) {
                    const auto& node = m_nodes[node_index];
                    scheduler.submit([&node, &completed]() {
                        if (node.work) {
                            node.work();
                        }
                        completed.fetch_add(1, std::memory_order_relaxed);
                    });
                }

                while (completed.load(std::memory_order_relaxed) < count) {
                    std::this_thread::yield();
                }
            }
        }
    }

    void TaskGraph::reset() {
        m_nodes.clear();
        m_edges.clear();
        m_levels.clear();
        m_compiled = false;
    }

} // namespace dodoe
