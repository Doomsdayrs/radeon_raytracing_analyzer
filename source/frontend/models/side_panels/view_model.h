//=============================================================================
// Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  Header for the View side pane model.
//=============================================================================

#ifndef RRA_MODELS_SIDE_PANELS_VIEW_MODEL_H_
#define RRA_MODELS_SIDE_PANELS_VIEW_MODEL_H_

#include "side_panel_model.h"
#include "public/view_state_adapter.h"
#include "io/camera_controllers.h"

namespace rra
{
    /// @brief Enum containing indices for the widgets shared between the model and UI.
    enum SidePaneViewWidgets
    {
        kSidePaneViewRenderGeometry,
        kSidePaneViewRenderBVH,
        kSidePaneViewRenderInstanceTransforms,
        kSidePaneViewWireframeOverlay,
        kSidePaneViewCullingMode,

        kSidePaneCameraRotationRow0Col0,
        kSidePaneCameraRotationRow0Col1,
        kSidePaneCameraRotationRow0Col2,
        kSidePaneCameraRotationRow1Col0,
        kSidePaneCameraRotationRow1Col1,
        kSidePaneCameraRotationRow1Col2,
        kSidePaneCameraRotationRow2Col0,
        kSidePaneCameraRotationRow2Col1,
        kSidePaneCameraRotationRow2Col2,
        kSidePaneCameraPositionX,
        kSidePaneCameraPositionY,
        kSidePaneCameraPositionZ,

        kSidePaneViewFieldOfView,
        kSidePaneViewFieldOfViewSlider,
        kSidePaneViewNearPlaneSlider,
        kSidePaneViewMovementSpeed,
        kSidePaneViewMovementSpeedSlider,

        kSidePaneViewInvertVertical,
        kSidePaneViewInvertHorizontal,

        kSidePaneViewXUp,
        kSidePaneViewYUp,
        kSidePaneViewZUp,
        kSidePaneViewGeometryMode,
        kSidePaneViewTraversalMode,

        kSidePaneViewTraversalContinuousUpdate,

        kSidePaneViewNumWidgets,
    };

    constexpr float kDefaultMovementSpeedMultiplier = 0.002f;    ///< The default movement speed is this multiple of the maximum movement speed.
    constexpr float kMinimumMovementSpeedMultiplier = 0.00005f;  ///< The minimum movement speed is this multiple of the maximum movement speed.
    const int32_t   kMovementSliderMaximum          = 1000;      ///< The number of tick marks on the slider, essentially.

    /// @brief The View model class declaration.
    class ViewModel : public SidePanelModel
    {
    public:
        /// @brief Constructor.
        explicit ViewModel();

        /// @brief Destructor.
        virtual ~ViewModel() = default;

        /// @brief Set the renderer adapter instance.
        ///
        /// @param [in] adapter The renderer adapter instance.
        virtual void SetRendererAdapters(const rra::renderer::RendererAdapterMap& adapters) override;

        /// @brief Update the model with the current BVH state.
        ///
        /// Propagate the state to the UI.
        virtual void Update() override;

        /// @brief Update the camera's rotation matrix and position in the side pane.
        void UpdateCameraTransformUI();

        /// @brief Get the culling modes supported by the view.
        ///
        /// @return A list of strings containing the culling modes.
        std::vector<std::string>& GetCullingModes() const;

        /// @brief Enable/disable whether to render the geometry.
        ///
        /// @param [in] enabled Flag indicating whether to render the geometry.
        void SetRenderGeometry(bool enabled);

        /// @brief Enable/disable whether to render the bounding volume hierarchy.
        ///
        /// @param [in] enabled Flag indicating whether to render the BVH.
        void SetRenderBVH(bool enabled);

        /// @brief Enable/disable whether to render the instance pretransform.
        ///
        /// @param [in] enabled Flag indicating whether to render the instance pretransform.
        void SetRenderInstancePretransform(bool enabled);

        /// @brief Enable/disable whether to render a wireframe overlay.
        ///
        /// @param [in] enabled Flag indicating whether to render a wireframe overlay.
        void SetWireframeOverlay(bool enabled);

        /// @brief Set the renderer culling mode.
        ///
        /// @param [in] index An index selected in the combo box corresponding to the culling mode.
        void SetCullingMode(int index);

        /// @brief Set the max traversal counter range.
        ///
        /// @param [in] min_value The minimum counter value.
        /// @param [in] max_value The maximum counter value.
        void SetTraversalCounterRange(uint32_t min_value, uint32_t max_value);

        /// @brief Enable/disable whether to render the traversal.
        ///
        /// @param [in] enabled Flag indicating whether to render the traversal.
        void SetRenderTraversal(bool enabled);

        /// @brief Adapts the counter range to the view.
        ///
        /// @param [in] update_function The callback to use when the range has been acquired.
        void AdaptTraversalCounterRangeToView(std::function<void(uint32_t min, uint32_t max)> update_function);

        /// @brief Toggles the traversal counter continuous updates.
        ///
        /// @param [in] update_function The callback to use when the range has been acquired.
        void ToggleTraversalCounterContinuousUpdate(std::function<void(uint32_t min, uint32_t max)> update_function);

        /// @brief Checks if the traversal counter continuous update is set.
        ///
        /// @returns True if the traversal counter continuous update is set.
        bool IsTraversalCounterContinuousUpdateSet();

        /// @brief Get the projection types supported by the view.
        ///
        /// @return A list of strings containing the projection types.
        std::vector<std::string>& GetProjectionTypes() const;

        /// @brief Get the control styles supported by the view.
        ///
        /// @return A list of strings containing the control style types.
        std::vector<std::string> GetControlStyles() const;

        /// @brief Get the projection modes supported by the camera controller.
        ///
        /// @return A list of strings containing the projection mode types.
        std::vector<std::string> GetProjectionModes() const;

        /// @brief Get the current camera controller index.
        ///
        /// This is so the selected camera controller is the default in the camera controller
        /// combo box in the TLAS & BLAS view panes.
        ///
        /// @return The controller index.
        int GetCurrentControllerIndex() const;

        /// @brief Get the current camera controller.
        ///
        /// @return The controller.
        ViewerIO* GetCurrentController();

        /// @param [in] hotkeys_widget A widget that contains the hotkey information.
        void UpdateControlHotkeys(QWidget* hotkeys_widget);

        /// @brief Returns the current camera controller orientation.
        ///
        /// @return The camera controller orientation;
        const ViewerIOOrientation& GetCameraControllerOrientation() const;

        /// @brief Set the field of view.
        ///
        /// @param [in] value The field of view.
        void SetFieldOfView(int value);

        /// @brief Set the near place.
        ///
        /// @param [in] value The near plane value.
        void SetNearPlane(int value);

        /// @brief Set the movement speed multiplier.
        ///
        /// @param [in] value The movement speed multiplier.
        void SetMovementSpeed(int value);

        /// @brief Set the control style.
        ///
        /// @param [in] index An index selected in the combo box corresponding to the style.
        bool SetControlStyle(int index);

        /// @brief Toggles the vertical axis inversion.
        void ToggleInvertVertical();

        /// @brief Toggles the horizontal axis inversion.
        void ToggleInvertHorizontal();

        /// @brief Set the orientation shared by BLAS and TLAS cameras.
        void SetOrientation(rra::ViewerIOOrientation orientation);

        /// @brief Set the up axis as X.
        void SetUpAxisAsX();

        /// @brief Set the up axis as Y.
        void SetUpAxisAsY();

        /// @brief Set the up axis as Z.
        void SetUpAxisAsZ();

        /// @brief Set whether or not to use orthographic projection.
        ///
        /// @param [in] ortho True if orthographic projection is desired.
        void SetOrthographic(bool ortho);

        /// @brief Set camera parameters based on common data.
        ///
        /// @param [in] controller_changed Flag indicating if the camera controller has changed.
        void SetCameraControllerParameters(bool controller_changed);

        /// @brief Set heatmap update callback.
        ///
        /// @param [in] heatmap_update_callback The callback.
        void SetHeatmapUpdateCallback(std::function<void(rra::renderer::HeatmapData)> heatmap_update_callback);

        /// @brief Set the maximum movement speed selectable from the movement speed slider.
        ///
        /// @param maximum_speed The new maximum speed.
        void SetMovementSpeedLimit(float maximum_speed);

        /// @brief Get the maximum movement speed selectable from the movement speed slider.
        ///
        /// @return The maximum speed.
        float GetMovementSpeedLimit();

    private:
        /// @brief Convert a movement speed slider value to a valid range-clamped movement speed value.
        ///
        /// @param [in] slider_value The slider value.
        ///
        /// @returns A floating point movement speed value corresponding to the selected slider value.
        float MovementSpeedValueFromSlider(int32_t slider_value);

        /// @brief Convert a movement speed value to a range-clamped slider value.
        ///
        /// @param [in] movement_speed The movement speed value.
        ///
        /// @returns A slider integer value corresponding to the selected movement speed value.
        int32_t MovementSpeedValueToSliderValue(float movement_speed);

        /// @brief Struct to describe the camera controls in the UI.
        ///
        /// This is so these parameters can be shared between all instances of the
        /// view pane, so if a UI element is changed on one side pane, then it can be
        /// updated on the other pane(s).
        struct CameraUIControls
        {
            int                      control_style_index;
            rra::ViewerIOOrientation orientation;
        };

        rra::renderer::ViewStateAdapter*                view_state_adapter_       = nullptr;  ///< The adapter used to alter the view state.
        rra::renderer::RenderStateAdapter*              render_state_adapter_     = nullptr;  ///< The adapter used to toggle mesh render states.
        CameraControllers                               camera_controllers_       = {};       ///< The camera controllers object to manage camera controllers.
        ViewerIO*                                       current_controller_       = nullptr;  ///< The camera controller currently in use.
        static CameraUIControls                         camera_controls_;                     ///< The camera controls shared by all instances of this class.
        std::function<void(rra::renderer::HeatmapData)> heatmap_update_callback_ = nullptr;   ///< The heatmap update callback.
        float                                           movement_speed_minimum_  = {};        ///< The minimum movement speed. Set from settings.
        float                                           movement_speed_maximum_  = {};        ///< The maximum movement speed. Set from settings.
    };
}  // namespace rra

#endif  // RRA_MODELS_SIDE_PANELS_VIEW_MODEL_H_
