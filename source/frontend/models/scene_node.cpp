//=============================================================================
// Copyright (c) 2021-2022 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for the SceneNode class.
//=============================================================================

#include <deque>
#include <algorithm>

#include "public/rra_blas.h"
#include "public/rra_tlas.h"
#include "scene_node.h"
#include "public/shared.h"

#include "public/intersect_min_max.h"

// We can't use std::max or glm::max since the windows macro ends up overriding the max keyword.
// So we underfine max for this file only.
#undef max
// Same for min
#undef min

namespace rra
{
    const float kVolumeEpsilon = 0.0001f;

    SceneNode::SceneNode()
    {
    }

    SceneNode::~SceneNode()
    {
        for (auto child_node : child_nodes_)
        {
            delete child_node;
        }
    }

    void SceneNode::AppendInstancesTo(renderer::InstanceMap& instances_map) const
    {
        std::deque<const SceneNode*> traversal_stack;
        traversal_stack.push_back(this);

        while (!traversal_stack.empty())
        {
            const SceneNode* node = traversal_stack.front();
            traversal_stack.pop_front();
            for (const auto& instance : node->instances_)
            {
                instances_map[instance.blas_index].push_back(instance);
            }

            for (const auto& child_node : node->child_nodes_)
            {
                traversal_stack.push_back(child_node);
            }
        }
    }

    std::array<glm::vec4, 6> GetNormalizedPlanesFromMatrix(glm::mat4 m)
    {
        std::array<glm::vec4, 6> planes = {
            glm::vec4(m[0][3] + m[0][0],
                      m[1][3] + m[1][0],
                      m[2][3] + m[2][0],
                      m[3][3] + m[3][0]),  // Left
            glm::vec4(m[0][3] - m[0][0],
                      m[1][3] - m[1][0],
                      m[2][3] - m[2][0],
                      m[3][3] - m[3][0]),  // Right
            glm::vec4(m[0][3] + m[0][1],
                      m[1][3] + m[1][1],
                      m[2][3] + m[2][1],
                      m[3][3] + m[3][1]),  // Bottom
            glm::vec4(m[0][3] - m[0][1],
                      m[1][3] - m[1][1],
                      m[2][3] - m[2][1],
                      m[3][3] - m[3][1]),  // Top
            glm::vec4(m[0][2],
                      m[1][2],
                      m[2][2],
                      m[3][2]),  // Far
            glm::vec4(m[0][3] - m[0][2],
                      m[1][3] - m[1][2],
                      m[2][3] - m[2][2],
                      m[3][3] - m[3][2]),  // Close
        };

        // Normalize by the plane normal.
        for (size_t i = 0; i < planes.size(); i++)
        {
            float magnitude = 1.0f / glm::sqrt((planes[i].x * planes[i].x) + (planes[i].y * planes[i].y) + (planes[i].z * planes[i].z));
            planes[i]       = planes[i] * magnitude;
        }

        return planes;
    }

    bool PlaneBackFaceTest(glm::vec4 plane, BoundingVolumeExtents e)
    {
        glm::vec3 min;
        glm::vec3 max;

        max.x = plane.x >= 0.0f ? e.min_x : e.max_x;
        max.y = plane.y >= 0.0f ? e.min_y : e.max_y;
        max.z = plane.z >= 0.0f ? e.min_z : e.max_z;

        min.x = plane.x >= 0.0f ? e.max_x : e.min_x;
        min.y = plane.y >= 0.0f ? e.max_y : e.min_y;
        min.z = plane.z >= 0.0f ? e.max_z : e.min_z;

        float distance = glm::dot(glm::vec3(plane), max);
        if (distance + plane.w > 0.0f)
        {
            return false;
        }

        distance = glm::dot(glm::vec3(plane), min);
        if (distance + plane.w < 0.0f)
        {
            return true;
        }

        return false;
    }

    bool BoundingVolumeExtentFovCull(BoundingVolumeExtents extents, glm::vec3 camera_position, float fov, float fov_ratio)
    {
        auto min = glm::vec3(extents.min_x, extents.min_y, extents.min_z);
        auto max = glm::vec3(extents.max_x, extents.max_y, extents.max_z);

        auto volume_position = min + (max - min) / 2.0f;
        auto distance        = glm::distance(camera_position, volume_position);

        auto diff = max - min;

        auto volume_radius = glm::max(diff.x, glm::max(diff.y, diff.z)) / 2.0f;

        auto volume_fov = glm::atan(volume_radius / distance);

        return volume_fov < (glm::radians(fov) * fov_ratio);
    }

    bool BoundingVolumeExtentsInsidePlanes(BoundingVolumeExtents extents, const std::array<glm::vec4, 6>& planes)
    {
        for (size_t i = 0; i < planes.size(); i++)
        {
            if (PlaneBackFaceTest(planes[i], extents))
            {
                return false;
            }
        }

        return true;
    }

    void SceneNode::AppendFrustumCulledInstanceMap(renderer::InstanceMap& instance_map, renderer::FrustumInfo& frustum_info) const
    {
        // Skip if marked as not visible.
        if (!visible_)
        {
            return;
        }

        // Extract the planes from the view_projection.
        auto culling_planes = GetNormalizedPlanesFromMatrix(frustum_info.camera_view_projection);

        // Cull for the child nodes.
        for (auto child_node : child_nodes_)
        {
            if (!BoundingVolumeExtentFovCull(
                    child_node->bounding_volume_, frustum_info.camera_position, frustum_info.camera_fov, frustum_info.fov_threshold_ratio) &&
                BoundingVolumeExtentsInsidePlanes(child_node->bounding_volume_, culling_planes))
            {
                child_node->AppendFrustumCulledInstanceMap(instance_map, frustum_info);
            }
        }

        // Check for instances.
        for (auto& instance : instances_)
        {
            if (!BoundingVolumeExtentFovCull(
                    instance.bounding_volume, frustum_info.camera_position, frustum_info.camera_fov, frustum_info.fov_threshold_ratio) &&
                BoundingVolumeExtentsInsidePlanes(instance.bounding_volume, culling_planes))
            {
                instance_map[instance.blas_index].push_back(instance);
            }
        }
    }

    void SceneNode::AppendInstanceMap(renderer::InstanceMap& instance_map) const
    {
        // Skip if marked as not visible.
        if (!visible_)
        {
            return;
        }

        for (auto child_node : child_nodes_)
        {
            child_node->AppendInstanceMap(instance_map);
        }

        for (auto& instance : instances_)
        {
            instance_map[instance.blas_index].push_back(instance);
        }
    }

    void SceneNode::AppendTrianglesTo(VertexList& vertex_list) const
    {
        // Skip if marked as not visible.
        if (!visible_)
        {
            return;
        }

        vertex_list.insert(vertex_list.end(), vertices_.begin(), vertices_.end());

        for (auto child_node : child_nodes_)
        {
            child_node->AppendTrianglesTo(vertex_list);
        }
    }

    SceneNode* SceneNode::ConstructFromBlasNode(uint64_t blas_index, uint32_t node_id, uint32_t depth)
    {
        SceneNode* node = new SceneNode();
        node->node_id_  = node_id;
        node->depth_    = depth;

        RraBlasGetBoundingVolumeExtents(blas_index, node_id, &node->bounding_volume_);

        if (RraBvhIsTriangleNode(node_id))
        {
            // Get the triangle nodes. If this is not a triangle the triangle count is 0.
            uint32_t triangle_count;
            RraBlasGetNodeTriangleCount(blas_index, node_id, &triangle_count);

            // Continue with processing the node if it's a triangle node with 1 or more triangles within.
            if (triangle_count > 0)
            {
                // Populate a vector of triangle vertex data.
                std::vector<TriangleVertices> triangles(triangle_count);
                RraBlasGetNodeTriangles(blas_index, node_id, triangles.data());

                // Retrieve the geometry index associated with the current triangle node.
                uint32_t geometry_index = 0;
                RraBlasGetGeometryIndex(blas_index, node_id, &geometry_index);

                uint32_t geometry_flags = 0;
                RraBlasGetGeometryFlags(blas_index, geometry_index, &geometry_flags);

                // Extract the opacity flag.
                bool is_opaque = (geometry_flags & GeometryFlags::kOpaque) == GeometryFlags::kOpaque;

                if (RraBvhIsTriangleNode(node_id))
                {
                    RraBlasGetPrimitiveIndex(blas_index, node_id, &(node->primitive_index_));
                    RraBlasGetGeometryIndex(blas_index, node_id, &(node->geometry_index_));
                }

                // Step over each triangle and extract data used to populate the vertex buffer.
                for (size_t triangle_index = 0; triangle_index < triangles.size(); triangle_index++)
                {
                    const TriangleVertices& triangle = triangles[triangle_index];

                    // Extract the vertex positions.
                    glm::vec3 p0 = glm::vec3(triangle.a.x, triangle.a.y, triangle.a.z);
                    glm::vec3 p1 = glm::vec3(triangle.b.x, triangle.b.y, triangle.b.z);
                    glm::vec3 p2 = glm::vec3(triangle.c.x, triangle.c.y, triangle.c.z);

                    // Compute the triangle normal.
                    glm::vec3 a      = p1 - p0;
                    glm::vec3 b      = p2 - p0;
                    glm::vec3 normal = glm::cross(a, b);
                    normal           = glm::normalize(normal);

                    // We can infer the z-component from x and y, but the sign is lost. So we encode the sign of z by adding
                    // kNormalSignIndicatorOffset to x if z is negative. This is then decoded in the shader.
                    glm::vec2 compact_normal = glm::vec2(normal.x, normal.y);
                    compact_normal.x         = (normal.z < 0.0f) ? compact_normal.x : compact_normal.x + kNormalSignIndicatorOffset;

                    float triangle_sah = 0.0f;
                    RraBlasGetTriangleSurfaceAreaHeuristic(blas_index, node_id, &triangle_sah);

                    float average_epo = 0.0f;
                    float max_epo     = 0.0f;
                    RRA_UNUSED(average_epo);
                    RRA_UNUSED(max_epo);

                    // We pack geometry index, depth, and opaque into one uint32_t.
                    uint32_t geometry_index_depth_opaque{};
                    geometry_index_depth_opaque |= geometry_index << 16;  // Bits 31-16 are geometry index.
                    geometry_index_depth_opaque |= depth << 1;            // Bits 15-1 are depth.
                    geometry_index_depth_opaque |= (uint32_t)is_opaque;   // Bit 0 is opaque.

                    // Triangle SAH is negative initially to indicate deselected triangles.
                    renderer::RraVertex v0 = {p0, -triangle_sah, compact_normal, geometry_index_depth_opaque, node_id};
                    renderer::RraVertex v1 = {p1, -triangle_sah, compact_normal, geometry_index_depth_opaque, node_id};
                    renderer::RraVertex v2 = {p2, -triangle_sah, compact_normal, geometry_index_depth_opaque, node_id};

                    // Add 3 new triangle vertices to the output vector.
                    node->vertices_.push_back(v0);
                    node->vertices_.push_back(v1);
                    node->vertices_.push_back(v2);
                }
            }

            return node;
        }

        uint32_t child_node_count;
        RraBlasGetChildNodeCount(blas_index, node_id, &child_node_count);

        std::vector<uint32_t> child_nodes(child_node_count);
        RraBlasGetChildNodes(blas_index, node_id, child_nodes.data());

        for (auto child_node : child_nodes)
        {
            if (child_node == node_id)
            {
                // Self refencing node will cause a stack overflow. Skip to prevent a crash.
                continue;
            }
            auto child_node_ptr     = SceneNode::ConstructFromBlasNode(blas_index, child_node, depth + 1);
            child_node_ptr->parent_ = node;
            node->child_nodes_.push_back(child_node_ptr);
        }

        return node;
    }

    SceneNode* SceneNode::ConstructFromBlas(uint32_t blas_index)
    {
        uint32_t root_node_index = UINT32_MAX;
        RraBvhGetRootNodePtr(&root_node_index);

        auto node = ConstructFromBlasNode(blas_index, root_node_index, 0);

        return node;
    }

    SceneNode* SceneNode::ConstructFromTlasBoxNode(uint64_t tlas_index, uint32_t node_id, uint32_t depth)
    {
        SceneNode* node = new SceneNode();
        node->node_id_  = node_id;
        node->depth_    = depth;

        RraTlasGetBoundingVolumeExtents(tlas_index, node_id, &node->bounding_volume_);

        if (RraBvhIsInstanceNode(node_id))
        {
            renderer::Instance instance = {};
            instance.selected           = false;
            instance.instance_node      = node_id;
            instance.depth              = depth;

            instance.transform = glm::mat4(0.0f);  // Reset the transform to prevent misalignment.

            RraTlasGetInstanceNodeTransform(tlas_index, node_id, reinterpret_cast<float*>(&instance.transform));
            instance.transform[3][3] = 1.0f;

            // Navi IP 1.1 encoding specifies that the transform is inverse, so we inverse it again to get the correct transform.
            instance.transform = glm::inverse(instance.transform);

            RraTlasGetBoundingVolumeExtents(tlas_index, node_id, &instance.bounding_volume);

            RraTlasGetBlasIndexFromInstanceNode(tlas_index, node_id, &instance.blas_index);
            RraBlasGetMaxTreeDepth(instance.blas_index, &instance.max_depth);
            RraBlasGetAvgTreeDepth(instance.blas_index, &instance.average_depth);

            uint32_t root_node = UINT32_MAX;
            RraBvhGetRootNodePtr(&root_node);

            RraBlasGetAverageSurfaceAreaHeuristic(instance.blas_index, root_node, true, &instance.average_triangle_sah);
            RraBlasGetMinimumSurfaceAreaHeuristic(instance.blas_index, root_node, true, &instance.min_triangle_sah);

            RraTlasGetInstanceIndexFromInstanceNode(tlas_index, node_id, &instance.instance_index);

            RraBlasGetBuildFlags(instance.blas_index, reinterpret_cast<VkBuildAccelerationStructureFlagBitsKHR*>(&instance.build_flags));

            RraTlasGetInstanceNodeMask(tlas_index, node_id, &instance.mask);

            RraTlasGetInstanceFlags(tlas_index, node_id, &instance.flags);

            node->instances_.push_back(instance);

            return node;
        }

        uint32_t child_node_count;
        RraTlasGetChildNodeCount(tlas_index, node_id, &child_node_count);

        std::vector<uint32_t> child_nodes(child_node_count);
        RraTlasGetChildNodes(tlas_index, node_id, child_nodes.data());

        for (auto child_node : child_nodes)
        {
            auto child_node_ptr     = SceneNode::ConstructFromTlasBoxNode(tlas_index, child_node, depth + 1);
            child_node_ptr->parent_ = node;
            node->child_nodes_.push_back(child_node_ptr);
        }

        return node;
    }

    SceneNode* SceneNode::ConstructFromTlas(uint64_t tlas_index)
    {
        uint32_t root_node_index = UINT32_MAX;
        RraBvhGetRootNodePtr(&root_node_index);
        return ConstructFromTlasBoxNode(tlas_index, root_node_index, 0);
    }

    void SceneNode::ResetSelection()
    {
        selected_ = false;

        for (auto child_node : child_nodes_)
        {
            child_node->ResetSelection();
        }

        for (auto& instance : instances_)
        {
            instance.selected = false;
        }

        for (auto& vertex : vertices_)
        {
            // Unselect.
            vertex.triangle_sah_and_selected = -std::abs(vertex.triangle_sah_and_selected);
        }
    }

    void SceneNode::ApplyNodeSelection()
    {
        if (!visible_)
        {
            return;
        }

        selected_ = true;

        for (auto child_node : child_nodes_)
        {
            child_node->ApplyNodeSelection();
        }

        for (auto& instance : instances_)
        {
            instance.selected = true;
        }

        for (auto& vertex : vertices_)
        {
            // Select.
            vertex.triangle_sah_and_selected = std::abs(vertex.triangle_sah_and_selected);
        }
    }

    BoundingVolumeExtents SceneNode::GetBoundingVolume() const
    {
        return bounding_volume_;
    }

    void SceneNode::CollectNodes(std::map<uint32_t, SceneNode*>& nodes)
    {
        nodes[node_id_] = this;
        for (auto child_node : child_nodes_)
        {
            child_node->CollectNodes(nodes);
        }
    }

    void SceneNode::Enable()
    {
        enabled_ = true;
        if (!visible_)
        {
            return;
        }
        for (auto child_node : child_nodes_)
        {
            child_node->Enable();
        }
    }

    void SceneNode::Disable()
    {
        enabled_ = false;
        for (auto child_node : child_nodes_)
        {
            child_node->Disable();
        }
    }

    void SceneNode::SetVisible(bool visible)
    {
        visible_ = visible;
        if (visible_)
        {
            for (auto child_node : child_nodes_)
            {
                child_node->Enable();
            }
        }
        else
        {
            for (auto child_node : child_nodes_)
            {
                child_node->Disable();
            }
        }
    }

    void SceneNode::SetAllChildrenAsVisible()
    {
        visible_ = true;
        enabled_ = true;
        for (auto child_node : child_nodes_)
        {
            child_node->SetAllChildrenAsVisible();
        }
    }

    bool SceneNode::IsVisible()
    {
        return visible_;
    }

    bool SceneNode::IsEnabled()
    {
        return enabled_;
    }

    bool SceneNode::IsSelected()
    {
        return selected_;
    }

    /// @brief Reduces the volume by min max on opposite corners.
    BoundingVolumeExtents ReduceVolumeExtents(BoundingVolumeExtents global_volume, BoundingVolumeExtents local_volume)
    {
        BoundingVolumeExtents reduced;
        reduced.max_x = glm::max(global_volume.max_x, local_volume.max_x);
        reduced.max_y = glm::max(global_volume.max_y, local_volume.max_y);
        reduced.max_z = glm::max(global_volume.max_z, local_volume.max_z);
        reduced.min_x = glm::min(global_volume.min_x, local_volume.min_x);
        reduced.min_y = glm::min(global_volume.min_y, local_volume.min_y);
        reduced.min_z = glm::min(global_volume.min_z, local_volume.min_z);
        return reduced;
    }

    void SceneNode::GetBoundingVolumeForSelection(BoundingVolumeExtents& volume) const
    {
        if (!visible_)
        {
            return;
        }

        for (auto child_node : child_nodes_)
        {
            child_node->GetBoundingVolumeForSelection(volume);
        }

        if (selected_)
        {
            volume = ReduceVolumeExtents(volume, bounding_volume_);
        }
    }

    void SceneNode::CastRay(glm::vec3 ray_origin, glm::vec3 ray_direction, std::vector<SceneNode*>& intersected_nodes)
    {
        if (!visible_)
        {
            return;
        }

        if (renderer::IntersectMinMax(ray_origin,
                                      ray_direction,
                                      glm::vec3(bounding_volume_.min_x, bounding_volume_.min_y, bounding_volume_.min_z),
                                      glm::vec3(bounding_volume_.max_x, bounding_volume_.max_y, bounding_volume_.max_z)))
        {
            intersected_nodes.push_back(this);
            for (auto child : child_nodes_)
            {
                child->CastRay(ray_origin, ray_direction, intersected_nodes);
            }
        }
    }

    std::vector<renderer::Instance> SceneNode::GetInstances() const
    {
        return instances_;
    }

    std::vector<SceneTriangle> SceneNode::GetTriangles() const
    {
        RRA_ASSERT(vertices_.size() % 3 == 0);
        std::vector<SceneTriangle> triangles;
        for (size_t i = 0; i < vertices_.size(); i += 3)
        {
            SceneTriangle triangle;
            triangle.a = vertices_[i];
            triangle.b = vertices_[i + 1];
            triangle.c = vertices_[i + 2];
            triangles.push_back(triangle);
        }
        return triangles;
    }

    uint32_t SceneNode::GetPrimitiveIndex() const
    {
        return primitive_index_;
    }

    uint32_t SceneNode::GetGeometryIndex() const
    {
        return geometry_index_;
    }

    uint32_t SceneNode::GetId() const
    {
        return node_id_;
    }

    void SceneNode::AppendBoundingVolumesTo(renderer::BoundingVolumeList& volume_list, uint32_t lower_bound, uint32_t upper_bound) const
    {
        if (visible_)
        {
            for (auto child : child_nodes_)
            {
                child->AppendBoundingVolumesTo(volume_list, lower_bound, upper_bound);
            }

            if (depth_ >= lower_bound && depth_ <= upper_bound)
            {
                renderer::BoundingVolumeInstance bvi;
                bvi.min = {bounding_volume_.min_x, bounding_volume_.min_y, bounding_volume_.min_z, depth_};
                bvi.max = {bounding_volume_.max_x, bounding_volume_.max_y, bounding_volume_.max_z};

                bvi.metadata = glm::vec4(-1.0f, depth_, 0.0f, 1.0f);

                if (RraBvhIsBox16Node(node_id_))
                {
                    bvi.metadata.x = 1.0f;
                }
                else if (RraBvhIsBox32Node(node_id_))
                {
                    bvi.metadata.x = 2.0f;
                }
                else if (RraBvhIsInstanceNode(node_id_))
                {
                    bvi.metadata.x = 3.0f;
                }
                else if (RraBvhIsProceduralNode(node_id_))
                {
                    bvi.metadata.x = 4.0f;
                }
                else if (RraBvhIsTriangleNode(node_id_))
                {
                    bvi.metadata.x = 5.0f;
                }

                if (selected_)
                {
                    bvi.metadata.x = 0.0f;
                }

                volume_list.push_back(bvi);
            }
        }
    }

    uint32_t SceneNode::GetDepth() const
    {
        return depth_;
    }

    std::vector<SceneNode*> SceneNode::GetPath() const
    {
        std::vector<SceneNode*> path;
        auto                    temp = parent_;
        while (temp)
        {
            path.push_back(temp);
            temp = temp->parent_;
        }
        std::reverse(path.begin(), path.end());
        return path;
    }

    SceneNode* SceneNode::GetParent() const
    {
        return parent_;
    }

    uint32_t SceneNode::AddToTraversalTree(renderer::TraversalTree& traversal_tree)
    {
        uint32_t current_index = static_cast<uint32_t>(traversal_tree.volumes.size());

        renderer::TraversalVolume traversal_volume;
        traversal_volume.min = glm::vec4(bounding_volume_.min_x, bounding_volume_.min_y, bounding_volume_.min_z, 1.0f);
        traversal_volume.max = glm::vec4(bounding_volume_.max_x, bounding_volume_.max_y, bounding_volume_.max_z, 1.0f);
        traversal_tree.volumes.push_back(traversal_volume);

        traversal_volume.parent          = 0;
        traversal_volume.index_at_parent = -1;

        if (RraBvhIsInstanceNode(node_id_))
        {
            traversal_volume.volume_type = renderer::TraversalVolumeType::kInstance;
            traversal_volume.leaf_start  = static_cast<uint32_t>(traversal_tree.instances.size());

            for (const auto& instance : instances_)
            {
                renderer::TraversalInstance ci;
                ci.transform         = instance.transform;
                ci.inverse_transform = glm::inverse(instance.transform);
                ci.selected          = IsSelected() ? 1 : 0;
                ci.blas_id           = static_cast<uint32_t>(instance.blas_index);
                ci.geometry_index    = 0;
                ci.flags             = instance.flags;

                traversal_tree.instances.push_back(ci);
            }

            traversal_volume.leaf_end = static_cast<uint32_t>(traversal_tree.instances.size());
        }
        else if (RraBvhIsTriangleNode(node_id_))
        {
            traversal_volume.volume_type = renderer::TraversalVolumeType::kTriangle;
            traversal_volume.leaf_start  = static_cast<uint32_t>(traversal_tree.vertices.size());
            traversal_tree.vertices.insert(traversal_tree.vertices.end(), vertices_.begin(), vertices_.end());
            traversal_volume.leaf_end = static_cast<uint32_t>(traversal_tree.vertices.size());
        }
        else if (RraBvhIsBoxNode(node_id_))
        {
            traversal_volume.volume_type = renderer::TraversalVolumeType::kBox;

            // Separate for loops needed to preserve alignment.

            RRA_ASSERT(child_nodes_.size() <= 4);

            uint32_t child_index = 0;
            for (auto child : child_nodes_)
            {
                uint32_t child_addr = child->AddToTraversalTree(traversal_tree);

                if (child->IsEnabled() && child->IsVisible())
                {
                    traversal_volume.child_mask = traversal_volume.child_mask | (0x1 << child_index);
                }

                auto child_bounds = child->GetBoundingVolume();

                traversal_volume.child_nodes[child_index]          = child_addr;
                traversal_volume.child_nodes_min[child_index]      = {child_bounds.min_x, child_bounds.min_y, child_bounds.min_z, 0.0f};
                traversal_volume.child_nodes_max[child_index]      = {child_bounds.max_x, child_bounds.max_y, child_bounds.max_z, 0.0f};
                traversal_tree.volumes[child_addr].parent          = current_index;
                traversal_tree.volumes[child_addr].index_at_parent = child_index;

                child_index++;
            }
        }

        traversal_tree.volumes[current_index] = traversal_volume;
        return current_index;
    }
}  // namespace rra
