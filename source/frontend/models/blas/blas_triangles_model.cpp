//=============================================================================
// Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for the BLAS triangles model.
//=============================================================================

#include "models/blas/blas_triangles_model.h"

#include <deque>

#include <QTableView>
#include <QScrollBar>
#include <QHeaderView>
#include <QSortFilterProxyModel>

#include "qt_common/utils/qt_util.h"
#include "qt_common/utils/scaling_manager.h"

#include "models/blas/blas_triangles_item_model.h"

#include "public/rra_bvh.h"
#include "public/rra_blas.h"
#include "public/rra_tlas.h"

namespace rra
{
    BlasTrianglesModel::BlasTrianglesModel(int32_t num_model_widgets)
        : ModelViewMapper(num_model_widgets)
        , table_model_(nullptr)
        , proxy_model_(nullptr)
    {
    }

    BlasTrianglesModel::~BlasTrianglesModel()
    {
        delete table_model_;
        delete proxy_model_;
    }

    void BlasTrianglesModel::ResetModelValues()
    {
        table_model_->removeRows(0, table_model_->rowCount());
        table_model_->SetRowCount(0);
        SetModelData(kBlasTrianglesBaseAddress, "-");
        SetModelData(kTlasTrianglesBaseAddress, "-");
    }

    bool BlasTrianglesModel::UpdateTable(uint64_t tlas_index, uint64_t blas_index)
    {
        uint64_t tlas_address = 0;
        if (RraTlasGetBaseAddress(tlas_index, &tlas_address) == kRraOk)
        {
            QString address_string = "TLAS base address: 0x" + QString("%1").arg(tlas_address, 0, 16);
            SetModelData(kTlasTrianglesBaseAddress, address_string);
        }

        uint64_t blas_address = 0;
        if (RraBlasGetBaseAddress(blas_index, &blas_address) == kRraOk)
        {
            QString address_string = "BLAS base address: 0x" + QString("%1").arg(blas_address, 0, 16);
            SetModelData(kBlasTrianglesBaseAddress, address_string);
        }

        uint32_t triangle_count = 0;
        if (RraBlasGetTriangleNodeCount(blas_index, &triangle_count) != kRraOk)
        {
            return false;
        }
        table_model_->SetRowCount(triangle_count);

        uint64_t rows_added = 0;

        if (triangle_count > 0)
        {
            uint32_t root_node = UINT32_MAX;
            RRA_BUBBLE_ON_ERROR(RraBvhGetRootNodePtr(&root_node));

            std::deque<uint32_t> traversal_stack;
            traversal_stack.push_back(root_node);
            uint32_t node_index = 0;

            // Traverse the tree and add all triangle nodes to the table.
            while (!traversal_stack.empty())
            {
                BlasTrianglesStatistics stats     = {};
                auto                    node_data = traversal_stack.front();
                traversal_stack.pop_front();

                // For each item on the stack, add the children if valid.
                // Sort node types. Loop once for box (interior) nodes, then once for leaf nodes.
                uint32_t child_node_count = 0;
                RRA_BUBBLE_ON_ERROR(RraBlasGetChildNodeCount(blas_index, node_data, &child_node_count));
                std::vector<uint32_t> child_nodes(child_node_count);
                RRA_BUBBLE_ON_ERROR(RraBlasGetChildNodes(blas_index, node_data, child_nodes.data()));

                for (uint32_t child_index = 0; child_index < child_node_count; child_index++)
                {
                    uint32_t child_node = child_nodes[child_index];
                    if (RraBvhIsBoxNode(child_node))
                    {
                        // Add box nodes to the list of nodes to process.
                        traversal_stack.push_back(child_node);
                    }
                    else if (RraBvhIsTriangleNode(child_node))
                    {
                        stats.node_id = child_node;

                        // Gather triangle data.
                        if (RraBlasGetNodeBaseAddress(blas_index, child_node, &stats.triangle_address) != kRraOk)
                        {
                            continue;
                        }
                        if (RraBvhGetNodeOffset(child_node, &stats.triangle_offset) != kRraOk)
                        {
                            continue;
                        }
                        if (RraBlasGetNodeTriangleCount(blas_index, child_node, &stats.triangle_count) != kRraOk)
                        {
                            continue;
                        }
                        if (RraBlasGetGeometryIndex(blas_index, child_node, &stats.geometry_index) != kRraOk)
                        {
                            continue;
                        }
                        if (RraBlasGetIsInactive(blas_index, child_node, &stats.is_inactive) != kRraOk)
                        {
                            continue;
                        }
                        if (RraBlasGetSurfaceArea(blas_index, child_node, &stats.triangle_surface_area) != kRraOk)
                        {
                            continue;
                        }
                        if (RraBlasGetSurfaceAreaHeuristic(blas_index, child_node, &stats.sah) != kRraOk)
                        {
                            continue;
                        }

                        uint32_t                    vertex_count = (stats.triangle_count == 1 ? 3 : 4);
                        std::vector<VertexPosition> verts(vertex_count);
                        if (RraBlasGetNodeVertices(blas_index, child_node, verts.data()) != kRraOk)
                        {
                            continue;
                        }
                        stats.vertex_0 = rra::renderer::float3(verts[0].x, verts[0].y, verts[0].z);
                        stats.vertex_1 = rra::renderer::float3(verts[1].x, verts[1].y, verts[1].z);
                        stats.vertex_2 = rra::renderer::float3(verts[2].x, verts[2].y, verts[2].z);
                        if (vertex_count == 4)
                        {
                            stats.vertex_3 = rra::renderer::float3(verts[3].x, verts[3].y, verts[3].z);
                        }

                        // Add triangle nodes to the table.
                        table_model_->AddTriangleStructure(stats);
                        rows_added++;
                        node_index++;
                    }
                }
            }
        }

        Q_ASSERT(rows_added == triangle_count);
        proxy_model_->invalidate();
        return triangle_count > 0;
    }

    void BlasTrianglesModel::InitializeTableModel(ScaledTableView* table_view, uint num_rows, uint num_columns)
    {
        if (proxy_model_ != nullptr)
        {
            delete proxy_model_;
            proxy_model_ = nullptr;
        }

        proxy_model_ = new BlasTrianglesProxyModel();
        table_model_ = proxy_model_->InitializeAccelerationStructureTableModels(table_view, num_rows, num_columns);
        table_model_->Initialize(table_view);
    }

    QModelIndex BlasTrianglesModel::FindTriangleIndex(uint32_t triangle_node_id, uint64_t blas_index) const
    {
        uint64_t node_address = 0;
        if (RraBlasGetNodeBaseAddress(blas_index, triangle_node_id, &node_address) == kRraOk)
        {
            return proxy_model_->FindModelIndex(node_address, kBlasTrianglesColumnTriangleAddress);
        }
        return QModelIndex();
    }

    uint32_t BlasTrianglesModel::GetNodeId(int row) const
    {
        return proxy_model_->GetData(row, rra::kBlasTrianglesColumnVertex3);
    }

    void BlasTrianglesModel::SearchTextChanged(const QString& filter)
    {
        proxy_model_->SetSearchFilter(filter);
        proxy_model_->invalidate();
    }

    BlasTrianglesProxyModel* BlasTrianglesModel::GetProxyModel() const
    {
        return proxy_model_;
    }

}  // namespace rra
