/**
 * @file MainWindow.cpp
 * @author   Ulrich Kemloh <kemlohulrich@gmail.com>
 * @version 0.1
 * Copyright (C) <2009-2010>
 *
 * @section LICENSE
 * This file is part of JuPedSim.
 *
 * JuPedSim is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * JuPedSim is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with JuPedSim. If not, see <http://www.gnu.org/licenses/>.
 *
 * @section DESCRIPTION
 *
 * \brief main program class
 *
 *
 *  Created on: 11.05.2009
 *
 */

#include "MainWindow.h"

#include "../forms/Settings.h"
#include "ApplicationState.h"
#include "Frame.h"
#include "Log.h"
#include "Parsing.h"
#include "SyncData.h"
#include "SystemSettings.h"
#include "TrajectoryPoint.h"
#include "Visualisation.h"
#include "extern_var.h"
#include "geometry/FacilityGeometry.h"

#include <QApplication>
#include <QCloseEvent>
#include <QColorDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QIODevice>
#include <QInputDialog>
#include <QListView>
#include <QMessageBox>
#include <QMimeData>
#include <QSplitter>
#include <QStandardItemModel>
#include <QString>
#include <QTableView>
#include <QTemporaryFile>
#include <QThread>
#include <QTime>
#include <filesystem>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

//----------- @todo: this part is copy/paste from main.cpp.
std::string ver_string1(int a, int b, int c)
{
    std::ostringstream ss;
    ss << a << '.' << b << '.' << c;
    return ss.str();
}

std::string true_cxx1 =
#ifdef __clang__
    "clang++";
#elif defined(__GNUC__)
    "g++";
#elif defined(__MINGW32__)
    "MinGW";
#elif defined(_MSC_VER)
    "Visual Studio";
#else
    "Compiler not identified";
#endif

std::string true_cxx_ver1 =
#ifdef __clang__
    ver_string1(__clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(__GNUC__)
    ver_string1(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#elif defined(__MINGW32__)
    ver_string1(__MINGW32__, __MINGW32_MAJOR_VERSION, __MINGW32_MINOR_VERSION);
#elif defined(_MSC_VER)
    ver_string1(_MSC_VER, _MSC_FULL_VER, _MSC_BUILD);
#else
    "";
#endif

//////////////////////////////////////////////////////////////////////////////
// Creation & Destruction
//////////////////////////////////////////////////////////////////////////////
MainWindow::MainWindow(QWidget * parent) : QMainWindow(parent)
{
    ui.setupUi(this);
    this->setWindowTitle("JPSvis");

    // used for saving the settings in a persistant way
    QCoreApplication::setOrganizationName("Forschungszentrum_Juelich_GmbH");
    QCoreApplication::setOrganizationDomain("jupedsim.org");
    QCoreApplication::setApplicationName("jupedsim");

    _visualisationThread = new Visualisation(this, ui.render_widget->renderWindow());

    travistoOptions = new Settings(this);
    travistoOptions->setWindowTitle("Settings");
    travistoOptions->setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint);

    QObject::connect(
        &_visualisationThread->getGeometry().GetModel(),
        SIGNAL(itemChanged(QStandardItem *)),
        this,
        SLOT(slotOnGeometryItemChanged(QStandardItem *)));

    isPlaying             = false;
    isPaused              = false;
    numberOfDatasetLoaded = 0;
    frameSliderHold       = false;

    // some hand made stuffs
    // the state of actionShowGeometry_Structure connect the state of the
    // structure window
    connect(&_geoStructure, &MyQTreeView::changeState, [=]() {
        ui.actionShowGeometry_Structure->setChecked(false);
    });
    // connections
    connect(ui.actionShow_Directions, &QAction::changed, this, &MainWindow::slotShowDirections);
    connect(
        ui.actionTop_Rotate,
        &QAction::triggered,
        this,
        &MainWindow::slotSetCameraPerspectiveToTopRotate);
    connect(
        ui.actionSide_Rotate,
        &QAction::triggered,
        this,
        &MainWindow::slotSetCameraPerspectiveToSideRotate);

    labelCurrentAction = new QLabel();
    labelCurrentAction->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    labelCurrentAction->setText("   Idle   ");
    statusBar()->addPermanentWidget(labelCurrentAction);

    labelFrameNumber = new QLabel();
    labelFrameNumber->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    labelFrameNumber->setText("fps:");
    statusBar()->addPermanentWidget(labelFrameNumber);

    labelRecording = new QLabel();
    labelRecording->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    labelRecording->setText(" rec: off ");
    statusBar()->addPermanentWidget(labelRecording);

    // restore the settings
    loadAllSettings();
}

MainWindow::~MainWindow()
{
    extern_shutdown_visual_thread = true;
    extern_recording_enable       = false;

    // save all settings for the next session
    if(ui.actionRemember_Settings->isChecked()) {
        saveAllSettings();
        Log::Info("saving all settings");
    } else {
        // first clear everyting
        QSettings settings;
        settings.clear();
        // then remember that we do not want any settings saved
        settings.setValue("options/rememberSettings", false);
        Log::Info("clearing all settings");
    }

    delete _visualisationThread;
    delete travistoOptions;
    delete labelCurrentAction;
    delete labelFrameNumber;
    delete labelRecording;
}

//////////////////////////////////////////////////////////////////////////////
// Public slots
//////////////////////////////////////////////////////////////////////////////
void MainWindow::slotOpenFile()
{
    switch(_state) {
        case ApplicationState::Playing:
            [[fallthrough]];
        case ApplicationState::NoData:
            [[fallthrough]];
        case ApplicationState::Paused: {
            const auto path = selectFileToLoad();
            if(path) {
                stopRendering();
                clearDataSet(1);
                const bool could_load_data = tryParseFile(path.value());
                if(could_load_data) {
                    _state = ApplicationState::Paused;
                    enablePlayerControls();
                    startRendering();
                } else {
                    _state = ApplicationState::NoData;
                    disablePlayerControls();
                }
            }
        }
    }
}

void MainWindow::slotToggleReplay(bool checked)
{
    if(checked) {
        startReplay();
    } else {
        stopReplay();
    }
}

void MainWindow::slotRewindFramesToBegin()
{
    switch(_state) {
        case ApplicationState::NoData:
            break;
        case ApplicationState::Paused:
            [[fallthrough]];
        case ApplicationState::Playing:
            extern_trajectories_firstSet.resetFrameCursor();
    }
}

//////////////////////////////////////////////////////////////////////////////
// Private methods
//////////////////////////////////////////////////////////////////////////////
void MainWindow::startReplay()
{
    switch(_state) {
        case ApplicationState::Playing:
            [[fallthrough]];
        case ApplicationState::NoData:
            break;
        case ApplicationState::Paused:
            _state = ApplicationState::Playing;
            // TODO(kkz): remove those bools
            isPlaying       = true;
            isPaused        = false;
            extern_is_pause = false;
            labelCurrentAction->setText("   playing   ");
            break;
    }
}

void MainWindow::stopReplay()
{
    switch(_state) {
        case ApplicationState::Playing:
            // TODO(kkz): remove those bools
            isPlaying       = false;
            isPaused        = true;
            extern_is_pause = true;
            labelCurrentAction->setText("   paused   ");
            _state = ApplicationState::Paused;
            break;
        case ApplicationState::NoData:
            [[fallthrough]];
        case ApplicationState::Paused:
            break;
    }
}

void MainWindow::enablePlayerControls()
{
    ui.BtPreviousFrame->setEnabled(true);
    ui.BtStart->setEnabled(true);
    ui.BtStart->setChecked(false);
    ui.BtNextFrame->setEnabled(true);
    ui.BtRecord->setEnabled(true);
    ui.replaySpeed->setEnabled(true);
    ui.rewind->setEnabled(true);
}

void MainWindow::disablePlayerControls()
{
    ui.BtPreviousFrame->setEnabled(false);
    ui.BtStart->setEnabled(false);
    ui.BtStart->setChecked(false);
    ui.BtNextFrame->setEnabled(false);
    ui.BtRecord->setEnabled(false);
    ui.replaySpeed->setEnabled(false);
    ui.rewind->setEnabled(false);
}

void MainWindow::startRendering()
{
    isPlaying       = false;
    isPaused        = true;
    extern_is_pause = true;
    _visualisationThread->run();
}

void MainWindow::stopRendering()
{
    _visualisationThread->stop();
    extern_shutdown_visual_thread = true;
    waitForVisioThread();

    // reset all frames cursors
    resetAllFrameCursor();

    // disable/reset all graphical elements
    slotClearAllDataset();
    isPlaying = false;
    isPaused  = false;
    resetGraphicalElements();
    labelCurrentAction->setText(" Idle ");
};

std::optional<std::filesystem::path> MainWindow::selectFileToLoad()
{
    const auto fileName = QFileDialog::getOpenFileName(
        this,
        "Select the file containing the data to visualize",
        QDir::currentPath(),
        "JuPedSim Files (*.xml *.txt);;All Files (*.*)");

    // the action was cancelled
    if(fileName.isNull()) {
        return {};
    } else {
        return {std::filesystem::path{fileName.toStdString()}};
    }
}

//////////////////////////////////////////////////////////////////////////////
// Old Code
//////////////////////////////////////////////////////////////////////////////
void MainWindow::slotHelpAbout()
{
    Log::Info("About JPSvis");
    QString gittext =
        QMessageBox::tr("<p style=\"line-height:1.4\" "
                        "style=\"color:Gray;\"><small><i>Version %1</i></small></p>"
                        "<p style=\"line-height:0.4\" style=\"color:Gray;\"><i>Hash</i> "
                        "%2</p>"
                        "<p  style=\"line-height:0.4\" style=\"color:Gray;\"><i>Date</i> "
                        "%3</p>"
                        "<p  style=\"line-height:0.4\" style=\"color:Gray;\"><i>Branch</i> "
                        "%4</p><hr>")
            .arg(JPSVIS_VERSION)
            .arg(GIT_COMMIT_HASH)
            .arg(GIT_COMMIT_DATE)
            .arg(GIT_BRANCH);

    QString text =
        QMessageBox::tr("<p style=\"color:Gray;\"><small><i> &copy; 2009-2021  Ulrich Kemloh "
                        "<br><a href=\"http://jupedsim.org\">jupedsim.org</a></i></small></p>"

        );

    QMessageBox msg(QMessageBox::Information, "About JPSvis", gittext + text, QMessageBox::Ok);
    QString logo = ":/new/iconsS/icons/JPSvis.icns";
    msg.setIconPixmap(QPixmap(logo));

    // Change font
    QFont font("Tokyo");
    font.setPointSize(10);
    msg.setFont(font);
    msg.exec();
}

/// clear all datasets previously entered.
void MainWindow::slotClearAllDataset()
{
    clearDataSet(1);
    numberOfDatasetLoaded = 0;
}

bool MainWindow::tryParseFile(const std::filesystem::path & path)
{
    Log::Info("Trying to parse %s", path.string().c_str());
    const auto file_type = Parsing::detectFileType(path);
    switch(file_type) {
        case Parsing::InputFileType::GEOMETRY_XML:
            return tryParseGeometry(path);
        case Parsing::InputFileType::TRAJECTORIES_TXT:
            return tryParseTrajectory(path);
        case Parsing::InputFileType::UNRECOGNIZED:
            return false;
    }
}

bool MainWindow::tryParseGeometry(const std::filesystem::path & path)
{
    return Parsing::readJpsGeometryXml(path, _visualisationThread->getGeometry());
}

bool MainWindow::tryParseTrajectory(const std::filesystem::path & path)
{
    const auto parent_path = path.parent_path();
    auto fileName          = QString::fromStdString(path.string());

    const auto source_file =
        parent_path / std::filesystem::path(Parsing::extractSourceFileTXT(fileName).toStdString());
    const bool readSource = std::filesystem::is_regular_file(source_file);
    if(readSource) {
        Log::Info("Found source file: \"%s\"", source_file.string().c_str());
    }

    const auto ttt_file =
        parent_path /
        std::filesystem::path(Parsing::extractTrainTimeTableFileTXT(fileName).toStdString());
    const bool readTrainTimeTable = std::filesystem::is_regular_file(ttt_file);
    if(readTrainTimeTable) {
        Log::Info("Found train time table file: \"%s\"", ttt_file.string().c_str());
    }

    const auto tt_file =
        parent_path /
        std::filesystem::path(Parsing::extractTrainTypeFileTXT(fileName).toStdString());
    const bool readTrainTypes = std::filesystem::is_regular_file(tt_file);
    if(readTrainTypes) {
        Log::Info("Found train types file: \"%s\"", tt_file.string().c_str());
    }

    const auto goal_file =
        parent_path / std::filesystem::path(Parsing::extractGoalFileTXT(fileName).toStdString());
    const bool readGoal = std::filesystem::is_regular_file(goal_file);
    if(readGoal) {
        Log::Info("Found goal file: \"%s\"", goal_file.string().c_str());
    }

    std::map<std::string, std::shared_ptr<TrainType>> trainTypes;
    if(readTrainTypes) {
        Parsing::LoadTrainType(tt_file.string(), trainTypes);
        extern_trainTypes = trainTypes;
    }

    std::map<int, std::shared_ptr<TrainTimeTable>> trainTimeTable;
    if(readTrainTimeTable) {
        bool ret               = Parsing::LoadTrainTimetable(ttt_file.string(), trainTimeTable);
        extern_trainTimeTables = trainTimeTable;
    }

    const auto geometry_file =
        parent_path /
        std::filesystem::path(Parsing::extractGeometryFilenameTXT(fileName).toStdString());

    if(!tryParseGeometry(geometry_file)) {
        return false;
    }

    std::tuple<Point, Point> trackStartEnd;
    double elevation;
    for(auto tab : trainTimeTable) {
        int trackId = tab.second->pid;
        trackStartEnd =
            Parsing::GetTrackStartEnd(QString::fromStdString(geometry_file.string()), trackId);
        elevation = 0;

        Point trackStart = std::get<0>(trackStartEnd);
        Point trackEnd   = std::get<1>(trackStartEnd);

        tab.second->pstart    = trackStart;
        tab.second->pend      = trackEnd;
        tab.second->elevation = elevation;

        Log::Info("=======\n");
        Log::Info("tab: %d\n", tab.first);
        Log::Info("Track start: [%.2f, %.2f]\n", trackStart._x, trackStart._y);
        Log::Info("Track end: [%.2f, %.2f]\n", trackEnd._x, trackEnd._y);
        Log::Info("Room: %d\n", tab.second->rid);
        Log::Info("Subroom %d\n", tab.second->sid);
        Log::Info("Elevation %d\n", tab.second->elevation);
        Log::Info("=======\n");
    }
    for(auto tab : trainTypes)
        Log::Info("type: %s\n", tab.first.c_str());

    double fps;
    // TODO(kkratz): Figure out why this is required
    extern_trajectories_firstSet.clearFrames();
    extern_first_dataset_visible = true;
    ui.actionFirst_Group->setEnabled(true);
    ui.actionFirst_Group->setChecked(true);
    slotToggleFirstPedestrianGroup();
    if(false == Parsing::ParseTxtFormat(fileName, &extern_trajectories_firstSet, &fps)) {
        return false;
    }

    QString frameRateStr = QString::number(fps);
    _visualisationThread->slotSetFrameRate(fps);
    labelFrameNumber->setText("fps: " + frameRateStr + "/" + frameRateStr);

    statusBar()->showMessage(tr("file loaded and parsed"));

    extern_shutdown_visual_thread = true;
    extern_first_dataset_loaded   = true;
    waitForVisioThread();
    return true;
}

void MainWindow::slotRecord()
{
    if(extern_recording_enable) {
        int res = QMessageBox::warning(
            this,
            "action",
            "JuPedSim is already recording a video\n"
            "do you wish to stop the recording?",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if(res == QMessageBox::Yes) {
            extern_recording_enable = false;
            ui.BtRecord->setToolTip("Start Recording");
            labelCurrentAction->setText("   Playing   ");
            labelRecording->setText(" rec: off ");
            return;
        }
    }
    extern_launch_recording = true;
    ui.BtRecord->setToolTip("Stop Recording");
    labelRecording->setText(" rec: on ");
}

void MainWindow::slotReset()
{
    // stop any recording
    if(extern_recording_enable) {
        int res = QMessageBox::question(
            this,
            "action",
            "do you wish to stop the recording?",
            QMessageBox::Discard | QMessageBox::Yes,
            QMessageBox::Yes);
        if(res == QMessageBox::Yes) {
            extern_recording_enable = false;
            labelCurrentAction->setText("   Playing   ");
        } else {
            return;
        }
    }

    if(anyDatasetLoaded()) {
        int res = QMessageBox::question(
            this,
            "action",
            "This will also clear any dataset if loaded.\n"
            "Do you wish to continue?",
            QMessageBox::Discard | QMessageBox::Yes,
            QMessageBox::Yes);
        if(res == QMessageBox::Discard) {
            return;
        }
    }

    // shutdown the visual thread
    extern_shutdown_visual_thread = true;
    waitForVisioThread();

    // reset all buttons
    slotClearAllDataset();
    isPlaying = false;
    isPaused  = false;
    labelCurrentAction->setText("   Idle   ");
    resetGraphicalElements();
}

void MainWindow::slotCurrentAction(QString msg)
{
    msg = " " + msg + " ";
    statusBar()->showMessage(msg);
}

void MainWindow::slotFrameNumber(unsigned long actualFrameCount, unsigned long minFrame)
{
    // compute the  mamixum framenumber
    int maxFrameCount = 1;
    if(extern_first_dataset_loaded) {
        maxFrameCount = extern_trajectories_firstSet.getFramesNumber();
    }
    maxFrameCount += minFrame;

    if(actualFrameCount > maxFrameCount)
        actualFrameCount = maxFrameCount;

    if(!frameSliderHold)
        if(maxFrameCount != 0) // TODO WTF, otherwise an arrymtic exeption arises
            ui.framesIndicatorSlider->setValue(
                (ui.framesIndicatorSlider->maximum() * actualFrameCount) / maxFrameCount);
}

void MainWindow::slotRunningTime(unsigned long timems)
{
    ui.time->setText(QString::fromStdString(std::to_string(timems)));
}

void MainWindow::slotRenderingTime(int fps)
{
    QString msg = labelFrameNumber->text().replace(QRegExp("[0-9]+/"), QString::number(fps) + "/");
    labelFrameNumber->setText(msg);
}
void MainWindow::slotExit()
{
    Log::Info("Exit jpsvis");
    stopRendering();
    cleanUp();
    qApp->exit();
}

void MainWindow::closeEvent(QCloseEvent * event)
{
    hide();
    cleanUp();
    event->accept();
}

void MainWindow::cleanUp()
{
    // stop the recording process
    extern_recording_enable       = false;
    extern_shutdown_visual_thread = true;

    if(SystemSettings::getRecordPNGsequence())
        slotRecordPNGsequence();

    waitForVisioThread();
}

void MainWindow::resetGraphicalElements()
{
    ui.BtRecord->setEnabled(false);
    ui.framesIndicatorSlider->setValue(ui.framesIndicatorSlider->minimum());
    ui.actionShow_Legend->setEnabled(true);
    ui.actionShow_Trajectories->setEnabled(true);
    ui.action3_D->setEnabled(true);
    ui.action2_D->setEnabled(true);

    labelRecording->setText("rec: off");
    statusBar()->showMessage(tr("select a File"));

    // resetting the start/stop recording action
    // check whether the a png recording sequence was playing, stop if the case
    if(SystemSettings::getRecordPNGsequence())
        slotRecordPNGsequence();
}

void MainWindow::slotToggleFirstPedestrianGroup()
{
    if(ui.actionFirst_Group->isChecked()) {
        extern_first_dataset_visible = true;
    } else {
        extern_first_dataset_visible = false;
    }
    extern_force_system_update = true;
}

bool MainWindow::anyDatasetLoaded()
{
    return extern_first_dataset_loaded;
}

void MainWindow::slotShowTrajectoryOnly()
{
    SystemSettings::setShowTrajectories(ui.actionShow_Trajectories->isChecked());
    extern_force_system_update = true;
}

void MainWindow::slotShowPedestrianOnly()
{
    if(ui.actionShow_Agents->isChecked()) {
        SystemSettings::setShowAgents(true);
    } else {
        SystemSettings::setShowAgents(false);
    }
    extern_force_system_update = true;
}

void MainWindow::slotShowGeometry()
{
    if(ui.actionShow_Geometry->isChecked()) {
        _visualisationThread->setGeometryVisibility(true);
        ui.actionShow_Exits->setEnabled(true);
        ui.actionShow_Walls->setEnabled(true);
        ui.actionShow_Geometry_Captions->setEnabled(true);
        ui.actionShow_Navigation_Lines->setEnabled(true);
        ui.actionShow_Floor->setEnabled(true);
        SystemSettings::setShowGeometry(true);
    } else {
        _visualisationThread->setGeometryVisibility(false);
        ui.actionShow_Exits->setEnabled(false);
        ui.actionShow_Walls->setEnabled(false);
        ui.actionShow_Geometry_Captions->setEnabled(false);
        ui.actionShow_Navigation_Lines->setEnabled(false);
        ui.actionShow_Floor->setEnabled(false);
        SystemSettings::setShowGeometry(false);
    }
    extern_force_system_update = true;
}

/// shows/hide geometry
void MainWindow::slotShowHideExits()
{
    bool status = ui.actionShow_Exits->isChecked();
    _visualisationThread->showDoors(status);
    SystemSettings::setShowExits(status);
}

/// shows/hide geometry
void MainWindow::slotShowHideWalls()
{
    bool status = ui.actionShow_Walls->isChecked();
    _visualisationThread->showWalls(status);
    SystemSettings::setShowWalls(status);
}

void MainWindow::slotShowHideNavLines()
{
    bool status = ui.actionShow_Navigation_Lines->isChecked();
    _visualisationThread->showNavLines(status);
    SystemSettings::setShowNavLines(status);
}

// todo: add to the system settings
void MainWindow::slotShowHideFloor()
{
    bool status = ui.actionShow_Floor->isChecked();
    _visualisationThread->showFloor(status);
    SystemSettings::setShowFloor(status);
}

/// update the position slider
void MainWindow::slotUpdateFrameSlider(int newValue)
{
    // first get the correct position
    int maxFrameCount = 1;
    if(extern_first_dataset_loaded) {
        int t = extern_trajectories_firstSet.getFramesNumber();
        if(maxFrameCount < t)
            maxFrameCount = t;
    }

    int update = ((maxFrameCount * newValue) / ui.framesIndicatorSlider->maximum());

    // then set the correct position
    if(extern_first_dataset_loaded) {
        extern_trajectories_firstSet.setFrameCursorTo(update);
    }
}

/// clear the corresponding dataset;
void MainWindow::clearDataSet(int ID)
{
    switch(ID) {
        case 1:
            // extern_trajectories_firstSet.clear();
            extern_trajectories_firstSet.clearFrames();
            extern_trajectories_firstSet.resetFrameCursor();
            extern_first_dataset_loaded  = false;
            extern_first_dataset_visible = false;
            ui.actionFirst_Group->setEnabled(false);
            ui.actionFirst_Group->setChecked(false);
            slotToggleFirstPedestrianGroup();
            numberOfDatasetLoaded--;
            _visualisationThread->getGeometry().Clear(); // also clear the geometry info
            // close geometryStrucutre window
            if(_geoStructure.isVisible())
                _geoStructure.close();
            break;

        default:
            break;
    }

    if(numberOfDatasetLoaded < 0)
        numberOfDatasetLoaded = 0;
}

void MainWindow::resetAllFrameCursor()
{
    extern_trajectories_firstSet.resetFrameCursor();
}

/// wait for visualisation thread to shutdown
///@todo why two different threads shutdown procedure.
void MainWindow::waitForVisioThread()
{
    extern_shutdown_visual_thread = false;
}

/// set visualisation mode to 2D
void MainWindow::slotToogle2D()
{
    if(ui.action2_D->isChecked()) {
        extern_is_3D = false;
        ui.action3_D->setChecked(false);
        SystemSettings::set2D(true);

    } else {
        extern_is_3D = true;
        ui.action3_D->setChecked(true);
        SystemSettings::set2D(false);
    }
    bool status = SystemSettings::get2D() && SystemSettings::getShowGeometry();
    _visualisationThread->setGeometryVisibility2D(status);
    extern_force_system_update = true;
}

/// set visualisation mode to 3D
void MainWindow::slotToogle3D()
{
    if(ui.action3_D->isChecked()) {
        extern_is_3D = true;
        ui.action2_D->setChecked(false);
        SystemSettings::set2D(false);

    } else {
        extern_is_3D = false;
        ui.action2_D->setChecked(true);
        SystemSettings::set2D(true);
    }
    bool status = !SystemSettings::get2D() && SystemSettings::getShowGeometry();
    _visualisationThread->setGeometryVisibility3D(status);
    extern_force_system_update = true;
}

void MainWindow::slotFrameSliderPressed()
{
    frameSliderHold = true;
}

void MainWindow::slotFrameSliderReleased()
{
    frameSliderHold = false;
}

void MainWindow::slotToogleShowLegend()
{
    if(ui.actionShow_Legend->isChecked()) {
        SystemSettings::setShowLegend(true);
    } else {
        SystemSettings::setShowLegend(false);
    }
}

void MainWindow::slotNextFrame()
{
    if(extern_first_dataset_loaded) {
        int newValue = extern_trajectories_firstSet.getFrameCursor() + 1;
        extern_trajectories_firstSet.setFrameCursorTo(newValue);
    }
}

void MainWindow::slotPreviousFrame()
{
    if(extern_first_dataset_loaded) {
        int newValue = extern_trajectories_firstSet.getFrameCursor() - 1;
        extern_trajectories_firstSet.setFrameCursorTo(newValue);
    }
}

void MainWindow::slotShowPedestrianCaption()
{
    SystemSettings::setShowAgentsCaptions(ui.actionShow_Captions->isChecked());
    extern_force_system_update = true;
}

void MainWindow::slotShowDirections()
{
    SystemSettings::setShowDirections(ui.actionShow_Directions->isChecked());
    extern_force_system_update = true;
}

void MainWindow::slotToogleShowAxis()
{
    _visualisationThread->setAxisVisible(ui.actionShow_Axis->isChecked());
}

// todo: rename this to slotChangeSettting
void MainWindow::slotChangePedestrianShape()
{
    travistoOptions->show();
}

void MainWindow::slotCaptionColorAuto()
{
    emit signal_controlSequence("CAPTION_AUTO");
}

void MainWindow::slotCaptionColorCustom()
{
    emit signal_controlSequence("CAPTION_CUSTOM");
}

void MainWindow::slotChangeBackgroundColor()
{
    QColorDialog * colorDialog = new QColorDialog(this);
    colorDialog->setToolTip("Choose a new color for the background");
    QColor col = colorDialog->getColor(Qt::white, this, "Select new background color");

    // the user may have cancelled the process
    if(col.isValid() == false)
        return;

    _visualisationThread->setBackgroundColor(col);

    QSettings settings;
    settings.setValue("options/bgColor", col);

    delete colorDialog;
}

/// change the wall color
void MainWindow::slotChangeWallsColor()
{
    QColorDialog * colorDialog = new QColorDialog(this);
    colorDialog->setToolTip("Choose a new color for walls");
    QColor col = colorDialog->getColor(Qt::white, this, "Select new wall color");

    // the user may have cancelled the process
    if(col.isValid() == false)
        return;

    _visualisationThread->setWallsColor(col);

    QSettings settings;
    settings.setValue("options/wallsColor", col);

    delete colorDialog;
}

/// change the exits color
void MainWindow::slotChangeExitsColor()
{
    QColorDialog * colorDialog = new QColorDialog(this);
    colorDialog->setToolTip("Choose a new color for the exits");
    QColor col = colorDialog->getColor(Qt::white, this, "Select new exit color");

    // the user may have cancelled the process
    if(col.isValid() == false)
        return;

    _visualisationThread->setExitsColor(col);

    QSettings settings;
    settings.setValue("options/exitsColor", col);
    Log::Info("Change Exits Color to %s", col.name().toStdString().c_str());
    delete colorDialog;
}

/// change the navigation lines colors
void MainWindow::slotChangeNavLinesColor()
{
    QColorDialog * colorDialog = new QColorDialog(this);
    colorDialog->setToolTip("Choose a new color for walls");
    QColor col = colorDialog->getColor(Qt::white, this, "Select new navigation lines color");

    // the user may have cancelled the process
    if(col.isValid() == false)
        return;

    _visualisationThread->setNavLinesColor(col);

    QSettings settings;
    settings.setValue("options/navLinesColor", col);

    delete colorDialog;
}

void MainWindow::slotChangeFloorColor()
{
    QColorDialog * colorDialog = new QColorDialog(this);
    colorDialog->setToolTip("Choose a new color for the floor");
    QColor col = colorDialog->getColor(Qt::white, this, "Select new floor color");

    // the user may have cancelled the process
    if(col.isValid() == false)
        return;

    _visualisationThread->setFloorColor(col);

    QSettings settings;
    settings.setValue("options/floorColor", col);

    delete colorDialog;
}

void MainWindow::slotChangeObstacleColor()
{
    QColorDialog * colorDialog = new QColorDialog(this);
    colorDialog->setToolTip("Choose a new color for the obstacles");
    QColor col = colorDialog->getColor(Qt::white, this, "Select new obstalce color");

    // the user may have cancelled the process
    if(col.isValid() == false)
        return;

    _visualisationThread->setObstacleColor(col);

    QSettings settings;
    settings.setValue("options/obstacle", col);

    delete colorDialog;
}

void MainWindow::slotSetCameraPerspectiveToTop()
{
    int p = 1; // TOP

    _visualisationThread->setCameraPerspective(p);
    // disable the virtual agent view
    SystemSettings::setVirtualAgent(-1);
}

void MainWindow::slotSetCameraPerspectiveToTopRotate()
{
    int p      = 2; // TOP rotate
    bool ok    = false;
    int degree = QInputDialog::getInt(
        this, "Top rotate", "Rotation degrees [range:-180-->180]:", 0, -180, 180, 10, &ok);
    if(ok) {
        _visualisationThread->setCameraPerspective(p, degree);
        // disable the virtual agent view
        SystemSettings::setVirtualAgent(-1);
    }
}

void MainWindow::slotSetCameraPerspectiveToSideRotate()
{
    int p      = 3; // SIDE rotate
    bool ok    = false;
    int degree = QInputDialog::getInt(
        this, "Side rotate", "Rotation degrees [range:-80-->80]:", 0, -80, 80, 10, &ok);
    if(ok) {
        _visualisationThread->setCameraPerspective(p, degree);
        // disable the virtual agent view
        SystemSettings::setVirtualAgent(-1);
    }
}

void MainWindow::slotSetCameraPerspectiveToVirtualAgent()
{
    bool ok   = false;
    int agent = QInputDialog::getInt(
        this,
        tr("choose the agent you want to see the scene through"),
        tr("agent ID(default to 1):"),
        1,
        1,
        500,
        1,
        &ok);

    if(ok) {
        int p = 4; // virtual agent
        _visualisationThread->setCameraPerspective(p);

        // get the virtual agent ID
        SystemSettings::setVirtualAgent(agent);
    }
}

void MainWindow::slotClearGeometry()
{
    _visualisationThread->setGeometry(NULL);
}

void MainWindow::slotErrorOutput(QString err)
{
    QMessageBox msgBox;
    msgBox.setText("Error");
    msgBox.setInformativeText(err);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.exec();
}

void MainWindow::slotTakeScreenShot()
{
    // extern_take_screenshot=true;
    extern_take_screenshot = !extern_take_screenshot;
}

/// load settings, parsed from the project file
void MainWindow::loadAllSettings()
{
    Log::Info("restoring previous settings");
    QSettings settings;

    // view
    if(settings.contains("view/2d")) {
        bool checked = settings.value("view/2d").toBool();
        ui.action2_D->setChecked(checked);
        ui.action3_D->setChecked(!checked);
        SystemSettings::set2D(checked);
        Log::Info("2D: %s", checked ? "Yes" : "No");
    }
    if(settings.contains("view/showAgents")) {
        bool checked = settings.value("view/showAgents").toBool();
        ui.actionShow_Agents->setChecked(checked);
        SystemSettings::setShowAgents(checked);
        Log::Info("show Agents: %s", checked ? "Yes" : "No");
    }
    if(settings.contains("view/showCaptions")) {
        bool checked = settings.value("view/showCaptions").toBool();
        ui.actionShow_Captions->setChecked(checked);
        SystemSettings::setShowAgentsCaptions(checked);
        Log::Info("show Captions: %s", checked ? "Yes" : "No");
    }
    if(settings.contains("view/showDirections")) {
        bool checked = settings.value("view/showDirections").toBool();
        ui.actionShow_Directions->setChecked(checked);
        SystemSettings::setShowDirections(checked);
        Log::Info("show Directions: %s", checked ? "Yes" : "No");
    }
    if(settings.contains("view/showTrajectories")) {
        bool checked = settings.value("view/showTrajectories").toBool();
        ui.actionShow_Trajectories->setChecked(checked);
        SystemSettings::setShowTrajectories(checked);
        Log::Info("show Trajectories: %s", checked ? "Yes" : "No");
    }
    if(settings.contains("view/showGeometry")) {
        bool checked = settings.value("view/showGeometry").toBool();
        ui.actionShow_Geometry->setChecked(checked);
        slotShowGeometry(); // will take care of the others things like
                            // enabling options
        Log::Info("show Geometry: %s", checked ? "Yes" : "No");
    }
    if(settings.contains("view/showFloor")) {
        bool checked = settings.value("view/showFloor").toBool();
        ui.actionShow_Floor->setChecked(checked);
        slotShowHideFloor();
        Log::Info("show Floor: %s", checked ? "Yes" : "No");
    }
    if(settings.contains("view/showExits")) {
        bool checked = settings.value("view/showExits").toBool();
        ui.actionShow_Exits->setChecked(checked);
        slotShowHideExits();
        Log::Info("show Exits: %s", checked ? "Yes" : "No");
    }
    if(settings.contains("view/showWalls")) {
        bool checked = settings.value("view/showWalls").toBool();
        ui.actionShow_Walls->setChecked(checked);
        slotShowHideWalls();
        Log::Info("show Walls: %s", checked ? "Yes" : "No");
    }
    if(settings.contains("view/showGeoCaptions")) {
        bool checked = settings.value("view/showGeoCaptions").toBool();
        ui.actionShow_Geometry_Captions->setChecked(checked);
        slotShowHideGeometryCaptions();
        Log::Info("show geometry Captions: %s", checked ? "Yes" : "No");
    }
    if(settings.contains("view/showNavLines")) {
        bool checked = settings.value("view/showNavLines").toBool();
        ui.actionShow_Navigation_Lines->setChecked(checked);
        slotShowHideNavLines();
        Log::Info("show Navlines: %s", checked ? "Yes" : "No");
    }
    if(settings.contains("view/showObstacles")) {
        bool checked = settings.value("view/showObstacles").toBool();
        ui.actionShow_Obstacles->setChecked(checked);
        slotShowHideObstacles();
        Log::Info("show showObstacles: %s", checked ? "Yes" : "No");
    }
    if(settings.contains("view/showOnScreensInfos")) {
        bool checked = settings.value("view/showOnScreensInfos").toBool();
        ui.actionShow_Onscreen_Infos->setChecked(checked);
        slotShowOnScreenInfos();
        Log::Info("show OnScreensInfos: %s", checked ? "Yes" : "No");
    }
    if(settings.contains("view/showGradientField")) {
        bool checked = settings.value("view/showGradientField").toBool();
        ui.actionShow_Gradient_Field->setChecked(checked);
        slotShowHideGradientField();
        Log::Info("show GradientField: %s", checked ? "Yes" : "No");
    }
    // options
    if(settings.contains("options/rememberSettings")) {
        bool checked = settings.value("options/rememberSettings").toBool();
        ui.actionRemember_Settings->setChecked(checked);
        Log::Info("remember settings: %s", checked ? "Yes" : "No");
    }

    if(settings.contains("options/bgColor")) {
        QColor color = settings.value("options/bgColor").value<QColor>();
        SystemSettings::setBackgroundColor(color);
        Log::Info("background color: %s", color.name().toStdString().c_str());
    }

    if(settings.contains("options/exitsColor")) {
        QColor color = settings.value("options/exitsColor").value<QColor>();
        SystemSettings::setExitsColor(color);
        Log::Info("Exit color: %s", color.name().toStdString().c_str());
    }

    if(settings.contains("options/floorColor")) {
        QColor color = settings.value("options/floorColor").value<QColor>();
        SystemSettings::setFloorColor(color);
        Log::Info("Floor color: %s", color.name().toStdString().c_str());
    }

    if(settings.contains("options/wallsColor")) {
        QColor color = settings.value("options/wallsColor").value<QColor>();
        SystemSettings::setWallsColor(color);
        Log::Info("Walls color: %s", color.name().toStdString().c_str());
    }

    if(settings.contains("options/navLinesColor")) {
        QColor color = settings.value("options/navLinesColor").value<QColor>();
        SystemSettings::setNavLinesColor(color);
        Log::Info("NavLines color: %s", color.name().toStdString().c_str());
    }
    if(settings.contains("options/obstaclesColor")) {
        QColor color = settings.value("options/obstaclesColor").value<QColor>();
        SystemSettings::setObstacleColor(color);
        Log::Info("Obstacles color: %s", color.name().toStdString().c_str());
    }

    extern_force_system_update = true;
}

void MainWindow::saveAllSettings()
{
    // visualiation
    QSettings settings;

    // view
    settings.setValue("view/2d", ui.action2_D->isChecked());
    settings.setValue("view/showAgents", ui.actionShow_Agents->isChecked());
    settings.setValue("view/showCaptions", ui.actionShow_Captions->isChecked());
    settings.setValue("view/showDirections", ui.actionShow_Directions->isChecked());
    settings.setValue("view/showTrajectories", ui.actionShow_Trajectories->isChecked());
    settings.setValue("view/showGeometry", ui.actionShow_Geometry->isChecked());
    settings.setValue("view/showFloor", ui.actionShow_Floor->isChecked());
    settings.setValue("view/showWalls", ui.actionShow_Walls->isChecked());
    settings.setValue("view/showExits", ui.actionShow_Exits->isChecked());
    settings.setValue("view/showGeoCaptions", ui.actionShow_Geometry_Captions->isChecked());
    settings.setValue("view/showNavLines", ui.actionShow_Navigation_Lines->isChecked());
    settings.setValue("view/showOnScreensInfos", ui.actionShow_Onscreen_Infos->isChecked());
    settings.setValue("view/showObstacles", ui.actionShow_Obstacles->isChecked());
    settings.setValue("view/showGradientField", ui.actionShow_Gradient_Field->isChecked());

    // options: the color settings are saved in the methods where they are used.
    settings.setValue("options/listeningPort", SystemSettings::getListeningPort());
    settings.setValue("options/rememberSettings", ui.actionRemember_Settings->isChecked());
}

/// start/stop the recording process als png images sequences
void MainWindow::slotRecordPNGsequence()
{
    // TODO(kkz) disabled
    if(true) {
        slotErrorOutput("Start a video first");
    }

    // get the status from the system settings and toogle it
    bool status = SystemSettings::getRecordPNGsequence();

    if(status) {
        ui.actionRecord_PNG_sequences->setText("Record PNG sequence");
    } else {
        ui.actionRecord_PNG_sequences->setText("Stop PNG Recording");
    }

    // toggle the status
    SystemSettings::setRecordPNGsequence(!status);
}

/// render a PNG image sequence to an AVI video
void MainWindow::slotRenderPNG2AVI()
{
    slotErrorOutput("Not Implemented yet, sorry !");
}

void MainWindow::dragEnterEvent(QDragEnterEvent * event)
{
    if(event->mimeData()->hasFormat("text/uri-list"))
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent * event)
{
    // TODO(kkratz): This needs to follow the same flow as open file
    QList<QUrl> urls = event->mimeData()->urls();
    if(urls.isEmpty())
        return;

    // reset before load new file
    slotReset(); // when choose Discard : anyDatasetLoaded()==true
    if(anyDatasetLoaded()) {
        return;
    }

    bool mayPlay = false;

    for(int i = 0; i < urls.size(); i++) {
        QString fileName = urls[i].toLocalFile();
        if(fileName.isEmpty())
            continue;
        if(tryParseFile(std::filesystem::path(fileName.toStdString()))) {
            mayPlay = true;
        }
    }
}

/// show/hide onscreen information
/// information include Time and pedestrians left in the facility
void MainWindow::slotShowOnScreenInfos()
{
    bool value = ui.actionShow_Onscreen_Infos->isChecked();
    _visualisationThread->setOnscreenInformationVisibility(value);
    SystemSettings::setOnScreenInfos(value);
    Log::Info("Show On Screen Infos: %s", value ? "On" : "Off");
}

/// show/hide the geometry captions
void MainWindow::slotShowHideGeometryCaptions()
{
    bool value = ui.actionShow_Geometry_Captions->isChecked();
    _visualisationThread->setGeometryLabelsVisibility(value);
    SystemSettings::setShowGeometryCaptions(value);
}
void MainWindow::slotShowHideObstacles()
{
    bool value = ui.actionShow_Obstacles->isChecked();
    _visualisationThread->showObstacle(value);
    SystemSettings::setShowObstacles(value);
}
void MainWindow::slotShowHideGradientField()
{
    bool value = ui.actionShow_Gradient_Field->isChecked();
    _visualisationThread->showGradientField(value);
    SystemSettings::setShowGradientField(value);
}

void MainWindow::slotShowGeometryStructure()
{
    _geoStructure.setHidden(!ui.actionShowGeometry_Structure->isChecked());
    if(_visualisationThread->getGeometry().RefreshView()) {
        _geoStructure.setWindowTitle("Geometry structure");
        _geoStructure.setModel(&_visualisationThread->getGeometry().GetModel());
    }
    Log::Info(
        "Show Geometry Structure: %s", ui.actionShowGeometry_Structure->isChecked() ? "On" : "Off");
}

void MainWindow::slotOnGeometryItemChanged(QStandardItem * item)
{
    QStringList l = item->data().toString().split(":");
    if(l.length() > 1) {
        int room   = l[0].toInt();
        int subr   = l[1].toInt();
        bool state = item->checkState();
        _visualisationThread->getGeometry().UpdateVisibility(room, subr, state);
    } else {
        for(int i = 0; i < item->rowCount(); i++) {
            QStandardItem * child = item->child(i);
            child->setCheckState(item->checkState());
            slotOnGeometryItemChanged(child);
        }
    }
}
