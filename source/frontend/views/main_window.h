//=============================================================================
// Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  Header for the main window.
//=============================================================================

#ifndef RRA_VIEWS_MAIN_WINDOW_H_
#define RRA_VIEWS_MAIN_WINDOW_H_

#include <QMainWindow>
#include <QAction>
#include <QMenu>
#include <QSignalMapper>

#include "ui_main_window.h"

#include "qt_common/custom_widgets/navigation_bar.h"
#include "qt_common/custom_widgets/navigation_list_widget.h"

#include "managers/pane_manager.h"
#include "views/debug_window.h"

#ifdef _DEBUG
#define RRA_DEBUG_WINDOW 1
#else
#define RRA_DEBUG_WINDOW 0
#endif

#ifdef BETA_LICENSE
#include "views/license_dialog.h"
#endif  // BETA_LICENSE

/// @brief Support for the main window. This handles all common components of the UI, including the tab bar,
/// the sub-tab menus and the file menus.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /// @brief Constructor.
    ///
    /// @param [in] parent The window's parent.
    explicit MainWindow(QWidget* parent = nullptr);

    /// @brief Destructor.
    virtual ~MainWindow();

    /// @brief Overridden window resize event.
    ///
    /// Handle what happens when a user resizes the main window.
    ///
    /// @param [in] event The resize event object.
    virtual void resizeEvent(QResizeEvent* event) Q_DECL_OVERRIDE;

    /// @brief Overridden window move event.
    ///
    /// Handle what happens when a user moves the main window.
    ///
    /// @param [in] event The move event object.
    virtual void moveEvent(QMoveEvent* event) Q_DECL_OVERRIDE;

    /// @brief Handle what happens when X button is pressed.
    ///
    /// @param [in] event The close event.
    virtual void closeEvent(QCloseEvent* event) Q_DECL_OVERRIDE;

    /// @brief Reset any UI elements that need resetting when a new trace file is loaded.
    ///
    /// This include references to models or event indices that rely on backend data.
    void ResetUI();

public slots:
    /// @brief Called when trace file finished loading.
    void OpenTrace();

    /// @brief Close an RRA trace file.
    void CloseTrace();

    /// @brief Open an RRA trace file via the file menu.
    ///
    /// Present the user with a file selection dialog box and load the trace that
    /// the user chooses.
    void OpenTraceFromFileMenu();

    /// @brief Populate recent files menu/list.
    void SetupRecentTracesMenu();

private slots:
    /// @brief Called when selecting a recent trace from the recent traces list.
    ///
    /// @param [in] trace_file The trace file to load, including full path.
    void LoadTrace(const QString& trace_file);

    /// @brief Open RRA help file.
    ///
    /// Present the user with help regarding RRA.
    void OpenHelp();

    /// @brief Display RRA about information.
    ///
    /// Present the user with information about RRA.
    void OpenAboutPane();

    /// @brief Update the title bar based on app state.
    void UpdateTitlebar();

    /// @brief Resize UI elements when the DPI scale factor changes.
    void OnScaleFactorChanged();

    /// @brief Close the trace and display a message indicating that the graphics context has failed to initialize.
    void OnGraphicsContextFailedToInitialize(const QString& failure_message);

    /// @brief Navigate to a specific pane.
    ///
    /// @param [in] pane The pane to jump to.
    void ViewPane(int pane);

#ifdef BETA_LICENSE
    /// @brief What happens when the user agrees to the license.
    void AgreedToLicense();
#endif  // BETA_LICENSE

private:
    /// @brief Handle what happens when the tool is closed.
    void CloseTool();

    /// @brief Setup navigation bar on the top.
    void SetupTabBar();

    /// @brief Create re-usable actions for our menu bar and possibly other widgets.
    void CreateActions();

    /// @brief Fill in the menu bar with items.
    void CreateMenus();

    /// @brief Resize NavigationLists across several tabs of the MainWindow so that they have a consistent width.
    ///
    /// The widest width will either be defined by a text item in the navigation list.
    void ResizeNavigationLists();

    /// @brief Setup main/debug window sizes and locations.
    ///
    /// @param [in] loaded_settings The bool to indicate if settings should be loaded.
    void SetupWindowRects(bool loaded_settings);

    /// @brief Setup mapping for keyboard binds.
    ///
    /// @param [in] mapper The signal mapper.
    /// @param [in] key    The pressed key.
    /// @param [in] pane   The target pane.
    void SetupHotkeyNavAction(QSignalMapper* mapper, int key, int pane);

    /// @brief Handle a drag enter event.
    ///
    /// @param [in] event The drag enter event.
    void dragEnterEvent(QDragEnterEvent* event);

    /// @brief Handle a drag-n-drop event.
    ///
    /// @param [in] event The drop event.
    void dropEvent(QDropEvent* event);

    /// @brief Setup the next pane and navigate to it.
    ///
    /// @param [in] pane The pane to jump to.
    rra::RRAPaneId SetupNextPane(rra::RRAPaneId pane);

    /// @brief Construct title bar content.
    QString GetTitleBarString() const;

    /// @brief Template function to create a new pane of a certain type.
    ///
    /// @param [in] widget_stack The stacked widget to add the created pane to.
    ///
    /// @return Pointer to the newly created pane.
    template <class PaneType>
    PaneType* CreatePane(QStackedWidget* widget_stack)
    {
        PaneType* pane = new PaneType(this);
        pane_manager_.AddPane(pane);
        widget_stack->addWidget(pane);
        return pane;
    }

    Ui::MainWindow* ui_;  ///< Pointer to the Qt UI design.

#ifdef RRA_DEBUG_WINDOW
    DebugWindow debug_window_;  ///< A supplemental debug window to output custom messages.
#endif

    QMenu*   file_menu_;           ///< File menu control.
    QAction* open_trace_action_;   ///< Action to open an RRA trace.
    QAction* close_trace_action_;  ///< Action to close an RRA trace.
    QAction* exit_action_;         ///< Action to exit RRA.
    QAction* help_action_;         ///< Action to display help.
    QAction* about_action_;        ///< Action to display About Information.

    QMenu* help_menu_;  ///< Help menu control

    QMenu*                  recent_traces_menu_;    ///< Sub menu containing recently opened files.
    QVector<QSignalMapper*> recent_trace_mappers_;  ///< Map signals to the recent traces when clicked on.
    QVector<QAction*>       recent_trace_actions_;  ///< List of actions for recent traces.

    NavigationBar    navigation_bar_;  ///< The Back/Forward Navigation buttons added to the main tab bar.
    rra::PaneManager pane_manager_;    ///< The class responsible for managing the relationships between different panes.
#ifdef BETA_LICENSE
    LicenseDialog* license_dialog_;  ///< The UI licence dialog box.
#endif  // BETA_LICENSE
};

#endif  // RRA_VIEWS_MAIN_WINDOW_H_
