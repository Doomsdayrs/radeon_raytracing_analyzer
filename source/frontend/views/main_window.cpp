//=============================================================================
// Copyright (c) 2021-2022 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation of the main window.
//=============================================================================

#include "views/main_window.h"

#include <QDesktopWidget>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMimeData>
#include <QScreen>

#include "qt_common/utils/common_definitions.h"
#include "qt_common/utils/qt_util.h"
#include "qt_common/utils/scaling_manager.h"

#include "public/rra_trace_loader.h"

#include "managers/load_animation_manager.h"
#include "managers/message_manager.h"
#include "managers/navigation_manager.h"
#include "managers/pane_manager.h"
#include "managers/trace_manager.h"
#include "views/overview/device_configuration_pane.h"
#include "views/overview/summary_pane.h"
#include "views/settings/settings_pane.h"
#include "views/settings/themes_and_colors_pane.h"
#include "views/settings/keyboard_shortcuts_pane.h"
#include "views/start/about_pane.h"
#include "views/start/recent_traces_pane.h"
#include "views/start/welcome_pane.h"
#include "settings/settings.h"
#include "settings/geometry_settings.h"
#include "util/rra_util.h"
#include "views/widget_util.h"

#include "constants.h"
#include "version.h"

// The maximum number of traces to list in the recent traces list.
static const int kMaxSubmenuTraces = 10;

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui_(new Ui::MainWindow)
    , file_menu_(nullptr)
    , open_trace_action_(nullptr)
    , close_trace_action_(nullptr)
    , exit_action_(nullptr)
    , help_action_(nullptr)
    , about_action_(nullptr)
    , help_menu_(nullptr)
    , recent_traces_menu_(nullptr)
    , navigation_bar_(this)
{
    const bool loaded_settings = rra::Settings::Get().LoadSettings();

#ifdef BETA_LICENSE
    // Set up the license dialog box.
    license_dialog_ = new LicenseDialog();
#endif  // BETA_LICENSE

    ui_->setupUi(this);

    setWindowTitle(GetTitleBarString());
    setWindowIcon(QIcon(":/Resources/assets/icon_32x32.png"));
    setAcceptDrops(true);

    // Set white background for this pane.
    rra::widget_util::SetWidgetBackgroundColor(this, Qt::white);

    // Setup window sizes and settings.
    SetupWindowRects(loaded_settings);

    // NOTE: Widgets need creating in the order they are to appear in the UI.
    CreatePane<WelcomePane>(ui_->start_stack_);
    RecentTracesPane* recent_traces_pane = CreatePane<RecentTracesPane>(ui_->start_stack_);
    CreatePane<AboutPane>(ui_->start_stack_);
    CreatePane<SummaryPane>(ui_->overview_stack_);
    CreatePane<DeviceConfigurationPane>(ui_->overview_stack_);
    CreatePane<SettingsPane>(ui_->settings_stack_);
    CreatePane<ThemesAndColorsPane>(ui_->settings_stack_);
    CreatePane<KeyboardShortcutsPane>(ui_->settings_stack_);

    // Add any panes created in the Qt .ui files to the pane manager, so they get updated.
    pane_manager_.AddPane(ui_->tlas_viewer_tab_);
    pane_manager_.AddPane(ui_->tlas_instances_tab_);
    pane_manager_.AddPane(ui_->tlas_blas_list_tab_);
    pane_manager_.AddPane(ui_->tlas_properties_tab_);
    pane_manager_.AddPane(ui_->blas_viewer_tab_);
    pane_manager_.AddPane(ui_->blas_instances_tab_);
    pane_manager_.AddPane(ui_->blas_triangles_tab_);
    pane_manager_.AddPane(ui_->blas_properties_tab_);

    ui_->main_tab_widget_->setTabEnabled(rra::kMainPaneOverview, false);
    ui_->main_tab_widget_->setTabEnabled(rra::kMainPaneTlas, false);
    ui_->main_tab_widget_->setTabEnabled(rra::kMainPaneBlas, false);

    SetupTabBar();
    CreateActions();
    CreateMenus();
    rra::LoadAnimationManager::Get().Initialize(ui_->main_tab_widget_, file_menu_);

    ResetUI();

    ViewPane(rra::kPaneIdStartWelcome);

    connect(&navigation_bar_.BackButton(), &QPushButton::clicked, &rra::NavigationManager::Get(), &rra::NavigationManager::NavigateBack);
    connect(&navigation_bar_.ForwardButton(), &QPushButton::clicked, &rra::NavigationManager::Get(), &rra::NavigationManager::NavigateForward);
    connect(&rra::NavigationManager::Get(), &rra::NavigationManager::EnableBackNavButton, &navigation_bar_, &NavigationBar::EnableBackButton);
    connect(&rra::NavigationManager::Get(), &rra::NavigationManager::EnableForwardNavButton, &navigation_bar_, &NavigationBar::EnableForwardButton);

    // Update the navigation and pane switching when the UI panes are changed.
    connect(ui_->start_list_, &QListWidget::currentRowChanged, &pane_manager_, &rra::PaneManager::UpdateStartListRow);
    connect(ui_->overview_list_, &QListWidget::currentRowChanged, &pane_manager_, &rra::PaneManager::UpdateOverviewListRow);
    connect(ui_->tlas_sub_tab_, &QTabWidget::currentChanged, &pane_manager_, &rra::PaneManager::UpdateTlasListIndex);
    connect(ui_->blas_sub_tab_, &QTabWidget::currentChanged, &pane_manager_, &rra::PaneManager::UpdateBlasListIndex);
    connect(ui_->settings_list_, &QListWidget::currentRowChanged, &pane_manager_, &rra::PaneManager::UpdateSettingsListRow);
    connect(ui_->main_tab_widget_, &QTabWidget::currentChanged, &pane_manager_, &rra::PaneManager::UpdateMainTabIndex);

    // Set up the stack widget signals so they know which pane in their stack to switch to.
    connect(ui_->start_list_, &QListWidget::currentRowChanged, ui_->start_stack_, &QStackedWidget::setCurrentIndex);
    connect(ui_->overview_list_, &QListWidget::currentRowChanged, ui_->overview_stack_, &QStackedWidget::setCurrentIndex);
    connect(ui_->settings_list_, &QListWidget::currentRowChanged, ui_->settings_stack_, &QStackedWidget::setCurrentIndex);

    connect(&rra::MessageManager::Get(), &rra::MessageManager::OpenTraceFileMenuClicked, this, &MainWindow::OpenTraceFromFileMenu);
    connect(recent_traces_pane, &RecentTracesPane::RecentFileDeleted, this, &MainWindow::SetupRecentTracesMenu);
    connect(&rra::TraceManager::Get(), &rra::TraceManager::TraceFileRemoved, this, &MainWindow::SetupRecentTracesMenu);

    connect(&rra::MessageManager::Get(), &rra::MessageManager::PaneSwitchRequested, this, &MainWindow::ViewPane);
    connect(&rra::NavigationManager::Get(), &rra::NavigationManager::NavigateButtonClicked, this, &MainWindow::SetupNextPane);

    connect(&rra::TraceManager::Get(), &rra::TraceManager::TraceOpened, this, &MainWindow::OpenTrace);
    connect(&rra::TraceManager::Get(), &rra::TraceManager::TraceClosed, this, &MainWindow::CloseTrace);

    // Connect to ScalingManager for notifications.
    connect(&ScalingManager::Get(), &ScalingManager::ScaleFactorChanged, this, &MainWindow::OnScaleFactorChanged);

    connect(&rra::MessageManager::Get(), &rra::MessageManager::GraphicsContextFailedToInitialize, this, &MainWindow::OnGraphicsContextFailedToInitialize);

#ifdef BETA_LICENSE
    // Setup license dialog signal/slots.
    connect(license_dialog_, &LicenseDialog::AgreeToLicense, this, &MainWindow::AgreedToLicense);
#endif  // BETA_LICENSE
}

MainWindow::~MainWindow()
{
    disconnect(&ScalingManager::Get(), &ScalingManager::ScaleFactorChanged, this, &MainWindow::OnScaleFactorChanged);

#ifdef BETA_LICENSE
    delete license_dialog_;
#endif  // BETA_LICENSE
    delete ui_;
}

void MainWindow::OnScaleFactorChanged()
{
    ResizeNavigationLists();
}

void MainWindow::OnGraphicsContextFailedToInitialize(const QString& failure_message)
{
    // Close the trace, which will trigger the UI to reset back to the welcome pane.
    emit rra::TraceManager::Get().TraceClosed();

    // Display an error message box.
    QtCommon::QtUtils::ShowMessageBox(
        centralWidget(), QMessageBox::StandardButton::Ok, QMessageBox::Icon::Critical, rra::text::kRendererInitializationFailedTitle, failure_message);
}

void MainWindow::ResizeNavigationLists()
{
    // Update the widths of the navigation lists so they are all the same width.
    // Since these are each independent widgets, the MainWindow needs to keep them in sync.
    int widest_width = std::max<int>(ui_->start_list_->sizeHint().width(), ui_->settings_list_->sizeHint().width());

    // Also use 1/12th of the MainWindow as a minimum width for the navigation list.
    int minimum_width = this->width() / 12;
    widest_width      = std::max<int>(widest_width, minimum_width);

    ui_->start_list_->setFixedWidth(widest_width);
    ui_->settings_list_->setFixedWidth(widest_width);
}

void MainWindow::SetupTabBar()
{
    // Set grey background for main tab bar background.
    rra::widget_util::SetWidgetBackgroundColor(ui_->main_tab_widget_, QColor(51, 51, 51));

    // Set the mouse cursor to pointing hand cursor for all of the tabs.
    QList<QTabBar*> tab_bar = ui_->main_tab_widget_->findChildren<QTabBar*>();
    for (QTabBar* item : tab_bar)
    {
        if (item != nullptr)
        {
            item->setCursor(Qt::PointingHandCursor);
            item->setContentsMargins(10, 10, 10, 10);
        }
    }

    // Set the tab that divides the left and right justified tabs.
    ui_->main_tab_widget_->SetSpacerIndex(rra::kMainPaneSpacer);

    // Adjust spacing around the navigation bar so that it appears centered on the tab bar.
    navigation_bar_.layout()->setContentsMargins(15, 3, 35, 2);

    // Setup navigation browser toolbar on the main tab bar.
    ui_->main_tab_widget_->SetTabTool(rra::kMainPaneNavigation, &navigation_bar_);
}

#ifdef BETA_LICENSE
void MainWindow::AgreedToLicense()
{
    rra::Settings::Get().SetLicenseAgreementVersion(PRODUCT_VERSION_STRING);
}
#endif  // BETA_LICENSE

void MainWindow::SetupWindowRects(bool loaded_settings)
{
    QList<QScreen*> screens = QGuiApplication::screens();
    QRect           geometry;

    Q_ASSERT(!screens.empty());

    if (!screens.empty())
    {
        geometry = screens.front()->availableGeometry();
    }

    if (loaded_settings)
    {
        rra::GeometrySettings::Restore(this);
    }
    else
    {
        // Move main window to default position if settings file was not loaded.
        const QPoint available_desktop_top_left = geometry.topLeft();
        move(available_desktop_top_left.x() + rra::kDesktopMargin, available_desktop_top_left.y() + rra::kDesktopMargin);

        const int width  = geometry.width() * 0.66f;
        const int height = geometry.height() * 0.66f;
        resize(width, height);
    }

#ifdef BETA_LICENSE
    if (license_dialog_->AgreedToLicense() == false)
    {
        const QRect&  main_window_geometry = geometry;
        const QPoint& license_window_size  = QPoint(license_dialog_->rect().right(), license_dialog_->rect().bottom());
        const QPoint& license_window_pos   = QPoint(main_window_geometry.x() + (main_window_geometry.width() / 2) - (license_window_size.x() / 2),
                                                  main_window_geometry.y() + (main_window_geometry.height() / 2) - (license_window_size.y() / 2));

        license_dialog_->move(license_window_pos);
        license_dialog_->show();
    }
#endif  // BETA_LICENSE

#if RRA_DEBUG_WINDOW
    const int desktop_width  = (rra::kDesktopAvailableWidthPercentage / 100.0f) * geometry.width();
    const int desktop_height = (rra::kDesktopAvailableHeightPercentage / 100.0f) * geometry.height();

    const int dbg_width  = desktop_width * (rra::kDebugWindowDesktopWidthPercentage / 100.0f);
    const int dbg_height = desktop_height * (rra::kDebugWindowDesktopHeightPercentage / 100.f);

    const int dbg_loc_x = rra::kDesktopMargin + geometry.left();
    const int dbg_loc_y = (desktop_height - dbg_height - rra::kDesktopMargin) + geometry.top();

    // Place debug out window.
    debug_window_.move(dbg_loc_x, dbg_loc_y);
    debug_window_.resize(dbg_width, dbg_height);
    debug_window_.show();

#endif  // RRA_DEBUG_WINDOW
}

void MainWindow::SetupHotkeyNavAction(QSignalMapper* mapper, int key, int pane)
{
    QAction* action = new QAction(this);
    action->setShortcut(key | Qt::ALT);

    connect(action, SIGNAL(triggered()), mapper, SLOT(map()));
    this->addAction(action);
    mapper->setMapping(action, pane);
}

void MainWindow::CreateActions()
{
    QSignalMapper* signal_mapper = new QSignalMapper(this);
    SetupHotkeyNavAction(signal_mapper, rra::kGotoOverviewSummaryPane, rra::kPaneIdOverviewSummary);
    SetupHotkeyNavAction(signal_mapper, rra::kGotoOverviewDeviceConfigPane, rra::kPaneIdOverviewDeviceConfig);

    SetupHotkeyNavAction(signal_mapper, rra::kGotoTlasViewerPane, rra::kPaneIdTlasViewer);
    SetupHotkeyNavAction(signal_mapper, rra::kGotoTlasInstancesPane, rra::kPaneIdTlasInstances);
    SetupHotkeyNavAction(signal_mapper, rra::kGotoTlasBlasListPane, rra::kPaneIdTlasBlasList);
    SetupHotkeyNavAction(signal_mapper, rra::kGotoTlasPropertiesPane, rra::kPaneIdTlasProperties);
    SetupHotkeyNavAction(signal_mapper, rra::kGotoBlasViewerPane, rra::kPaneIdBlasViewer);
    SetupHotkeyNavAction(signal_mapper, rra::kGotoBlasInstancesPane, rra::kPaneIdBlasInstances);
    SetupHotkeyNavAction(signal_mapper, rra::kGotoBlasTrianglesPane, rra::kPaneIdBlasTriangles);
    SetupHotkeyNavAction(signal_mapper, rra::kGotoBlasPropertiesPane, rra::kPaneIdBlasProperties);
    SetupHotkeyNavAction(signal_mapper, rra::kGotoWelcomePane, rra::kPaneIdStartWelcome);
    SetupHotkeyNavAction(signal_mapper, rra::kGotoRecentTracesPane, rra::kPaneIdStartRecentTraces);
    SetupHotkeyNavAction(signal_mapper, rra::kGotoAboutPane, rra::kPaneIdStartAbout);
    SetupHotkeyNavAction(signal_mapper, rra::kGotoGeneralSettingsPane, rra::kPaneIdSettingsGeneral);
    SetupHotkeyNavAction(signal_mapper, rra::kGotoThemesAndColorsPane, rra::kPaneIdSettingsThemesAndColors);
    SetupHotkeyNavAction(signal_mapper, rra::kGotoKeyboardShortcutsPane, rra::kPaneIdSettingsKeyboardShortcuts);

    connect(signal_mapper, SIGNAL(mapped(int)), this, SLOT(ViewPane(int)));

    // Set up forward/backward navigation.
    QAction* shortcut = new QAction(this);
    shortcut->setShortcut(Qt::ALT | rra::kKeyNavForwardArrow);

    connect(shortcut, &QAction::triggered, &rra::NavigationManager::Get(), &rra::NavigationManager::NavigateForward);
    this->addAction(shortcut);

    shortcut = new QAction(this);
    shortcut->setShortcut(Qt::ALT | rra::kKeyNavBackwardArrow);

    connect(shortcut, &QAction::triggered, &rra::NavigationManager::Get(), &rra::NavigationManager::NavigateBack);
    this->addAction(shortcut);

    shortcut = new QAction(this);
    shortcut->setShortcut(rra::kKeyNavBackwardBackspace);

    connect(shortcut, &QAction::triggered, &rra::NavigationManager::Get(), &rra::NavigationManager::NavigateBack);
    this->addAction(shortcut);

    open_trace_action_ = new QAction(tr("Open trace"), this);
    open_trace_action_->setShortcut(Qt::CTRL | Qt::Key_O);
    connect(open_trace_action_, &QAction::triggered, this, &MainWindow::OpenTraceFromFileMenu);

    close_trace_action_ = new QAction(tr("Close trace"), this);
    close_trace_action_->setShortcut(Qt::CTRL | Qt::Key_F4);
    connect(close_trace_action_, &QAction::triggered, this, &MainWindow::CloseTrace);
    close_trace_action_->setDisabled(true);

    exit_action_ = new QAction(tr("Exit"), this);
    exit_action_->setShortcut(Qt::ALT | Qt::Key_F4);
    connect(exit_action_, &QAction::triggered, this, &MainWindow::CloseTool);

    for (int i = 0; i < kMaxSubmenuTraces; i++)
    {
        recent_trace_actions_.push_back(new QAction("", this));
        recent_trace_mappers_.push_back(new QSignalMapper());
    }

    help_action_ = new QAction(tr("Help"), this);
    connect(help_action_, &QAction::triggered, this, &MainWindow::OpenHelp);

    about_action_ = new QAction(tr("About Radeon Raytracing Analyzer"), this);
    connect(about_action_, &QAction::triggered, this, &MainWindow::OpenAboutPane);
}

void MainWindow::SetupRecentTracesMenu()
{
    for (int i = 0; i < recent_trace_mappers_.size(); i++)
    {
        disconnect(recent_trace_actions_[i], SIGNAL(triggered(bool)), recent_trace_mappers_[i], SLOT(map()));
        disconnect(recent_trace_mappers_[i], SIGNAL(mapped(QString)), this, SLOT(LoadTrace(QString)));
    }

    recent_traces_menu_->clear();

    const QVector<RecentFileData>& files = rra::Settings::Get().RecentFiles();

    const int num_items = (files.size() > kMaxSubmenuTraces) ? kMaxSubmenuTraces : files.size();

    for (int i = 0; i < num_items; i++)
    {
        recent_trace_actions_[i]->setText(files[i].path);

        recent_traces_menu_->addAction(recent_trace_actions_[i]);

        connect(recent_trace_actions_[i], SIGNAL(triggered(bool)), recent_trace_mappers_[i], SLOT(map()));

        recent_trace_mappers_[i]->setMapping(recent_trace_actions_[i], files[i].path);

        connect(recent_trace_mappers_[i], SIGNAL(mapped(QString)), this, SLOT(LoadTrace(QString)));
    }

    emit rra::MessageManager::Get().RecentFileListChanged();
}

void MainWindow::CreateMenus()
{
    file_menu_          = menuBar()->addMenu(tr("File"));
    recent_traces_menu_ = new QMenu("Recent traces");

    file_menu_->addAction(open_trace_action_);
    file_menu_->addAction(close_trace_action_);
    file_menu_->addSeparator();
    file_menu_->addMenu(recent_traces_menu_);
    file_menu_->addSeparator();
    file_menu_->addAction(exit_action_);

    file_menu_ = menuBar()->addMenu(tr("Help"));
    file_menu_->addAction(help_action_);
    file_menu_->addAction(about_action_);

    SetupRecentTracesMenu();
}

void MainWindow::LoadTrace(const QString& trace_file)
{
    rra::TraceManager::Get().LoadTrace(trace_file);
}

void MainWindow::OpenTrace()
{
    close_trace_action_->setDisabled(false);

    ui_->main_tab_widget_->setTabEnabled(rra::kMainPaneOverview, true);
    ui_->main_tab_widget_->setTabEnabled(rra::kMainPaneTlas, true);
    ui_->main_tab_widget_->setTabEnabled(rra::kMainPaneBlas, true);

    pane_manager_.OnTraceOpen();

    ViewPane(rra::kPaneIdOverviewSummary);

    SetupRecentTracesMenu();

    UpdateTitlebar();
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    rra::Settings::Get().SetWindowSize(event->size().width(), event->size().height());

    ResizeNavigationLists();

    rra::LoadAnimationManager::Get().ResizeAnimation();
}

void MainWindow::moveEvent(QMoveEvent* event)
{
    QMainWindow::moveEvent(event);

    rra::Settings::Get().SetWindowPos(geometry().x(), geometry().y());
}

void MainWindow::OpenTraceFromFileMenu()
{
    QString file_name = QFileDialog::getOpenFileName(this, "Open file", rra::Settings::Get().GetLastFileOpenLocation(), rra::text::kFileOpenFileTypes);

    if (!file_name.isNull())
    {
        rra::TraceManager::Get().LoadTrace(file_name);
    }
}

void MainWindow::CloseTrace()
{
    pane_manager_.OnTraceClose();

    ResetUI();
    rra::TraceManager::Get().ClearTrace();

    rra::NavigationManager::Get().Reset();
    close_trace_action_->setDisabled(true);
    UpdateTitlebar();
    setWindowTitle(GetTitleBarString());
}

void MainWindow::ResetUI()
{
    // Default to first tab.
    const rra::NavLocation& nav_location = pane_manager_.ResetNavigation();

    ui_->main_tab_widget_->setCurrentIndex(nav_location.main_tab_index);
    ui_->start_list_->setCurrentRow(nav_location.start_list_row);
    ui_->overview_list_->setCurrentRow(nav_location.overview_list_row);
    ui_->tlas_sub_tab_->setCurrentIndex(nav_location.tlas_list_row);
    ui_->blas_sub_tab_->setCurrentIndex(nav_location.blas_list_row);
    ui_->settings_list_->setCurrentRow(nav_location.settings_list_row);

    ui_->main_tab_widget_->setTabEnabled(rra::kMainPaneOverview, false);
    ui_->main_tab_widget_->setTabEnabled(rra::kMainPaneTlas, false);
    ui_->main_tab_widget_->setTabEnabled(rra::kMainPaneBlas, false);

    UpdateTitlebar();
    pane_manager_.Reset();
}

void MainWindow::OpenHelp()
{
    // Get the file info.
    QFileInfo file_info(QCoreApplication::applicationDirPath() + rra::text::kHelpFile);

    // Check to see if the file is not a directory and that it exists.
    if (file_info.isFile() && file_info.exists())
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + rra::text::kHelpFile));
    }
    else
    {
        // The selected help file is missing on the disk so display a message box stating so.
        const QString text = rra::text::kMissingHelpFile + QCoreApplication::applicationDirPath() + rra::text::kHelpFile;
        QtCommon::QtUtils::ShowMessageBox(this, QMessageBox::Ok, QMessageBox::Critical, rra::text::kMissingHelpFile, text);
    }
}

void MainWindow::OpenAboutPane()
{
    ViewPane(rra::kPaneIdStartAbout);
}

void MainWindow::CloseTool()
{
    CloseTrace();
#if RRA_DEBUG_WINDOW
    debug_window_.close();
#endif  // RRA_DEBUG_WINDOW

#ifdef BETA_LICENSE
    if (license_dialog_ != nullptr)
    {
        license_dialog_->close();
    }
#endif  // BETA_LICENSE

    close();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    Q_UNUSED(event);

    rra::GeometrySettings::Save(this);

    CloseTool();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event != nullptr)
    {
        if (event->mimeData()->hasUrls())
        {
            const uint32_t num_urls = event->mimeData()->urls().size();

            for (uint32_t i = 0; i < num_urls; i++)
            {
                const QString potential_trace_path = event->mimeData()->urls().at(i).toLocalFile();

                // Check if the file is valid while dragging it into the window to show the symbol of interdiction for invalid files.
                if (rra_util::TraceValidToLoad(potential_trace_path) == true)
                {
                    event->setDropAction(Qt::LinkAction);
                    event->accept();
                }
            }
        }
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    if (event != nullptr)
    {
        const uint32_t num_urls = event->mimeData()->urls().size();

        for (uint32_t i = 0; i < num_urls; i++)
        {
            const QString potential_trace_path = event->mimeData()->urls().at(i).toLocalFile();

            if (rra_util::TraceValidToLoad(potential_trace_path) == true)
            {
                rra::TraceManager::Get().LoadTrace(potential_trace_path);
                break;
            }
        }
    }
}

rra::RRAPaneId MainWindow::SetupNextPane(rra::RRAPaneId pane)
{
    const rra::NavLocation* const nav_location = pane_manager_.SetupNextPane(pane);
    if (nav_location == nullptr)
    {
        return pane;
    }

    // These things emit signals.
    ui_->start_list_->setCurrentRow(nav_location->start_list_row);
    ui_->overview_list_->setCurrentRow(nav_location->overview_list_row);
    ui_->tlas_sub_tab_->setCurrentIndex(nav_location->tlas_list_row);
    ui_->blas_sub_tab_->setCurrentIndex(nav_location->blas_list_row);
    ui_->settings_list_->setCurrentRow(nav_location->settings_list_row);
    ui_->main_tab_widget_->setCurrentIndex(nav_location->main_tab_index);

    return (pane_manager_.UpdateCurrentPane());
}

void MainWindow::ViewPane(int pane)
{
    rra::RRAPaneId current_pane = SetupNextPane(static_cast<rra::RRAPaneId>(pane));
    Q_ASSERT(current_pane == pane);
    rra::NavigationManager::Get().RecordNavigationEventPaneSwitch(current_pane);
}

QString MainWindow::GetTitleBarString() const
{
    QString title = PRODUCT_APP_NAME PRODUCT_BUILD_SUFFIX;

    title.append(" - ");
    QString version_string = QString("V%1").arg(PRODUCT_VERSION_STRING);

    title.append(version_string);

    return title;
}

void MainWindow::UpdateTitlebar()
{
    QString title = "";

    if (RraTraceLoaderValid())
    {
        const QString& file_name = rra::TraceManager::Get().GetTracePath();
        title.append(file_name);
        title.append(" - ");
    }

    title.append(GetTitleBarString());

    setWindowTitle(title);
}
