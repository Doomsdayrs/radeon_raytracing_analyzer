//=============================================================================
// Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation of the TLAS viewer pane.
//=============================================================================

#include "views/tlas/tlas_viewer_pane.h"

#include "qt_common/utils/scaling_manager.h"

#include "constants.h"
#include "managers/message_manager.h"
#include "models/acceleration_structure_tree_view_item.h"
#include "models/acceleration_structure_viewer_model.h"
#include "models/tlas/tlas_viewer_model.h"
#include "views/widget_util.h"
#include "public/rra_api_info.h"

static const int kSplitterWidth = 300;

TlasViewerPane::TlasViewerPane(QWidget* parent)
    : AccelerationStructureViewerPane(parent)
    , ui_(new Ui::TlasViewerPane)
{
    ui_->setupUi(this);
    ui_->viewer_container_widget_->SetupUI(this);

    rra::widget_util::ApplyStandardPaneStyle(this, ui_->main_content_, ui_->main_scroll_area_);
    ui_->tlas_tree_->setIndentation(rra::kTreeViewIndent);
    ui_->tlas_tree_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui_->tlas_tree_->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui_->tlas_tree_->header()->setStretchLastSection(false);
    ui_->tlas_tree_->installEventFilter(this);

    ui_->expand_collapse_tree_->Init(QStringList({rra::text::kTextExpandTree, rra::text::kTextCollapseTree}));
    ui_->expand_collapse_tree_->setCursor(Qt::PointingHandCursor);

    int        size           = ScalingManager::Get().Scaled(kSplitterWidth);
    QList<int> splitter_sizes = {size, size};
    ui_->splitter_->setSizes(splitter_sizes);

    SetTableParams(ui_->extents_table_);
    SetTableParams(ui_->flags_table_);
    SetTableParams(ui_->transform_table_);
    SetTableParams(ui_->position_table_);
    ui_->flags_table_->horizontalHeader()->setStretchLastSection(true);

    derived_model_                    = new rra::TlasViewerModel(ui_->tlas_tree_);
    model_                            = derived_model_;
    acceleration_structure_combo_box_ = ui_->content_bvh_;

    ui_->content_blas_address_->setCursor(Qt::PointingHandCursor);
    ui_->content_parent_->setCursor(Qt::PointingHandCursor);

    // Initialize tables.
    model_->InitializeExtentsTableModel(ui_->extents_table_);
    derived_model_->InitializeFlagsTableModel(ui_->flags_table_);
    derived_model_->InitializeTransformTableModel(ui_->transform_table_);
    derived_model_->InitializePositionTableModel(ui_->position_table_);

    model_->InitializeModel(ui_->content_node_address_, rra::kTlasStatsAddress, "text");
    model_->InitializeModel(ui_->content_node_type_, rra::kTlasStatsType, "text");
    model_->InitializeModel(ui_->content_blas_address_, rra::kTlasStatsBlasAddress, "text");
    model_->InitializeModel(ui_->content_parent_, rra::kTlasStatsParent, "text");
    model_->InitializeModel(ui_->content_instance_id_, rra::kTlasStatsInstanceId, "text");
    model_->InitializeModel(ui_->content_instance_mask_, rra::kTlasStatsInstanceMask, "text");
    model_->InitializeModel(ui_->content_instance_hit_group_index_, rra::kTlasStatsInstanceHitGroupIndex, "text");

    connect(ui_->tlas_tree_, &QAbstractItemView::clicked, [=](const QModelIndex& index) { this->SelectBlasFromTree(index, false); });
    connect(ui_->tlas_tree_, &QAbstractItemView::doubleClicked, [=](const QModelIndex& index) { this->SelectBlasFromTree(index, true); });
    connect(ui_->tlas_tree_->selectionModel(), &QItemSelectionModel::selectionChanged, this, &TlasViewerPane::TreeNodeChanged);
    connect(acceleration_structure_combo_box_, &ArrowIconComboBox::SelectionChanged, this, &TlasViewerPane::UpdateSelectedTlas);
    connect(&rra::MessageManager::Get(), &rra::MessageManager::InstancesTableDoubleClicked, this, &TlasViewerPane::SetBlasInstanceSelection);
    connect(model_, &rra::AccelerationStructureViewerModel::SceneSelectionChanged, this, &TlasViewerPane::HandleSceneSelectionChanged);
    connect(ui_->expand_collapse_tree_, &ScaledCycleButton::Clicked, model_, &rra::AccelerationStructureViewerModel::ExpandCollapseTreeView);
    connect(ui_->search_box_, &TextSearchWidget::textChanged, model_, &rra::AccelerationStructureViewerModel::SearchTextChanged);
    connect(ui_->content_blas_address_, &ScaledPushButton::clicked, this, &TlasViewerPane::GotoBlasPaneFromBlasAddress);
    connect(ui_->content_parent_, &ScaledPushButton::clicked, this, &TlasViewerPane::SelectParentNode);
    connect(&rra::MessageManager::Get(), &rra::MessageManager::TlasSelected, this, &TlasViewerPane::SetTlasIndex);

    ui_->tree_depth_slider_->setCursor(Qt::PointingHandCursor);
    connect(ui_->tree_depth_slider_, &DepthSliderWidget::SpanChanged, this, &TlasViewerPane::UpdateTreeDepths);

    connect(ui_->side_panel_container_->GetViewPane(), &ViewPane::ControlStyleChanged, this, &TlasViewerPane::UpdateCameraController);
    connect(ui_->side_panel_container_->GetViewPane(), &ViewPane::RenderModeChanged, [=](bool geometry_mode) {
        ui_->viewer_container_widget_->ShowColoringMode(geometry_mode);
    });

    flag_table_delegate_ = new FlagTableItemDelegate();
    ui_->flags_table_->setItemDelegate(flag_table_delegate_);
}

TlasViewerPane::~TlasViewerPane()
{
    delete flag_table_delegate_;
}

void TlasViewerPane::showEvent(QShowEvent* event)
{
    ui_->label_bvh_->setText("TLAS:");
    AccelerationStructureViewerPane::showEvent(event);
}

void TlasViewerPane::OnTraceClose()
{
    // Only attempt to disconnect the mouse click signal if the renderer widget has already been initialized.
    if (renderer_widget_ != nullptr)
    {
        renderer_widget_->disconnect();
    }
    ui_->content_node_address_->setDisabled(true);
    ui_->content_node_address_->setToolTip("");
    ui_->search_box_->setText("");

    AccelerationStructureViewerPane::OnTraceClose();
}

void TlasViewerPane::OnTraceOpen()
{
    InitializeRendererWidget(ui_->tlas_scene_, ui_->side_panel_container_, ui_->viewer_container_widget_, rra::renderer::BvhTypeFlags::TopLevel);
    last_selected_as_id_ = 0;
    ui_->side_panel_container_->OnTraceOpen();
    AccelerationStructureViewerPane::OnTraceOpen();
    ui_->tree_depth_slider_->SetLowerValue(0);
    ui_->tree_depth_slider_->SetUpperValue(0);
    ui_->viewer_container_widget_->ShowColoringMode(true);

    if (RraApiInfoIsVulkan())
    {
        ui_->label_instance_id_->setText("Instance custom index");
        ui_->label_instance_mask_->setText("Instance mask");
        ui_->label_instance_hit_group_index_->setText("Instance shader binding table record offset");
    }
    else
    {
        ui_->label_instance_id_->setText("Instance ID");
        ui_->label_instance_mask_->setText("Instance mask");
        ui_->label_instance_hit_group_index_->setText("Instance contribution to hit group index");
    }
}

void TlasViewerPane::UpdateTreeDepths(int min_value, int max_value)
{
    rra::Scene* scene = model_->GetSceneCollectionModel()->GetSceneByIndex(last_selected_as_id_);
    if (scene)
    {
        scene->SetDepthRange(min_value, max_value);
    }

    ui_->tree_depth_start_value_->setText(QString::number(min_value));
    ui_->tree_depth_end_value_->setText(QString::number(max_value));
}

void TlasViewerPane::SelectBlasFromTree(const QModelIndex& index, const bool navigate_to_blas_pane)
{
    rra::TlasViewerModel* model = dynamic_cast<rra::TlasViewerModel*>(model_);
    RRA_ASSERT(model != nullptr);
    if (model != nullptr)
    {
        uint64_t acceleration_structure_index = model_->FindAccelerationStructureIndex(acceleration_structure_combo_box_);
        if (acceleration_structure_index != UINT64_MAX)
        {
            uint64_t blas_index = model->GetBlasIndex(acceleration_structure_index, index);
            SelectLeafNode(blas_index, navigate_to_blas_pane);

            // Emit a signal that an instance has been selected.
            uint32_t instance_index = model->GetInstanceIndex(acceleration_structure_index, index);
            emit     rra::MessageManager::Get().InstanceSelected(instance_index);
        }
    }
}

void TlasViewerPane::HandleSceneSelectionChanged()
{
    this->UpdateSceneSelection(ui_->tlas_tree_);

    rra::TlasViewerModel* model = dynamic_cast<rra::TlasViewerModel*>(model_);
    RRA_ASSERT(model != nullptr);
    if (model != nullptr)
    {
        uint64_t acceleration_structure_index = model_->FindAccelerationStructureIndex(acceleration_structure_combo_box_);
        if (acceleration_structure_index != UINT64_MAX)
        {
            auto scene = model->GetSceneCollectionModel()->GetSceneByIndex(acceleration_structure_index);

            auto selection = scene->GetMostRecentSelectedNodeId();

            uint32_t instance_index = model->GetInstanceIndexFromNode(acceleration_structure_index, selection);

            // Emit a signal to update the selection list if TLAS.
            emit rra::MessageManager::Get().InstanceSelected(instance_index);
        }
    }
}

void TlasViewerPane::SelectLeafNode(const uint64_t blas_index, const bool navigate_to_blas_pane)
{
    rra::TlasViewerModel* model = dynamic_cast<rra::TlasViewerModel*>(model_);
    RRA_ASSERT(model != nullptr);
    if (model != nullptr && model->BlasValid(blas_index))
    {
        // Emit a signal that a BLAS has been selected.
        emit rra::MessageManager::Get().BlasSelected(blas_index);

        if (navigate_to_blas_pane)
        {
            // Switch to the BLAS pane.
            emit rra::MessageManager::Get().PaneSwitchRequested(rra::kPaneIdBlasViewer);
        }
    }
}

void TlasViewerPane::TreeNodeChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    const QModelIndexList& selected_indices = selected.indexes();
    if (selected_indices.size() > 0)
    {
        const QModelIndex& model_index = selected_indices[0];
        bool               is_root     = !model_index.parent().isValid();

        // We only show the parent address if the selected node is not the root node.
        ui_->label_parent_->setVisible(!is_root);
        ui_->content_parent_->setVisible(!is_root);
    }

    AccelerationStructureViewerPane::SelectedTreeNodeChanged(selected, deselected);
}

void TlasViewerPane::UpdateWidgets(const QModelIndex& index)
{
    // Figure out which groups to show.
    // Show the common group if a valid node is selected.
    // Show the instance group if a valid instance is selected.
    bool common_valid   = false;
    bool instance_valid = false;
    if (index.isValid())
    {
        common_valid = true;
        if (model_->SelectedNodeIsLeaf())
        {
            instance_valid              = true;
            rra::TlasViewerModel* model = dynamic_cast<rra::TlasViewerModel*>(model_);
            RRA_ASSERT(model != nullptr);
            if (model != nullptr)
            {
                if (last_selected_as_id_ != UINT64_MAX)
                {
                    uint64_t blas_index = model->GetBlasIndex(last_selected_as_id_, index);
                    if (!model->BlasValid(blas_index))
                    {
                        instance_valid = false;
                        common_valid   = false;
                    }
                }
            }
        }
    }

    ui_->common_group_->setVisible(common_valid);
    ui_->instance_group_->setVisible(instance_valid);
}

void TlasViewerPane::UpdateSelectedTlas()
{
    last_selected_as_id_ = AccelerationStructureViewerPane::UpdateSelectedBvh();

    const rra::Scene* scene = model_->GetSceneCollectionModel()->GetSceneByIndex(last_selected_as_id_);
    ui_->tree_depth_slider_->SetLowerValue(scene->GetDepthRangeLowerBound());
    ui_->tree_depth_slider_->SetUpperValue(scene->GetDepthRangeUpperBound());
    ui_->tree_depth_slider_->SetUpperBound(scene->GetSceneStatistics().max_node_depth);

    ui_->tlas_tree_->SetViewerModel(model_, last_selected_as_id_);
    ui_->expand_collapse_tree_->SetCurrentItemIndex(rra::AccelerationStructureViewerModel::TreeViewExpandMode::kCollapsed);

    emit rra::MessageManager::Get().TlasSelected(last_selected_as_id_);
}

void TlasViewerPane::SetBlasInstanceSelection(uint64_t tlas_index, uint64_t blas_index, uint64_t instance_index)
{
    rra::Scene*                       scene     = model_->GetSceneCollectionModel()->GetSceneByIndex(tlas_index);
    const rra::renderer::InstanceMap& instances = scene->GetInstances();

    auto blas_instance_iter = instances.find(blas_index);
    if (blas_instance_iter != instances.end())
    {
        auto blas_instances          = blas_instance_iter->second;
        bool is_instance_index_valid = blas_instances.size() > 0 && instance_index < blas_instances.size();
        RRA_ASSERT(is_instance_index_valid);
        if (is_instance_index_valid)
        {
            // Retrieve the info for the instance that's being selected.
            const rra::renderer::Instance& instance_info = blas_instances[instance_index];

            // Move the camera to focus on the selected BLAS instance's bounding volume.
            rra::renderer::Camera& scene_camera      = renderer_interface_->GetCamera();
            rra::ViewerIO*         camera_controller = static_cast<rra::ViewerIO*>(scene_camera.GetCameraController());
            camera_controller->FocusCameraOnVolume(&renderer_interface_->GetCamera(), instance_info.bounding_volume);

            scene->SetSceneSelection(instance_info.instance_node);

            // Update the scene selection, which will select the instance in the viewport and the treeview.
            UpdateSceneSelection(ui_->tlas_tree_);
        }
    }
}

void TlasViewerPane::GotoBlasPaneFromBlasAddress(bool checked)
{
    Q_UNUSED(checked);

    QModelIndexList indexes = ui_->tlas_tree_->selectionModel()->selectedIndexes();
    if (indexes.size() > 0)
    {
        const QModelIndex& selected_index = indexes.at(0);
        SelectBlasFromTree(selected_index, true);
    }
}

void TlasViewerPane::SelectParentNode(bool checked)
{
    Q_UNUSED(checked);

    QModelIndexList indexes = ui_->tlas_tree_->selectionModel()->selectedIndexes();
    if (indexes.size() > 0)
    {
        const QModelIndex& selected_index = indexes.at(0);
        ui_->tlas_tree_->selectionModel()->reset();
        ui_->tlas_tree_->selectionModel()->setCurrentIndex(selected_index.parent(), QItemSelectionModel::Select);
    }
}

void TlasViewerPane::SetTlasIndex(uint64_t tlas_index)
{
    if (tlas_index != last_selected_as_id_)
    {
        last_selected_as_id_ = tlas_index;
    }
}
