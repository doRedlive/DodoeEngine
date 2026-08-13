// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/math/math.h"
#include "runtime/core/object/object.h"

namespace dodoe {

    struct BoneBindPose {
        Vector3f position{0.0f};
        Quaternion rotation{1.0f, 0.0f, 0.0f, 0.0f};
        Vector3f scale{1.0f};

        [[nodiscard]] Matrix4f toMatrix() const {
            Matrix4f matrix(1.0f);
            matrix = Math::Translate(matrix, position);
            matrix = matrix * glm::mat4_cast(rotation);
            matrix = Math::Scale(matrix, scale);
            return matrix;
        }
    };

    struct SkeletonNode {
        String name{};
        Int32 parent{-1};
        BoneBindPose bind_pose{};
    };

    class DODOE_API Skeleton : public Object {
        DynamicArray<SkeletonNode> m_nodes{};
        Int32 m_root{-1};

    public:
        static constexpr Int32 kInvalidNode = -1;
        static constexpr UInt32 kLocalId = 1;

        Skeleton() = default;
        explicit Skeleton(const ObjectID& id)
            : Object(id) {}

        [[nodiscard]] const char* getObjectTypeName() const override { return "Skeleton"; }

        void clear() {
            m_nodes.clear();
            m_root = -1;
        }

        Int32 addNode(const String& name, const Int32 parent, const BoneBindPose& bind_pose) {
            if (m_nodes.empty()) {
                m_root = 0;
            }
            SkeletonNode node;
            node.name = name;
            node.parent = parent;
            node.bind_pose = bind_pose;
            m_nodes.push_back(std::move(node));
            return static_cast<Int32>(m_nodes.size()) - 1;
        }

        [[nodiscard]] Size_t getNodeCount() const { return m_nodes.size(); }
        [[nodiscard]] Int32 getRoot() const { return m_root; }
        [[nodiscard]] const SkeletonNode& getNode(const Int32 index) const { return m_nodes[index]; }

        [[nodiscard]] Int32 findNode(const String& name) const {
            for (Size_t i = 0; i < m_nodes.size(); ++i) {
                if (m_nodes[i].name == name) {
                    return static_cast<Int32>(i);
                }
            }
            return kInvalidNode;
        }

        void computeBindWorldMatrices(DynamicArray<Matrix4f>& out_world) const {
            out_world.resize(m_nodes.size());
            for (Size_t i = 0; i < m_nodes.size(); ++i) {
                const auto& node = m_nodes[i];
                if (node.parent >= 0) {
                    out_world[i] = out_world[static_cast<Size_t>(node.parent)] * node.bind_pose.toMatrix();
                }
                else {
                    out_world[i] = node.bind_pose.toMatrix();
                }
            }
        }

        void computeWorldMatrices(const DynamicArray<BoneBindPose>& local_poses,
                                  DynamicArray<Matrix4f>& out_world) const {
            out_world.resize(m_nodes.size());
            for (Size_t i = 0; i < m_nodes.size(); ++i) {
                const auto& node = m_nodes[i];
                const BoneBindPose& local = i < local_poses.size() ? local_poses[i] : node.bind_pose;
                if (node.parent >= 0) {
                    out_world[i] = out_world[static_cast<Size_t>(node.parent)] * local.toMatrix();
                }
                else {
                    out_world[i] = local.toMatrix();
                }
            }
        }

        void computeSkinningMatrices(const DynamicArray<Matrix4f>& animated_world,
                                     DynamicArray<Matrix4f>& out_skinning) const {
            DynamicArray<Matrix4f> bind_world;
            computeBindWorldMatrices(bind_world);
            out_skinning.resize(m_nodes.size());
            for (Size_t i = 0; i < m_nodes.size(); ++i) {
                out_skinning[i] = Math::Inverse(bind_world[i]) * animated_world[i];
            }
        }

        [[nodiscard]] static Skeleton* Create(const ObjectID& id);
        static void Shutdown();
    };

} // dodoe
