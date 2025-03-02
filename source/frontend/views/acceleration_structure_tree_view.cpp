//=============================================================================
/// Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  Implementation of an acceleration structure tree view.
///
/// Based on a scaled treeview, it adds a right-click context menu.
//=============================================================================

#include "acceleration_structure_tree_view.h"

#include <QMenu>
#include <QContextMenuEvent>
#include <QApplication>

AccelerationStructureTreeView::AccelerationStructureTreeView(QWidget* parent)
    : ScaledTreeView(parent)
{
}

AccelerationStructureTreeView::~AccelerationStructureTreeView()
{
}

void AccelerationStructureTreeView::SetViewerModel(rra::AccelerationStructureViewerModel* model, uint64_t current_bvh_index)
{
    model_             = model;
    current_bvh_index_ = current_bvh_index;
}

void AccelerationStructureTreeView::contextMenuEvent(QContextMenuEvent* event)
{
    if (model_ == nullptr)
    {
        return;
    }

    const QModelIndexList& list = selectedIndexes();
    if (list.size() >= 1)
    {
        const QModelIndex& model_index = list[0];
        model_->SetSelectedNodeIndex(model_index);
    }

    QMenu menu;
    menu.setStyle(QApplication::style());

    rra::SceneContextMenuRequest request = {};
    request.location                     = rra::SceneContextMenuLocation::kSceneContextMenuLocationTreeView;

    auto context_options = model_->GetSceneContextOptions(current_bvh_index_, request);

    for (const auto& option_action : context_options)
    {
        auto action = new QAction(QString::fromStdString(option_action.first));
        menu.addAction(action);
    }

    const QPoint pos    = event->globalPos();
    QAction*     action = menu.exec(pos);

    if (action != nullptr)
    {
        context_options[action->text().toStdString()]();
    }
}
