// do@Redlive
#include "scene_graph.h"

#include "glm/gtc/matrix_transform.hpp"

namespace dodoe {

    void SceneGraphLeaf::setNode(const Ref<SceneGraphNode>& node) {
        m_node = node;
    }

    const std::string& SceneGraphLeaf::getName() const {
        if (const auto node = getNode(); node) {
            return node->getName();
        }
        static const std::string empty_string = "";
        return empty_string;
    }

    void SceneGraphLeaf::setName(const std::string& name) const {
        if (const auto node = getNode()) {
            node->setName(name);
        }
    }

    MeshInstance::MeshInstance(Ref<Mesh> mesh)
        : m_mesh(std::move(mesh)) {
    }

    Matrix4f MeshInstance::getModelMatrix() const {
        if (const auto node = getNode()) {
            return node->getGlobalMatrix();
        }
        return Matrix4f(1.0f);
    }

    Ref<SceneGraphLeaf> MeshInstance::clone() {
        auto instance = create_ref<MeshInstance>(m_mesh);
        instance->setInstanceIndex(m_instance_index);
        instance->setGeometryInstanceIndex(m_geometry_instance_index);
        return instance;
    }

    Ref<SceneGraphLeaf> PerspectiveCamera::clone() {
        auto camera = create_ref<PerspectiveCamera>();
        if (isValid()) {
            camera->setViewProjectionMatrix(getViewProjectionMatrix());
        }
        return camera;
    }

    Ref<SceneGraphLeaf> OrthographicCamera::clone() {
        auto camera = create_ref<OrthographicCamera>();
        if (isValid()) {
            camera->setViewProjectionMatrix(getViewProjectionMatrix());
        }
        return camera;
    }

    Ref<SceneGraphLeaf> Light::clone() {
        return create_ref<Light>();
    }

    Ref<SceneGraphLeaf> DirectionalLight::clone() {
        auto light = create_ref<DirectionalLight>();
        light->color = color;
        light->irradiance = irradiance;
        light->angular_size = angular_size;
        return light;
    }

    Ref<SceneGraphLeaf> SpotLight::clone() {
        auto light = create_ref<SpotLight>();
        light->color = color;
        light->intensity = intensity;
        light->radius = radius;
        light->range = range;
        light->inner_angle = inner_angle;
        light->outer_angle = outer_angle;
        return light;
    }

    Ref<SceneGraphLeaf> PointLight::clone() {
        auto light = create_ref<PointLight>();
        light->color = color;
        light->intensity = intensity;
        light->radius = radius;
        light->range = range;
        return light;
    }

    void SceneGraphNode::setLeaf(const Ref<SceneGraphLeaf>& leaf) {
        if (m_leaf) {
            if (const auto graph = getGraph()) {
                graph->unregisterLeaf(m_leaf);
            }
            m_leaf->setNode(nullptr);
        }

        m_leaf = leaf;
        if (m_leaf) {
            m_leaf->setNode(shared_from_this());
            if (const auto graph = getGraph()) {
                graph->registerLeaf(m_leaf);
            }
        }
    }

    void SceneGraphNode::addChild(const Ref<SceneGraphNode>& child) {
        if (!child || child.get() == this) {
            return;
        }
        child->setParent(shared_from_this());
        child->setGraph(getGraph());
        if (std::find(m_children.begin(), m_children.end(), child) == m_children.end()) {
            m_children.push_back(child);
        }
    }

    void SceneGraphNode::removeChild(const Ref<SceneGraphNode>& child) {
        if (!child) {
            return;
        }
        const auto it = std::find(m_children.begin(), m_children.end(), child);
        if (it != m_children.end()) {
            (*it)->setParent(nullptr);
            m_children.erase(it);
        }
    }

    void SceneGraphNode::clearChildren() {
        for (auto& child : m_children) {
            child->setParent(nullptr);
        }
        m_children.clear();
    }

    Matrix4f SceneGraphNode::getLocalMatrix() const {
        Matrix4f model(1.0f);
        model = glm::translate(model, m_translation);
        model = glm::rotate(model, glm::radians(m_rotation.x), Vector3f(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians(m_rotation.y), Vector3f(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, glm::radians(m_rotation.z), Vector3f(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, m_scaling);
        return model;
    }

    Matrix4f SceneGraphNode::getGlobalMatrix() const {
        if (const auto parent = getParent()) {
            return parent->getGlobalMatrix() * getLocalMatrix();
        }
        return getLocalMatrix();
    }

    void SceneGraphNode::setGraph(const Ref<SceneGraph>& graph) {
        m_graph = graph;
        for (auto& child : m_children) {
            child->setGraph(graph);
        }
    }

    void SceneGraphNode::setParent(const Ref<SceneGraphNode>& parent) {
        m_parent = parent;
    }

    Ref<SceneGraph> SceneGraph::Create() {
        auto graph = create_ref<SceneGraph>();
        graph->reset();
        return graph;
    }

    void SceneGraph::reset() {
        m_root = nullptr;
        m_main_camera_node = nullptr;
        m_entity_nodes.clear();
        m_materials.clear();
        m_meshes.clear();
        m_geometry_count = 0;
        m_instances.clear();
        m_cameras.clear();
        m_lights.clear();
        ensureInitialized();
    }

    Ref<SceneCamera> SceneGraph::getMainCamera() const {
        if (!m_main_camera_node || !m_main_camera_node->getLeaf()) {
            return nullptr;
        }
        return std::dynamic_pointer_cast<SceneCamera>(m_main_camera_node->getLeaf());
    }

    bool SceneGraph::hasNode(Uuid entity_uuid) const {
        const auto it = m_entity_nodes.find(entity_uuid);
        return it != m_entity_nodes.end() && static_cast<bool>(it->second);
    }

    Ref<MeshInstance> SceneGraph::getMeshInstance(Uuid entity_uuid) const {
        auto it = m_entity_nodes.find(entity_uuid);
        if (it == m_entity_nodes.end() || !it->second) {
            return nullptr;
        }
        return std::dynamic_pointer_cast<MeshInstance>(it->second->getLeaf());
    }

    Ref<SceneGraphNode> SceneGraph::createNode(const Ref<SceneGraphNode>& parent) {
        ensureInitialized();
        auto node = create_ref<SceneGraphNode>();
        node->setGraph(self());
        if (parent) {
            parent->addChild(node);
        } else {
            m_root->addChild(node);
        }
        return node;
    }

    Ref<SceneGraphNode> SceneGraph::ensureEntityNode(Uuid entity_uuid) {
        ensureInitialized();
        auto it = m_entity_nodes.find(entity_uuid);
        if (it != m_entity_nodes.end()) {
            return it->second;
        }

        auto node = createNode(m_root);
        node->setEntityUuid(entity_uuid);
        node->setName(std::to_string(static_cast<uint64_t>(entity_uuid)));
        m_entity_nodes[entity_uuid] = node;
        return node;
    }

    Ref<SceneGraphNode> SceneGraph::upsertNode(Uuid entity_uuid, const std::string& name, Uuid parent_uuid,
                                               const Vector3f& translation, const Vector3f& rotation, const Vector3f& scaling) {
        auto node = ensureEntityNode(entity_uuid);
        node->setEntityUuid(entity_uuid);
        node->setName(name);
        node->setTranslation(translation);
        node->setRotation(rotation);
        node->setScaling(scaling);

        auto parent_node = parent_uuid.isValid() ? ensureEntityNode(parent_uuid) : m_root;
        if (parent_node == node) {
            parent_node = m_root;
        }
        if (node->getParentPtr() != parent_node) {
            if (const auto current_parent = node->getParentPtr()) {
                current_parent->removeChild(node);
            }
            parent_node->addChild(node);
        }

        return node;
    }

    void SceneGraph::removeNodeRecursive(Ref<SceneGraphNode> node) {
        if (!node) {
            return;
        }

        const auto children = node->getChildren();
        for (const auto& child : children) {
            removeNodeRecursive(child);
        }

        m_entity_nodes.erase(node->getEntityUuid());
        node->setLeaf(nullptr);
        if (const auto parent = node->getParentPtr()) {
            parent->removeChild(node);
        }
    }

    void SceneGraph::removeNode(Uuid entity_uuid) {
        auto it = m_entity_nodes.find(entity_uuid);
        if (it == m_entity_nodes.end()) {
            return;
        }
        removeNodeRecursive(it->second);
    }

    Ref<SceneCamera> SceneGraph::setMainCamera(const Ref<SceneCamera>& camera) {
        ensureInitialized();
        if (!m_main_camera_node) {
            m_main_camera_node = createNode(m_root);
            m_main_camera_node->setName("MainCamera");
        }
        m_main_camera_node->setLeaf(camera);
        return std::dynamic_pointer_cast<SceneCamera>(m_main_camera_node->getLeaf());
    }

    Ref<MeshInstance> SceneGraph::upsertMeshInstance(Uuid entity_uuid, const Ref<Mesh>& mesh) {
        auto node = ensureEntityNode(entity_uuid);
        auto instance = std::dynamic_pointer_cast<MeshInstance>(node->getLeaf());
        if (!instance) {
            instance = create_ref<MeshInstance>(mesh);
            node->setLeaf(instance);
        } else if (instance->getMesh() != mesh) {
            node->setLeaf(nullptr);
            instance = create_ref<MeshInstance>(mesh);
            node->setLeaf(instance);
        }

        instance->setMesh(mesh);
        return instance;
    }

    Ref<SceneGraphNode> SceneGraph::upsertPointLight(Uuid entity_uuid, const std::string& name, Uuid parent_uuid,
                                                     const Vector3f& translation, const Vector3f& rotation, const Vector3f& scaling,
                                                     const Ref<PointLight>& light) {
        auto node = upsertNode(entity_uuid, name, parent_uuid, translation, rotation, scaling);
        node->setLeaf(light);
        return node;
    }

    Ref<SceneGraphNode> SceneGraph::upsertSpotLight(Uuid entity_uuid, const std::string& name, Uuid parent_uuid,
                                                    const Vector3f& translation, const Vector3f& rotation, const Vector3f& scaling,
                                                    const Ref<SpotLight>& light) {
        auto node = upsertNode(entity_uuid, name, parent_uuid, translation, rotation, scaling);
        node->setLeaf(light);
        return node;
    }

    void SceneGraph::removeMeshInstance(Uuid entity_uuid) {
        auto it = m_entity_nodes.find(entity_uuid);
        if (it == m_entity_nodes.end() || !it->second) {
            return;
        }

        it->second->setLeaf(nullptr);
    }

    void SceneGraph::rebuild() {
        std::sort(
            m_instances.begin(),
            m_instances.end(),
            [](const Ref<MeshInstance>& lhs, const Ref<MeshInstance>& rhs) {
                const auto* lhs_mesh = lhs ? lhs->getMesh().get() : nullptr;
                const auto* rhs_mesh = rhs ? rhs->getMesh().get() : nullptr;
                if (lhs_mesh != rhs_mesh) {
                    return lhs_mesh < rhs_mesh;
                }
                return lhs->getNode() < rhs->getNode();
            }
        );

        int instance_index = 0;
        int geometry_instance_index = 0;
        for (const auto& instance : m_instances) {
            instance->setInstanceIndex(instance_index++);
            instance->setGeometryInstanceIndex(geometry_instance_index);
            geometry_instance_index += static_cast<int>(instance->getGeometryCount());
        }
    }

    void SceneGraph::registerLeaf(const Ref<SceneGraphLeaf>& leaf) {
        if (!leaf) {
            return;
        }

        if (leaf->getType() == SceneGraphLeafType::MeshInstance) {
            auto mesh_instance = std::static_pointer_cast<MeshInstance>(leaf);
            if (const auto& mesh = mesh_instance->getMesh(); mesh) {
                if (m_meshes.addRef(mesh)) {
                    m_geometry_count += mesh->geometries.size();
                }

                for (const auto& geometry : mesh->geometries) {
                    m_materials.addRef(geometry->material);
                }
            }
            m_instances.push_back(mesh_instance);
        } else if (leaf->getType() == SceneGraphLeafType::SceneCamera) {
            m_cameras.push_back(std::static_pointer_cast<SceneCamera>(leaf));
        } else if (leaf->getType() == SceneGraphLeafType::Light) {
            m_lights.push_back(std::static_pointer_cast<Light>(leaf));
        }
    }

    void SceneGraph::unregisterLeaf(const Ref<SceneGraphLeaf>& leaf) {
        if (!leaf) {
            return;
        }

        if (leaf->getType() == SceneGraphLeafType::MeshInstance) {
            auto mesh_instance = std::static_pointer_cast<MeshInstance>(leaf);
            if (const auto& mesh = mesh_instance->getMesh(); mesh) {
                if (m_meshes.release(mesh)) {
                    m_geometry_count -= mesh->geometries.size();
                }

                for (const auto& geometry : mesh->geometries) {
                    m_materials.release(geometry->material);
                }
            }

            if (auto it = std::find(m_instances.begin(), m_instances.end(), mesh_instance); it != m_instances.end()) {
                m_instances.erase(it);
            }
        } else if (leaf->getType() == SceneGraphLeafType::SceneCamera) {
            auto camera = std::static_pointer_cast<SceneCamera>(leaf);
            if (auto it = std::find(m_cameras.begin(), m_cameras.end(), camera); it != m_cameras.end()) {
                m_cameras.erase(it);
            }
        } else if (leaf->getType() == SceneGraphLeafType::Light) {
            auto light = std::static_pointer_cast<Light>(leaf);
            if (auto it = std::find(m_lights.begin(), m_lights.end(), light); it != m_lights.end()) {
                m_lights.erase(it);
            }
        }
    }

    void SceneGraph::ensureInitialized() {
        if (m_root) {
            return;
        }

        m_root = create_ref<SceneGraphNode>();
        m_root->setName("Root");
        m_root->setGraph(self());
    }

    Ref<SceneGraph> SceneGraph::self() {
        try {
            return shared_from_this();
        } catch (const std::bad_weak_ptr&) {
            return nullptr;
        }
    }
} // dodoe
