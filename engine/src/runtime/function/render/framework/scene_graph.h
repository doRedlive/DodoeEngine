// do@Redlive
#pragma once

#include "dopch.h"

#include "runtime/core/utils/uuid.h"
#include "runtime/core/utils/util.h"
#include "../render_types.h"

namespace dodoe {

    namespace {
        template <typename T>
        class ResourceTracker {
            using UnderlyingConstIterator = typename std::unordered_map<Ref<T>, ui32>::const_iterator;

            std::unordered_map<Ref<T>, ui32> m_umap;
        public:
            class ConstIterator {
                UnderlyingConstIterator m_iter;
            public:
                ConstIterator(UnderlyingConstIterator iter) : m_iter(std::move(iter)) { }
                ConstIterator& operator++() { m_iter++; return *this; }
                ConstIterator operator++(int) { ConstIterator res = *this; m_iter++; return res; }
                bool operator==(ConstIterator other) const { return m_iter == other.m_iter; }
                bool operator!=(ConstIterator other) const { return !(*this == other); }
                const Ref<T>& operator*() const { return m_iter->first; }
            };

            bool addRef(const Ref<T>& resource) {
                if (!resource) return false;
                const ui32 ref_count = ++m_umap[resource];
                return ref_count == 1;
            }

            bool release(const Ref<T>& resource) {
                if (!resource) return false;
                auto it = m_umap.find(resource);
                if (it == m_umap.end()) {
                    DO_ASSERT(false, "Try to release an object not owned by this tracked!");
                    return false;
                }

                if (it->second == 0) {
                    DO_ASSERT(false);
                } 
                else {
                    --it->second;
                }

                if (it->second == 0) {
                    m_umap.erase(it);
                    return true;
                }
                return false;
            }

            void clear() { m_umap.clear(); }

            [[nodiscard]] ConstIterator begin() const { return ConstIterator(m_umap.cbegin()); }
            [[nodiscard]] ConstIterator end()   const { return ConstIterator(m_umap.cend()); }
            [[nodiscard]] bool   empty() const { return m_umap.empty(); }
            [[nodiscard]] size_t size()  const { return m_umap.size(); }
            [[nodiscard]] bool contains(const Ref<T>& resource) const { return m_umap.contains(resource); }
        };
    }

    class SceneGraph;
    class SceneGraphNode;
    class SceneGraphLeaf;

    enum class SceneGraphLeafType {
        MeshInstance,
        SceneCamera,
        Light,
        Animation,
    };

    class SceneGraphLeaf {
        friend class SceneGraphNode;
        Weak<SceneGraphNode> m_node;
    protected:
        SceneGraphLeaf() = default;
    public:
        virtual ~SceneGraphLeaf() = default;

        [[nodiscard]] SceneGraphNode* getNode() const { return m_node.lock().get(); }
        [[nodiscard]] Ref<SceneGraphNode> getNodePtr() const { return m_node.lock(); }
        [[nodiscard]] const std::string& getName() const;
        [[nodiscard]] virtual SceneGraphLeafType getType() const = 0;
        [[nodiscard]] virtual Ref<SceneGraphLeaf> clone() = 0;

        void setName(const std::string& name) const;

    private:
        void setNode(const Ref<SceneGraphNode>& node);

        SceneGraphLeaf(const SceneGraphLeaf&) = delete;
        SceneGraphLeaf(const SceneGraphLeaf&&) = delete;
        SceneGraphLeaf& operator=(const SceneGraphLeaf&) = delete;
        SceneGraphLeaf& operator=(const SceneGraphLeaf&&) = delete;
    };

    class MeshInstance : public SceneGraphLeaf {
        friend class SceneGraph;
        int m_instance_index{-1};
        int m_geometry_instance_index{-1};
        Ref<Mesh> m_mesh;
    public:
        MeshInstance() = default;
        explicit MeshInstance(Ref<Mesh> mesh);

        [[nodiscard]] SceneGraphLeafType getType() const override { return SceneGraphLeafType::MeshInstance; }
        [[nodiscard]] const Ref<Mesh>& getMesh() const { return m_mesh; }
        [[nodiscard]] int getInstanceIndex() const { return m_instance_index; }
        [[nodiscard]] int getGeometryInstanceIndex() const { return m_geometry_instance_index; }
        [[nodiscard]] size_t getGeometryCount() const { return m_mesh ? m_mesh->geometries.size() : 0; }
        [[nodiscard]] Matrix4f getModelMatrix() const;
        [[nodiscard]] Ref<SceneGraphLeaf> clone() override;

        void setMesh(const Ref<Mesh>& mesh) { m_mesh = mesh; }
        void setInstanceIndex(int instance_index) { m_instance_index = instance_index; }
        void setGeometryInstanceIndex(int geometry_instance_index) { m_geometry_instance_index = geometry_instance_index; }
    };

    class SceneCamera : public SceneGraphLeaf {
        Matrix4f m_view_proj_matrix{1.0f};
        Vector3f m_position{0.0f};
        bool m_valid{false};
    public:
        SceneCamera() = default;

        [[nodiscard]] SceneGraphLeafType getType() const override { return SceneGraphLeafType::SceneCamera; }
        void setViewProjectionMatrix(const Matrix4f& matrix) { m_view_proj_matrix = matrix; m_valid = true; }
        void setPosition(const Vector3f& position) { m_position = position; }
        void invalidate() { m_valid = false; }

        [[nodiscard]] const Matrix4f& getViewProjectionMatrix() const { return m_view_proj_matrix; }
        [[nodiscard]] const Vector3f& getPosition() const { return m_position; }
        [[nodiscard]] bool isValid() const { return m_valid; }
    };

    class PerspectiveCamera : public SceneCamera {
    public:
        float z_near{1.0f};
        float vertical_fov{1.0f};
        std::optional<float> z_far;
        std::optional<float> aspect_ratio;
        [[nodiscard]] Ref<SceneGraphLeaf> clone() override;
    };

    class OrthographicCamera : public SceneCamera {
    public:
        float z_near{0.0f};
        float z_far{1.0f};
        float x_mag{1.0f};
        float y_mag{1.0f};
        [[nodiscard]] Ref<SceneGraphLeaf> clone() override;
    };

    class Light : public SceneGraphLeaf {
    public:
        [[nodiscard]] SceneGraphLeafType getType() const override { return SceneGraphLeafType::Light; }
        [[nodiscard]] Ref<SceneGraphLeaf> clone() override;
    };

    class DirectionalLight : public Light {
    public:
        Color color{Color::white()};
        float irradiance{1.0f};
        float angular_size{0.0f};
        [[nodiscard]] Ref<SceneGraphLeaf> clone() override;
    };

    class SpotLight : public Light {
    public:
        Color color{Color::white()};
        float intensity{1.0f};
        float radius{0.0f};
        float range{0.0f};
        float inner_angle{180.0f};
        float outer_angle{180.0f};
        [[nodiscard]] Ref<SceneGraphLeaf> clone() override;
    };

    class PointLight : public Light {
    public:
        Color color{Color::white()};
        float intensity{1.0f};
        float radius{0.0f};
        float range{0.0f};
        [[nodiscard]] Ref<SceneGraphLeaf> clone() override;
    };

    class SceneGraphNode : public std::enable_shared_from_this<SceneGraphNode> {
        Weak<SceneGraph> m_graph;
        Weak<SceneGraphNode> m_parent;
        std::vector<Ref<SceneGraphNode>> m_children;
        Ref<SceneGraphLeaf> m_leaf;
        Uuid m_entity_uuid{};
        std::string m_name;
        Vector3f m_rotation{0.0f};
        Vector3f m_scaling{1.0f};
        Vector3f m_translation{0.0f};
    public:
        SceneGraphNode() = default;

        [[nodiscard]] SceneGraphNode* getParent() const { return m_parent.lock().get(); }
        [[nodiscard]] Ref<SceneGraphNode> getParentPtr() const { return m_parent.lock(); }
        [[nodiscard]] SceneGraphNode* getChild(const size_t index) const { return (index < m_children.size()) ? m_children[index].get() : nullptr; }
        [[nodiscard]] const std::vector<Ref<SceneGraphNode>>& getChildren() const { return m_children; }
        [[nodiscard]] size_t getChildrenCount() const { return m_children.size(); }
        [[nodiscard]] const Ref<SceneGraphLeaf>& getLeaf() const { return m_leaf; }
        [[nodiscard]] Uuid getEntityUuid() const { return m_entity_uuid; }
        [[nodiscard]] const std::string& getName() const { return m_name; }
        [[nodiscard]] Ref<SceneGraph> getGraph() const { return m_graph.lock(); }

        void setEntityUuid(Uuid entity_uuid) { m_entity_uuid = entity_uuid; }
        void setName(const std::string& name) { m_name = name; }
        void setTranslation(const Vector3f& translation) { m_translation = translation; }
        void setRotation(const Vector3f& rotation) { m_rotation = rotation; }
        void setScaling(const Vector3f& scaling) { m_scaling = scaling; }
        void setLeaf(const Ref<SceneGraphLeaf>& leaf);
        void addChild(const Ref<SceneGraphNode>& child);
        void removeChild(const Ref<SceneGraphNode>& child);
        void clearChildren();

        [[nodiscard]] const Vector3f& getTranslation() const { return m_translation; }
        [[nodiscard]] const Vector3f& getRotation() const { return m_rotation; }
        [[nodiscard]] const Vector3f& getScaling() const { return m_scaling; }
        [[nodiscard]] Matrix4f getLocalMatrix() const;
        [[nodiscard]] Matrix4f getGlobalMatrix() const;
    private:
        void setGraph(const Ref<SceneGraph>& graph);
        void setParent(const Ref<SceneGraphNode>& parent);

        friend class SceneGraph;
    };

    class SceneGraph : public std::enable_shared_from_this<SceneGraph> {
        Ref<SceneGraphNode> m_root{nullptr};
        Ref<SceneGraphNode> m_main_camera_node{nullptr};
        UnorderedMap<Uuid, Ref<SceneGraphNode>> m_entity_nodes;
        ResourceTracker<Material> m_materials;
        ResourceTracker<Mesh> m_meshes;
        Size_t m_geometry_count{0};
        DynamicArray<Ref<MeshInstance>> m_instances{};
        DynamicArray<Ref<SceneCamera>> m_cameras{};
        DynamicArray<Ref<Light>> m_lights{};
    public:
        static Ref<SceneGraph> Create();

        SceneGraph() = default;

        void reset();

        [[nodiscard]] Ref<SceneGraphNode> getRoot() const { return m_root; }
        [[nodiscard]] Ref<SceneGraphNode> getMainCameraNode() const { return m_main_camera_node; }
        [[nodiscard]] Ref<SceneCamera> getMainCamera() const;
        [[nodiscard]] bool hasNode(Uuid entity_uuid) const;
        [[nodiscard]] Ref<MeshInstance> getMeshInstance(Uuid entity_uuid) const;
        [[nodiscard]] const std::vector<Ref<MeshInstance>>& getMeshInstances() const { return m_instances; }

        Ref<SceneGraphNode> createNode(const Ref<SceneGraphNode>& parent = nullptr);
        Ref<SceneGraphNode> upsertNode(Uuid entity_uuid, const std::string& name, Uuid parent_uuid,
                                       const Vector3f& translation, const Vector3f& rotation, const Vector3f& scaling);
        Ref<SceneGraphNode> upsertPointLight(Uuid entity_uuid, const std::string& name, Uuid parent_uuid,
                            const Vector3f& translation, const Vector3f& rotation, const Vector3f& scaling,
                            const Ref<PointLight>& light);
        Ref<SceneGraphNode> upsertSpotLight(Uuid entity_uuid, const std::string& name, Uuid parent_uuid,
                           const Vector3f& translation, const Vector3f& rotation, const Vector3f& scaling,
                           const Ref<SpotLight>& light);
        void removeNode(Uuid entity_uuid);
        Ref<SceneCamera> setMainCamera(const Ref<SceneCamera>& camera);
        Ref<MeshInstance> upsertMeshInstance(Uuid entity_uuid, const Ref<Mesh>& mesh);
        void removeMeshInstance(Uuid entity_uuid);
        void rebuild();

        void registerLeaf(const Ref<SceneGraphLeaf>& leaf);
        void unregisterLeaf(const Ref<SceneGraphLeaf>& leaf);

        [[nodiscard]] const auto& getMaterials() const { return m_materials; }
        [[nodiscard]] const auto& getMeshes() const { return m_meshes; }
        [[nodiscard]] const std::vector<Ref<Light>>& getLights() const { return m_lights; }
    private:
        void ensureInitialized();
        Ref<SceneGraphNode> ensureEntityNode(Uuid entity_uuid);
        void removeNodeRecursive(Ref<SceneGraphNode> node);
        Ref<SceneGraph> self();
    };

} // dodoe
