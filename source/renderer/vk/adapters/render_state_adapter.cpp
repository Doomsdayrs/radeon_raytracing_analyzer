//=============================================================================
// Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for the Render State Adapter type.
//=============================================================================

#include "public/render_state_adapter.h"
#include "public/shared.h"
#include "public/renderer_types.h"

#include "../renderer_vulkan.h"
#include "../render_modules/mesh_render_module.h"
#include "../render_modules/bounding_volume.h"
#include "../render_modules/traversal.h"
#include "../render_modules/selection_module.h"

#include <algorithm>

namespace rra
{
    namespace renderer
    {
        static const char* kGeometryColoringModeName_BlasAverageSAH        = "Color geometry by average SAH (BLAS)";
        static const char* kGeometryColoringModeDescription_BlasAverageSAH = "Shows the average surface area heuristic of all triangles in a BLAS.";
        static const char* kGeometryColoringModeName_TriangleSAH           = "Color geometry by SAH (Triangle)";
        static const char* kGeometryColoringModeDescription_TriangleSAH    = "Shows the surface area heuristic of each triangle.";
        static const char* kGeometryColoringModeName_BlasMinSAH            = "Color geometry by minimum SAH (BLAS)";
        static const char* kGeometryColoringModeDescription_BlasMinSAH     = "Shows the minimum surface area heuristic of all triangles in a BLAS.";
        static const char* kGeometryColoringModeName_InstanceMask          = "Color geometry by mask (Instance)";
        static const char* kGeometryColoringModeDescription_InstanceMask   = "Each combination of instance mask flags is assigned a unique color.";
        static const char* kGeometryColoringModeName_Opacity               = "Color geometry by opacity (Geometry)";
        static const char* kGeometryColoringModeDescription_Opacity =
            "A color showing the presence of the opacity flag. Colors defined in Themes and colors pane under 'Opacity coloring'.";
        static const char* kGeometryColoringModeName_GeometryIndex              = "Color geometry by geometry index (Geometry)";
        static const char* kGeometryColoringModeDescription_GeometryIndex       = "A color for each geometry index within a BLAS.";
        static const char* kGeometryColoringModeName_PreferFastBuildOrTraceFlag = "Color geometry by fast build/trace flag (BLAS)";
        static const char* kGeometryColoringModeDescription_PreferFastBuildOrTraceFlag =
            "The fast build/trace flag. Colors defined in Themes and colors pane under 'Build type coloring'.";
        static const char* kGeometryColoringModeName_AllowUpdateFlag = "Color geometry by allow update flag (BLAS)";
        static const char* kGeometryColoringModeDescription_AllowUpdateFlag =
            "The allow update flag. Colors defined in Themes and colors pane under 'Build flag coloring'.";
        static const char* kGeometryColoringModeName_AllowCompactionFlag = "Color geometry by allow compaction flag (BLAS)";
        static const char* kGeometryColoringModeDescription_AllowCompactionFlag =
            "The allow compaction flag. Colors defined in Themes and colors pane under 'Build flag coloring'.";
        static const char* kGeometryColoringModeName_LowMemoryFlag = "Color geometry by low memory flag (BLAS)";
        static const char* kGeometryColoringModeDescription_LowMemoryFlag =
            "The low memory flag. Colors defined in Themes and colors pane under 'Build flag coloring'.";
        static const char* kGeometryColoringModeName_InstanceFacingCullDisable            = "Color geometry by facing cull disable flag (Instance)";
        static const char* kGeometryColoringModeDescription_InstanceFacingCullDisable     = "The instance facing cull disable flag.";
        static const char* kGeometryColoringModeName_InstanceFlipFacing                   = "Color geometry by flip facing flag (Instance)";
        static const char* kGeometryColoringModeDescription_InstanceFlipFacing            = "The instance flip facing flag.";
        static const char* kGeometryColoringModeName_InstanceForceOpaqueOrNoOpaque        = "Color geometry by force opaque / no opaque flag (Instance)";
        static const char* kGeometryColoringModeDescription_InstanceForceOpaqueOrNoOpaque = "The instance force opaque / no opaque flag.";
        static const char* kGeometryColoringModeName_TreeLevel                            = "Color geometry by tree level (Triangle)";
        static const char* kGeometryColoringModeDescription_TreeLevel         = "Tree level displays the triangle's depth within the BVH as a heatmap.";
        static const char* kGeometryColoringModeName_BlasMaxDepth             = "Color geometry by max tree depth (BLAS)";
        static const char* kGeometryColoringModeDescription_BlasMaxDepth      = "A heatmap showing which BLAS have the largest BVH depth.";
        static const char* kGeometryColoringModeName_BlasAverageDepth         = "Color geometry by average tree depth (BLAS)";
        static const char* kGeometryColoringModeDescription_BlasAverageDepth  = "A heatmap showing which BLAS have the largest average BVH depth.";
        static const char* kGeometryColoringModeName_BlasInstanceId           = "Color geometry by unique color (BLAS)";
        static const char* kGeometryColoringModeDescription_BlasInstanceId    = "A color for each unique BLAS.";
        static const char* kGeometryColoringModeName_InstanceIndex            = "Color geometry by unique color (Instance)";
        static const char* kGeometryColoringModeDescription_InstanceIndex     = "Each instance is assigned a unique color.";
        static const char* kGeometryColoringModeName_BlasInstanceCount        = "Color geometry by instance count (BLAS)";
        static const char* kGeometryColoringModeDescription_BlasInstanceCount = "A heatmap showing which BLAS are most commonly instanced.";
        static const char* kGeometryColoringModeName_BlasTriangleCount        = "Color geometry by triangle count (BLAS)";
        static const char* kGeometryColoringModeDescription_BlasTriangleCount = "A heatmap showing which BLAS contain the highest triangle count.";
        static const char* kGeometryColoringModeName_Lit                      = "Color geometry by lighting";
        static const char* kGeometryColoringModeDescription_Lit               = "Directionally lit shading.";
        static const char* kGeometryColoringModeName_Technical                = "Color geometry by technical drawing";
        static const char* kGeometryColoringModeDescription_Technical         = "Directionally lit Gooch shading.";

        static const char* kGeometryColoringModeName_AverageEPO           = "Color geometry by average EPO (Triangle)";
        static const char* kGeometryColoringModeDescription_AverageEPO    = "Shows the average endpoint overlap from the triangle up to the BLAS root.";
        static const char* kGeometryColoringModeName_MaxEPO               = "Color geometry by maximum EPO (Triangle)";
        static const char* kGeometryColoringModeDescription_MaxEPO        = "Shows the maximum endpoint overlap from the triangle up to the BLAS root.";
        static const char* kGeometryColoringModeName_TriangleIndex        = "Color geometry by leaf node triangle index (Triangle)";
        static const char* kGeometryColoringModeDescription_TriangleIndex = "The triangle index within a leaf node.";
        static const char* kGeometryColoringModeName_TriangleCount        = "Color geometry by leaf node triangle count (Triangle)";
        static const char* kGeometryColoringModeDescription_TriangleCount = "The triangle count within a leaf node.";

        // A declaration of all available coloring modes.
        static const std::vector<GeometryColoringModeInfo> kAvailableGeometryColoringModes = {
            {GeometryColoringMode::kBlasAverageSAH,
             BvhTypeFlags::TopLevel,
             kGeometryColoringModeName_BlasAverageSAH,
             kGeometryColoringModeDescription_BlasAverageSAH},
            {GeometryColoringMode::kTriangleSAH, BvhTypeFlags::All, kGeometryColoringModeName_TriangleSAH, kGeometryColoringModeDescription_TriangleSAH},
            {GeometryColoringMode::kBlasMinSAH, BvhTypeFlags::TopLevel, kGeometryColoringModeName_BlasMinSAH, kGeometryColoringModeDescription_BlasMinSAH},
            {GeometryColoringMode::kInstanceMask,
             BvhTypeFlags::TopLevel,
             kGeometryColoringModeName_InstanceMask,
             kGeometryColoringModeDescription_InstanceMask},
            {GeometryColoringMode::kOpacity, BvhTypeFlags::All, kGeometryColoringModeName_Opacity, kGeometryColoringModeDescription_Opacity},
            {GeometryColoringMode::kGeometryIndex, BvhTypeFlags::All, kGeometryColoringModeName_GeometryIndex, kGeometryColoringModeDescription_GeometryIndex},
            {GeometryColoringMode::kFastBuildOrTraceFlag,
             BvhTypeFlags::TopLevel,
             kGeometryColoringModeName_PreferFastBuildOrTraceFlag,
             kGeometryColoringModeDescription_PreferFastBuildOrTraceFlag},
            {GeometryColoringMode::kAllowUpdateFlag,
             BvhTypeFlags::TopLevel,
             kGeometryColoringModeName_AllowUpdateFlag,
             kGeometryColoringModeDescription_AllowUpdateFlag},
            {GeometryColoringMode::kAllowCompactionFlag,
             BvhTypeFlags::TopLevel,
             kGeometryColoringModeName_AllowCompactionFlag,
             kGeometryColoringModeDescription_AllowCompactionFlag},
            {GeometryColoringMode::kLowMemoryFlag,
             BvhTypeFlags::TopLevel,
             kGeometryColoringModeName_LowMemoryFlag,
             kGeometryColoringModeDescription_LowMemoryFlag},
            {GeometryColoringMode::kInstanceFacingCullDisableBit,
             BvhTypeFlags::TopLevel,
             kGeometryColoringModeName_InstanceFacingCullDisable,
             kGeometryColoringModeDescription_InstanceFacingCullDisable},
            {GeometryColoringMode::kInstanceFlipFacingBit,
             BvhTypeFlags::TopLevel,
             kGeometryColoringModeName_InstanceFlipFacing,
             kGeometryColoringModeDescription_InstanceFlipFacing},
            {GeometryColoringMode::kInstanceForceOpaqueOrNoOpaqueBits,
             BvhTypeFlags::TopLevel,
             kGeometryColoringModeName_InstanceForceOpaqueOrNoOpaque,
             kGeometryColoringModeDescription_InstanceForceOpaqueOrNoOpaque},
            {GeometryColoringMode::kTreeLevel, BvhTypeFlags::All, kGeometryColoringModeName_TreeLevel, kGeometryColoringModeDescription_TreeLevel},
            {GeometryColoringMode::kBlasMaxDepth,
             BvhTypeFlags::TopLevel,
             kGeometryColoringModeName_BlasMaxDepth,
             kGeometryColoringModeDescription_BlasMaxDepth},
            {GeometryColoringMode::kBlasAverageDepth,
             BvhTypeFlags::TopLevel,
             kGeometryColoringModeName_BlasAverageDepth,
             kGeometryColoringModeDescription_BlasAverageDepth},
            {GeometryColoringMode::kBlasInstanceId,
             BvhTypeFlags::TopLevel,
             kGeometryColoringModeName_BlasInstanceId,
             kGeometryColoringModeDescription_BlasInstanceId},
            {GeometryColoringMode::kInstanceIndex,
             BvhTypeFlags::TopLevel,
             kGeometryColoringModeName_InstanceIndex,
             kGeometryColoringModeDescription_InstanceIndex},
            {GeometryColoringMode::kBlasInstanceCount,
             BvhTypeFlags::TopLevel,
             kGeometryColoringModeName_BlasInstanceCount,
             kGeometryColoringModeDescription_BlasInstanceCount},
            {GeometryColoringMode::kBlasTriangleCount,
             BvhTypeFlags::TopLevel,
             kGeometryColoringModeName_BlasTriangleCount,
             kGeometryColoringModeDescription_BlasTriangleCount},
            {GeometryColoringMode::kLit, BvhTypeFlags::All, kGeometryColoringModeName_Lit, kGeometryColoringModeDescription_Lit},
            {GeometryColoringMode::kTechnical, BvhTypeFlags::All, kGeometryColoringModeName_Technical, kGeometryColoringModeDescription_Technical},
        };

        /// @brief Get the info index for a given coloring mode.
        ///
        /// @param [in] coloring_mode The coloring mode value.
        ///
        /// @returns The info index associated with the given coloring mode.
        int32_t GetIndexFromGeometryColoringMode(int32_t coloring_mode)
        {
            // Find the matching coloring mode in the list of available modes.
            auto mode_iter = std::find_if(kAvailableGeometryColoringModes.begin(),
                                          kAvailableGeometryColoringModes.end(),
                                          [&coloring_mode](const GeometryColoringModeInfo& x) { return static_cast<int32_t>(x.value) == coloring_mode; });

            // Return the offset to the iterator.
            return static_cast<int32_t>(mode_iter - kAvailableGeometryColoringModes.begin());
        }

        RenderStateAdapter::RenderStateAdapter(RendererInterface*          renderer,
                                               MeshRenderModule*           blas_mesh_module,
                                               BoundingVolumeRenderModule* bounding_volume_module,
                                               TraversalRenderModule*      traversal_render_module,
                                               SelectionRenderModule*      selection_render_module)
        {
            vulkan_renderer_         = static_cast<RendererVulkan*>(renderer);
            mesh_render_module_      = blas_mesh_module;
            bounding_volume_module_  = bounding_volume_module;
            traversal_render_module_ = traversal_render_module;
            selection_render_module_ = selection_render_module;

            SceneUniformBuffer& scene_ubo = vulkan_renderer_->GetSceneUbo();
            scene_ubo.wireframe_enabled   = 1;
        }

        void RenderStateAdapter::SetRenderGeometry(bool render_geometry)
        {
            mesh_render_module_->GetRenderState().render_geometry = render_geometry;
            mesh_render_module_->GetRenderState().updated         = true;

            if (render_geometry)
            {
                traversal_render_module_->Disable();
                bounding_volume_module_->SetRenderPassHint(RenderPassHint::kRenderPassHintClearNone);
            }
            else
            {
                vulkan_renderer_->GetCamera().SetNearClipScale(0.01f);
                bounding_volume_module_->SetRenderPassHint(RenderPassHint::kRenderPassHintClearDepthOnly);
            }

            auto renderer_interface = mesh_render_module_->GetRendererInterface();
            // If setting UI on trace load, render_interface will be null, so no need to mark as dirty.
            if (renderer_interface)
            {
                renderer_interface->MarkAsDirty();
            }
        }

        bool RenderStateAdapter::GetRenderWireframe() const
        {
            return mesh_render_module_->GetRenderState().render_wireframe;
        }

        void RenderStateAdapter::SetRenderWireframe(bool render_wireframe)
        {
            mesh_render_module_->GetRenderState().render_wireframe = render_wireframe;
            mesh_render_module_->GetRenderState().updated          = true;
            SceneUniformBuffer& scene_ubo                          = vulkan_renderer_->GetSceneUbo();
            scene_ubo.wireframe_enabled                            = render_wireframe ? 1 : 0;
            mesh_render_module_->GetRendererInterface()->MarkAsDirty();
        }

        bool RenderStateAdapter::GetRenderGeometry() const
        {
            return mesh_render_module_->GetRenderState().render_geometry;
        }

        void RenderStateAdapter::SetGeometryColoringMode(GeometryColoringMode coloring_mode)
        {
            mesh_render_module_->SetGeometryColoringMode(coloring_mode);
            vulkan_renderer_->MarkAsDirty();
        }

        void RenderStateAdapter::SetBVHColoringMode(int32_t coloring_mode)
        {
            SceneUniformBuffer& scene_ubo = vulkan_renderer_->GetSceneUbo();
            scene_ubo.bvh_coloring_mode   = coloring_mode;
        }

        void RenderStateAdapter::SetTraversalCounterMode(int32_t counter_mode)
        {
            SceneUniformBuffer& scene_ubo    = vulkan_renderer_->GetSceneUbo();
            scene_ubo.traversal_counter_mode = counter_mode;
        }

        void RenderStateAdapter::AdaptTraversalCounterRangeToView(std::function<void(uint32_t min, uint32_t max)> update_function)
        {
            vulkan_renderer_->MarkAsDirty();
            traversal_render_module_->QueueTraversalCounterRangeUpdate([=](uint32_t min, uint32_t max) {
                update_function(min, max);
                if (vulkan_renderer_)
                {
                    vulkan_renderer_->MarkAsDirty();
                }
            });
        }

        void RenderStateAdapter::SetTraversalCounterContinuousUpdateFunction(std::function<void(uint32_t min, uint32_t max)> update_function)
        {
            vulkan_renderer_->MarkAsDirty();

            SceneUniformBuffer& scene_ubo = vulkan_renderer_->GetSceneUbo();

            if (update_function == nullptr)
            {
                traversal_render_module_->SetTraversalCounterContinuousUpdateFunction(update_function);
                scene_ubo.traversal_counter_use_custom_min_max = 1;
                return;
            }

            scene_ubo.traversal_counter_use_custom_min_max = 0;

            traversal_render_module_->SetTraversalCounterContinuousUpdateFunction([=](uint32_t min, uint32_t max) {
                update_function(min, max);
                if (vulkan_renderer_ && traversal_counter_min_ != min && traversal_counter_max_ != max)
                {
                    traversal_counter_min_ = min;
                    traversal_counter_max_ = max;
                    vulkan_renderer_->MarkAsDirty();
                }
            });
        }

        bool RenderStateAdapter::IsTraversalCounterContinuousUpdateFunctionSet()
        {
            return traversal_render_module_->IsTraversalCounterContinuousUpdateFunctionSet();
        }

        int32_t RenderStateAdapter::GetGeometryColoringMode() const
        {
            return GetIndexFromGeometryColoringMode((int32_t)mesh_render_module_->GetGeometryColoringMode());
        }

        void RenderStateAdapter::GetAvailableGeometryColoringModes(BvhTypeFlags type, std::vector<GeometryColoringModeInfo>& coloring_modes) const
        {
            for (const auto& mode_info : kAvailableGeometryColoringModes)
            {
                if ((mode_info.viewer_availability & type) == type)
                {
                    coloring_modes.push_back(mode_info);
                }
            }
        }

        int RenderStateAdapter::GetCullingMode() const
        {
            return mesh_render_module_->GetRenderState().culling_mode;
        }

        void RenderStateAdapter::SetCullingMode(int culling_mode)
        {
            mesh_render_module_->GetRenderState().culling_mode = culling_mode;
            mesh_render_module_->GetRenderState().updated      = true;
            SceneUniformBuffer& scene_ubo                      = vulkan_renderer_->GetSceneUbo();
            scene_ubo.culling_mode                             = culling_mode;

            if (mesh_render_module_->GetRendererInterface() != nullptr)
            {
                mesh_render_module_->GetRendererInterface()->MarkAsDirty();
            }
        }

        void RenderStateAdapter::SetTraversalCounterRange(uint32_t min_value, uint32_t max_value)
        {
            vulkan_renderer_->GetSceneUbo().min_traversal_count_limit = static_cast<float>(min_value);
            vulkan_renderer_->GetSceneUbo().max_traversal_count_limit = static_cast<float>(max_value);
        }

        uint32_t RenderStateAdapter::GetTraversalCounterMin() const
        {
            return static_cast<uint32_t>(vulkan_renderer_->GetSceneUbo().min_traversal_count_limit);
        }

        uint32_t RenderStateAdapter::GetTraversalCounterMax() const
        {
            return static_cast<uint32_t>(vulkan_renderer_->GetSceneUbo().max_traversal_count_limit);
        }

        void RenderStateAdapter::SetRenderTraversal(bool render_traversal)
        {
            if (render_traversal)
            {
                traversal_render_module_->Enable();
                mesh_render_module_->Disable();

                vulkan_renderer_->GetCamera().SetNearClipScale(0.01f);
                bounding_volume_module_->SetRenderPassHint(RenderPassHint::kRenderPassHintClearDepthOnly);
            }
            else
            {
                traversal_render_module_->Disable();
                mesh_render_module_->Enable();
                mesh_render_module_->GetRenderState().updated = true;

                bounding_volume_module_->SetRenderPassHint(RenderPassHint::kRenderPassHintClearNone);
            }

            auto renderer_interface = traversal_render_module_->GetRendererInterface();
            // If setting UI on trace load, render_interface will be null, so no need to mark as dirty.
            if (renderer_interface)
            {
                renderer_interface->MarkAsDirty();
            }
        }

        bool RenderStateAdapter::GetRenderTraversal() const
        {
            return traversal_render_module_->IsEnabled();
        }

        void RenderStateAdapter::SetRenderBoundingVolumes(bool render_bounding_volumes)
        {
            if (render_bounding_volumes)
            {
                bounding_volume_module_->Enable();
                selection_render_module_->EnableOutlineRendering();
            }
            else
            {
                bounding_volume_module_->Disable();
                selection_render_module_->DisableOutlineRendering();
            }
            bounding_volume_module_->GetRendererInterface()->MarkAsDirty();
        }

        bool RenderStateAdapter::GetRenderBoundingVolumes() const
        {
            return bounding_volume_module_->IsEnabled();
        }

        void RenderStateAdapter::SetRenderInstancePretransform(bool render_instance_pretransform)
        {
            if (render_instance_pretransform)
            {
                selection_render_module_->EnableTransformRendering();
            }
            else
            {
                selection_render_module_->DisableTransformRendering();
            }
            bounding_volume_module_->GetRendererInterface()->MarkAsDirty();
        }

        void RenderStateAdapter::SetHeatmapData(HeatmapData heatmap_data)
        {
            vulkan_renderer_->SetHeatmapData(heatmap_data);
            for (auto callback : heatmap_update_callbacks_)
            {
                callback(heatmap_data);
            }
        }

        void RenderStateAdapter::AddHeatmapUpdateCallback(std::function<void(rra::renderer::HeatmapData)> heatmap_update_callback)
        {
            if (heatmap_update_callback)
            {
                heatmap_update_callbacks_.push_back(heatmap_update_callback);
            }
        }

    }  // namespace renderer
}  // namespace rra
