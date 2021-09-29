/**
 * @file    SaxParser.cpp
 * @author  Ulrich Kemloh <kemlohulrich@gmail.com>
 * @version 0.1
 * Created on: 6 Sep 2010
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
 *
 *
 */
#include "Parsing.h"

#include "Frame.h"
#include "FrameElement.h"
#include "Log.h"
#include "SyncData.h"
#include "SystemSettings.h"
#include "TrajectoryPoint.h"
#include "general/Macros.h"
#include "geometry/Building.h"
#include "geometry/FacilityGeometry.h"
#include "geometry/GeometryFactory.h"
#include "geometry/JPoint.h"
#include "geometry/SubRoom.h"
#include "geometry/Wall.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QString>
#include <QTextStream>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <vtkActor.h>
#include <vtkAssembly.h>
#include <vtkCellArray.h>
#include <vtkGenericDataObjectReader.h>
#include <vtkImageDataGeometryFilter.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkPolyDataReader.h>
#include <vtkPolygon.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkStripper.h>
#include <vtkStructuredGridReader.h>
#include <vtkStructuredPointsReader.h>
#include <vtkTriangleFilter.h>
#include <vtkVersion.h>

namespace Parsing
{
InputFileType detectFileType(const std::filesystem::path & path)
{
    // Ok this is really stupid, but for now it is good enough
    // The detection is just looking at the file extension
    // TODO(kkratz): Do some ACTUAL detection
    if(!path.has_extension()) {
        return InputFileType::UNRECOGNIZED;
    }
    const auto file_extension = path.extension().string();
    if(file_extension == ".xml" || file_extension == ".XML") {
        return InputFileType::GEOMETRY_XML;
    }
    if(file_extension == ".txt" || file_extension == ".TXT") {
        return InputFileType::TRAJECTORIES_TXT;
    }
    return InputFileType::UNRECOGNIZED;
}

std::tuple<Point, Point> GetTrackStartEnd(QString geometryFile, int trackId)
{
    QString wd;
    QDir dir(wd);
    QDir fileDir(geometryFile);
    SystemSettings::getWorkingDirectory(wd);
    if(!fileDir.isAbsolute()) {
        QString s    = dir.relativeFilePath(geometryFile);
        geometryFile = wd + QDir::separator() + s;
    }

    Log::Info("filename: <%s)", geometryFile.toStdString().c_str());
    Log::Info("wd: <%s>", wd.toStdString().c_str());

    std::vector<Point> end_points;
    Point start_point(0, 0);
    Point end_point(0, 0);

    QDomDocument doc("");
    QFile file(geometryFile);
    if(!file.open(QIODevice::ReadOnly)) {
        Log::Error(
            "GetTrackStartEnd: could not open the file: %s\n", geometryFile.toStdString().c_str());
        exit(EXIT_FAILURE);
    }
    QString * errorCode = new QString();
    if(!doc.setContent(&file, errorCode)) {
        file.close();
        Log::Error(">> ErrorCode: %d\n", errorCode);
        exit(EXIT_FAILURE);
    }
    QDomElement root = doc.firstChildElement("geometry");
    // only parsing the geometry node
    if(root.tagName() != "geometry") {
        std::tuple<Point, Point> ret = std::make_tuple(Point(0, 0), Point(0, 0));
        Log::Error("root %s is not geometry\n", root.tagName().toStdString().c_str());
        return ret;
    }


    // parsing the subrooms
    QDomNodeList xSubRoomsNodeList = doc.elementsByTagName("subroom");
    // parsing the walls
    for(int i = 0; i < xSubRoomsNodeList.length(); i++) {
        QDomElement xPoly = xSubRoomsNodeList.item(i).firstChildElement("polygon");
        while(!xPoly.isNull()) {
            auto Type          = xPoly.attribute("type", "notype").toStdString();
            auto Caption       = xPoly.attribute("caption", "nocaption").toStdString();
            int parsed_trackId = xPoly.attribute("track_id", "-1").toInt();
            if(Type != "track") {
                xPoly = xPoly.nextSiblingElement("polygon");
                continue;
            }

            if(parsed_trackId != trackId) {
                xPoly = xPoly.nextSiblingElement("polygon");
                continue;
            }
            QDomNodeList xVertices = xPoly.elementsByTagName("vertex");
            for(int i = 0; i < xVertices.count(); i++) {
                auto start = xVertices.item(i).toElement().attribute("start", "false");

                double x1 = xVertices.item(i).toElement().attribute("px", "0").toDouble();
                double y1 = xVertices.item(i).toElement().attribute("py", "0").toDouble();
                if(start == "true") {
                    start_point = Point(x1, y1);
                } else {
                    end_points.push_back(Point(x1, y1)); // collect other points of track
                }
            }
            xPoly = xPoly.nextSiblingElement("polygon");
        } // poly
    }     // sub

    double min_d = -10000;
    // find the track point with the biggest distance to start.
    for(auto p : end_points) {
        double d = (p - start_point).NormSquare();
        if(d > min_d) {
            end_point = p;
            min_d     = d;
        }
    }
    std::tuple<Point, Point> ret = std::make_tuple(start_point, end_point);
    return ret;
}


bool readJpsGeometryXml(const std::filesystem::path & path, GeometryFactory & geoFac)
{
    Log::Info("Reading JPS geometry from \"%s\"", path.string().c_str());
    const auto jps_project_root_path = path.parent_path();
    Log::Info(
        "Rootdir for parsing the geometry file \"%s\"", jps_project_root_path.string().c_str());

    auto building = std::make_unique<Building>();
    building->SetProjectRootDir(jps_project_root_path.string());
    if(!building->LoadGeometry(path.string())) {
        return false;
    }
    if(!building->InitGeometry()) {
        return false;
    }

    double captionsColor = 0; // red
    int room_id          = -1;
    int subroom_id       = -1;
    for(auto && itr_room : building->GetAllRooms()) {
        room_id++;
        for(auto && itr_subroom : itr_room.second->GetAllSubRooms()) {
            subroom_id++;
            std::string room_caption = itr_room.second->GetCaption() + "_RId_" +
                                       QString::number(itr_room.first).toStdString();
            std::string subroom_caption = itr_subroom.second->GetCaption() + "_RId_" +
                                          QString::number(itr_room.first).toStdString();
            auto geometry = std::shared_ptr<FacilityGeometry>(
                new FacilityGeometry(itr_subroom.second->GetType(), room_caption, subroom_caption));

            int currentFloorPolyID = 0;
            int currentObstPolyID  = 0;

            // Setup the points
            VTK_CREATE(vtkPoints, floor_points);
            VTK_CREATE(vtkPoints, obstacles_points);
            // Add the polygon to a list of polygons
            VTK_CREATE(vtkCellArray, floor_polygons);
            VTK_CREATE(vtkCellArray, obstacles_polygons);

            SubRoom * sub = itr_subroom.second.get();

            std::vector<Point> poly = sub->GetPolygon();
            if(sub->IsClockwise() == true) {
                std::reverse(poly.begin(), poly.end());
            }

            // Create the polygon
            VTK_CREATE(vtkPolygon, polygon);
            polygon->GetPointIds()->SetNumberOfIds(poly.size());

            for(unsigned int s = 0; s < poly.size(); s++) {
                floor_points->InsertNextPoint(
                    poly[s]._x * FAKTOR, poly[s]._y * FAKTOR, sub->GetElevation(poly[s]) * FAKTOR);
                polygon->GetPointIds()->SetId(s, currentFloorPolyID++);
            }
            floor_polygons->InsertNextCell(polygon);

            // plot the walls only for not stairs
            const std::vector<Wall> & walls = sub->GetAllWalls();
            for(unsigned int w = 0; w < walls.size(); w++) {
                Point p1  = walls[w].GetPoint1();
                Point p2  = walls[w].GetPoint2();
                double z1 = sub->GetElevation(p1);
                double z2 = sub->GetElevation(p2);

                if(sub->GetType() == "stair") {
                    geometry->addStair(
                        p1._x * FAKTOR,
                        p1._y * FAKTOR,
                        z1 * FAKTOR,
                        p2._x * FAKTOR,
                        p2._y * FAKTOR,
                        z2 * FAKTOR);
                } else {
                    geometry->addWall(
                        p1._x * FAKTOR,
                        p1._y * FAKTOR,
                        z1 * FAKTOR,
                        p2._x * FAKTOR,
                        p2._y * FAKTOR,
                        z2 * FAKTOR);
                }
            }

            // insert the subroom caption
            std::string caption = itr_room.second->GetCaption() + " ( " +
                                  QString::number(sub->GetSubRoomID()).toStdString() + " ) ";
            const Point & p = sub->GetCentroid();
            double z        = sub->GetElevation(p);
            double pos[3]   = {p._x * FAKTOR, p._y * FAKTOR, z * FAKTOR};
            geometry->addObjectLabel(pos, pos, caption, captionsColor);

            // plot the obstacles
            for(auto obst : sub->GetAllObstacles()) {
                for(auto wall : obst->GetAllWalls()) {
                    Point p1  = wall.GetPoint1();
                    Point p2  = wall.GetPoint2();
                    double z1 = sub->GetElevation(p1);
                    double z2 = sub->GetElevation(p2);
                    geometry->addWall(
                        p1._x * FAKTOR,
                        p1._y * FAKTOR,
                        z1 * FAKTOR,
                        p2._x * FAKTOR,
                        p2._y * FAKTOR,
                        z2 * FAKTOR);
                }
                // add the obstacle caption
                const Point & p = obst->GetCentroid();
                double z        = sub->GetElevation(p);
                double pos[3]   = {p._x * FAKTOR, p._y * FAKTOR, z * FAKTOR};
                geometry->addObjectLabel(pos, pos, obst->GetCaption(), captionsColor);

                // add a special texture to the obstacles
                auto poly = obst->GetPolygon();
                // Create the polygon
                VTK_CREATE(vtkPolygon, polygon);
                polygon->GetPointIds()->SetNumberOfIds(poly.size());

                for(unsigned int s = 0; s < poly.size(); s++) {
                    obstacles_points->InsertNextPoint(
                        poly[s]._x * FAKTOR,
                        poly[s]._y * FAKTOR,
                        sub->GetElevation(poly[s]) * FAKTOR);
                    polygon->GetPointIds()->SetId(s, currentObstPolyID++);
                }
                obstacles_polygons->InsertNextCell(polygon);
            }

            // Create a PolyData to represent the floor
            VTK_CREATE(vtkPolyData, floorPolygonPolyData);
            floorPolygonPolyData->SetPoints(floor_points);
            floorPolygonPolyData->SetPolys(floor_polygons);
            geometry->addFloor(floorPolygonPolyData);

            // Create a PolyData to represen the obstacles
            VTK_CREATE(vtkPolyData, obstPolygonPolyData);
            obstPolygonPolyData->SetPoints(obstacles_points);
            obstPolygonPolyData->SetPolys(obstacles_polygons);
            geometry->addObstacles(obstPolygonPolyData);

            // add the crossings
            for(auto && cr : itr_subroom.second->GetAllCrossings()) {
                Point p1  = cr->GetPoint1();
                Point p2  = cr->GetPoint2();
                double z1 = cr->GetSubRoom1()->GetElevation(p1);
                double z2 = cr->GetSubRoom1()->GetElevation(p2);
                geometry->addNavLine(
                    p1._x * FAKTOR,
                    p1._y * FAKTOR,
                    z1 * FAKTOR,
                    p2._x * FAKTOR,
                    p2._y * FAKTOR,
                    z2 * FAKTOR);

                const Point & p = cr->GetCentre();
                double pos[3]   = {p._x * FAKTOR, p._y * FAKTOR, z1 * FAKTOR};
                geometry->addObjectLabel(
                    pos,
                    pos,
                    "nav_" + QString::number(cr->GetID()).toStdString() + "_" +
                        QString::number(cr->GetUniqueID()).toStdString(),
                    captionsColor);
            }

            // add the exits
            for(auto && tr : itr_subroom.second->GetAllTransitions()) {
                Point p1  = tr->GetPoint1();
                Point p2  = tr->GetPoint2();
                double z1 = 0;
                double z2 = 0;

                if(tr->GetSubRoom1()) // get elevation for both points
                {
                    z2 = tr->GetSubRoom1()->GetElevation(p2);
                    z1 = tr->GetSubRoom1()->GetElevation(p1);
                } else if(!tr->GetSubRoom2()) {
                    z2 = tr->GetSubRoom2()->GetElevation(p2);
                    z1 = tr->GetSubRoom2()->GetElevation(p1);
                } else
                    Log::Error(
                        "Can not calculate elevations for transition %d:%s. Both subrooms are not "
                        "defined\n",
                        tr->GetID(),
                        tr->GetCaption().c_str());

                geometry->addDoor(
                    p1._x * FAKTOR,
                    p1._y * FAKTOR,
                    z1 * FAKTOR,
                    p2._x * FAKTOR,
                    p2._y * FAKTOR,
                    z2 * FAKTOR);

                const Point & p = tr->GetCentre();
                double pos[3]   = {p._x * FAKTOR, p._y * FAKTOR, z1 * FAKTOR};
                geometry->addObjectLabel(
                    pos,
                    pos,
                    "door_" + QString::number(tr->GetID()).toStdString() + +"_" +
                        QString::number(tr->GetUniqueID()).toStdString(),
                    captionsColor);
            }

            geoFac.AddElement(room_id, subroom_id, geometry);
        }
    }
    return true;
}

QString extractSourceFileTXT(QString & filename)
{
    QString extracted_source_name = "";
    QFile file(filename);
    QString line;
    if(file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while(!in.atEnd()) {
            // look for a line with
            line = in.readLine();
            if(line.split(":").size() == 2) {
                if(line.split(":")[0].contains("sources", Qt::CaseInsensitive)) {
                    extracted_source_name = line.split(":")[1].simplified().remove(' ');
                    break;
                }
            }
        } // while
    }     // if open
    if(extracted_source_name == "") {
        Log::Warning("Could not extract source file!");
    }

    else
        Log::Info(
            "Extracted source from TXT file <%s>", extracted_source_name.toStdString().c_str());
    return extracted_source_name;
}

QString extractTrainTypeFileTXT(QString & filename)
{
    QString extracted_tt_name = "";
    QFile file(filename);
    QString line;
    if(file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while(!in.atEnd()) {
            // look for a line with
            line = in.readLine();
            if(line.split(":").size() == 2) {
                if(line.split(":")[0].contains("trainType", Qt::CaseInsensitive)) {
                    extracted_tt_name = line.split(":")[1].simplified().remove(' ');
                    break;
                }
            }
        } // while
    }     // if open
    if(extracted_tt_name == "") {
        Log::Warning("Could not extract trainType file!");
    }

    else
        Log::Info(
            "Extracted trainType from TXT file <%s>", extracted_tt_name.toStdString().c_str());
    return extracted_tt_name;
}

QString extractTrainTimeTableFileTXT(QString & filename)
{
    QString extracted_ttt_name = "";
    QFile file(filename);
    QString line;
    if(file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while(!in.atEnd()) {
            // look for a line with
            line = in.readLine();
            if(line.split(":").size() == 2) {
                if(line.split(":")[0].contains("trainTimeTable", Qt::CaseInsensitive)) {
                    extracted_ttt_name = line.split(":")[1].simplified().remove(' ');
                    break;
                }
            }
        } // while
    }     // if open
    if(extracted_ttt_name == "") {
        Log::Warning("Could not extract trainTimeTable file!");
    }

    else
        Log::Info(
            "Extracted trainTimeTable from TXT file <%s>",
            extracted_ttt_name.toStdString().c_str());
    return extracted_ttt_name;
}


QString extractGoalFileTXT(QString & filename)
{
    QString extracted_goal_name = "";
    QFile file(filename);
    QString line;
    if(file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while(!in.atEnd()) {
            // look for a line with
            line = in.readLine();
            if(line.split(":").size() == 2) {
                if(line.split(":")[0].contains("goals", Qt::CaseInsensitive)) {
                    extracted_goal_name = line.split(":")[1].simplified().remove(' ');
                    break;
                }
            }
        } // while
    }     // if open
    if(extracted_goal_name == "") {
        Log::Warning("Could not extract goal file!");
    }

    else
        Log::Info("Extracted goal from TXT file <%s>", extracted_goal_name.toStdString().c_str());
    return extracted_goal_name;
}


QString extractGeometryFilenameTXT(QString & filename)
{
    QString extracted_geo_name = "";
    QFile file(filename);
    QString line;
    if(file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while(!in.atEnd()) {
            // look for a line with
            line = in.readLine();
            if(line.split(":").size() == 2) {
                if(line.split(":")[0].contains("geometry", Qt::CaseInsensitive)) {
                    extracted_geo_name = line.split(":")[1].simplified().remove(' ');
                    break;
                }
            }
        } // while
    }     // if open
    if(extracted_geo_name == "") {
        Log::Warning("Could not extract geometry file!");
        //          extracted_geo_name = "geo.xml";
    }

    else
        Log::Info(
            "Extracted geometry from TXT file <%s>", extracted_geo_name.toStdString().c_str());
    return extracted_geo_name;
}

bool ParseTxtFormat(const QString & fileName, SyncData * dataset, double * fps)
{
    Log::Info("parsing txt trajectory <%s> ", fileName.toStdString().c_str());
    *fps = 16; // default value
    QFile inputFile(fileName);
    if(inputFile.open(QIODevice::ReadOnly)) {
        QTextStream in(&inputFile);
        int lastFrameID = -1;
        int maxFrame    = 1000;
        // initialize the process dialog
        QProgressDialog progressDialog("Simulation", "Cancel", 1, maxFrame, NULL);
        progressDialog.setModal(true);
        progressDialog.setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        progressDialog.setFixedSize(400, 100);
        progressDialog.setLabelText("<h3>Loading...</h3>");
        QList<QPushButton *> buttons = progressDialog.findChildren<QPushButton *>();
        buttons.at(0)->hide(); // that is the cancel button
        progressDialog.setValue(1);
        progressDialog.show();

        double unitFactor = FAKTOR; // @todo: use correct unit
        int minFrame      = 0;
        while(!in.atEnd()) {
            QString line = in.readLine();
            if(line.startsWith("#")) // looking for framerate
            {
                if(line.split(":").size() == 2) {
                    if(line.split(":")[0].contains("framerate", Qt::CaseInsensitive)) {
                        *fps = line.split(":")[1].toDouble();
                        Log::Info("Frame rate  <%.0f>", *fps);
                    }
                }
                continue;
            }
            line = line.trimmed(); // remove whitespaces from
                                   // beginning and end
            QStringList pieces = line.split(QRegExp("\\s+"));

            double pos[3];
            double angle[3]  = {0, 0, 30};
            double radius[3] = {0.3 * FAKTOR, 0.3 * FAKTOR, 0.3 * FAKTOR};

            int agentID     = -1;
            int frameID     = -1;
            double color    = 155;
            static int once = 1;
            switch(pieces.size()) {
                case 5:
                    agentID = pieces[0].toInt();
                    frameID = pieces[1].toInt();
                    if(once) // first frame we get
                    {
                        minFrame = frameID;
                        once     = 0;
                    }

                    pos[0] = pieces[2].toDouble() * unitFactor;
                    pos[1] = pieces[3].toDouble() * unitFactor;
                    pos[2] = pieces[4].toDouble() * unitFactor;
                    break;

                case 9:
                case 10:
                case 11:
                case 12:
                case 13:
                case 14:
                    agentID   = pieces[0].toInt();
                    frameID   = pieces[1].toInt();
                    color     = pieces[8].toDouble();
                    pos[0]    = pieces[2].toDouble() * unitFactor;
                    pos[1]    = pieces[3].toDouble() * unitFactor;
                    pos[2]    = pieces[4].toDouble() * unitFactor;
                    radius[0] = pieces[5].toDouble() * unitFactor;
                    radius[1] = pieces[6].toDouble() * unitFactor;
                    angle[2]  = pieces[7].toDouble();
                    if(once) // first frame we get
                    {
                        minFrame = frameID;
                        Log::Info("minFrame =  %d\n", minFrame);
                        once = 0;
                    }
                    break;

                default:
                    // try to scan the line for the unit
                    if(line.contains("centimeter", Qt::CaseInsensitive) ||
                       line.contains("centimetre", Qt::CaseInsensitive)) {
                        unitFactor = 0.01;
                        Log::Info("unit centimetre detected");
                    } else if(
                        line.contains("meter", Qt::CaseInsensitive) ||
                        line.contains("metre", Qt::CaseInsensitive)) {
                        unitFactor = 1;
                        Log::Info("unit metre detected");
                    } else {
                        Log::Warning("Ignoring line: <%s>", line.toStdString().c_str());
                    }
                    continue; // next line
                    break;
            }
            frameID -= minFrame;
            FrameElement * element = new FrameElement(agentID - 1);
            element->SetPos(pos);
            element->SetOrientation(angle);
            element->SetRadius(radius);
            element->SetColor(color);
            element->SetMinFrame(minFrame);
            if(dataset->GetFrames().count(frameID) < 1) {
                Frame * frame = new Frame(frameID);
                frame->addElement(element);
                dataset->addFrame(frame);
            } else {
                dataset->GetFrames()[frameID]->addElement(element);
            }

            // a new frame is starting.
            // not longer necessary if you are using maps and frameid
            if(frameID != lastFrameID) {
                progressDialog.setValue(dataset->getSize());
                lastFrameID = frameID;
                QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            }
        }

        inputFile.close();
        Log::Info("%d frames added!", dataset->GetFrames().size());
        // construct the polydata
        for(const auto & frame : dataset->GetFrames()) {
            frame.second->ComputePolyData();
        }
    } else {
        Log::Error("could not open the file  <%s>", fileName.toStdString().c_str());
        return false;
    }
    return true;
}

bool LoadTrainTimetable(
    std::string Filename,
    std::map<int, std::shared_ptr<TrainTimeTable>> & trainTimeTables)
{
    TiXmlDocument docTTT(Filename);
    if(!docTTT.LoadFile()) {
        Log::Error("%s", docTTT.ErrorDesc());
        Log::Error("could not parse the train timetable file.");
        return false;
    }
    TiXmlElement * xTTT = docTTT.RootElement();
    if(!xTTT) {
        Log::Error("Root element does not exist in TTT file.");
        return false;
    }
    if(xTTT->ValueStr() != "train_time_table") {
        Log::Error("Parsing train timetable file. Root element value is not 'train_time_table'.");
        return false;
    }
    for(TiXmlElement * e = xTTT->FirstChildElement("train"); e;
        e                = e->NextSiblingElement("train")) {
        std::shared_ptr<TrainTimeTable> TTT = parseTrainTimeTableNode(e);
        if(TTT) { // todo: maybe get pointer to train
            if(trainTimeTables.count(TTT->id) != 0) {
                Log::Warning("Duplicate id for train time table found [%d]", TTT->id);
                exit(EXIT_FAILURE);
            }
            // get track start
            trainTimeTables[TTT->id] = TTT;
        } else {
            Log::Error("Something went south!\n");
        }
    }
    return true;
}


bool LoadTrainType(
    std::string Filename,
    std::map<std::string, std::shared_ptr<TrainType>> & trainTypes)
{
    TiXmlDocument docTT(Filename);
    if(!docTT.LoadFile()) {
        Log::Error("%s", docTT.ErrorDesc());
        Log::Error("could not parse the train type file.");
        return false;
    }
    TiXmlElement * xTT = docTT.RootElement();
    if(!xTT) {
        Log::Error("Root element does not exist in TT file.");
        return false;
    }
    if(xTT->ValueStr() != "train_type") {
        Log::Error("Parsing train type file. Root element value is not 'train_type'.");
        return false;
    }
    for(TiXmlElement * e = xTT->FirstChildElement("train"); e; e = e->NextSiblingElement("train")) {
        std::shared_ptr<TrainType> TT = parseTrainTypeNode(e);
        if(TT) {
            if(trainTypes.count(TT->_type.c_str()) != 0) {
                Log::Warning("Duplicate type for train found [%s]", TT->_type.c_str());
            }
            trainTypes[TT->_type] = TT;
        }
    }
    return true;
}


std::shared_ptr<TrainTimeTable> parseTrainTimeTableNode(TiXmlElement * e)
{
    Log::Info("Loading train time table NODE");
    // std::string caption = xmltoa(e->Attribute("caption"), "-1");
    int id              = xmltoi(e->Attribute("id"), -1);
    int track_id        = xmltoi(e->Attribute("track_id"), -1);
    double train_offset = xmltof(e->Attribute("train_offset"), -1);
    bool reversed       = false;
    double elevation    = 5; // dummy default value
    std::string in      = xmltoa(e->Attribute("reversed"), "false");
    std::transform(in.begin(), in.end(), in.begin(), ::tolower);
    if(in == "false") {
        reversed = false;
    } else if(in == "true") {
        reversed = true;
    } else {
        reversed = false;
    }
    std::string type     = xmltoa(e->Attribute("type"), "-1");
    int room_id          = xmltoi(e->Attribute("room_id"), -1);
    int subroom_id       = xmltoi(e->Attribute("subroom_id"), -1);
    float arrival_time   = xmltof(e->Attribute("arrival_time"), -1);
    float departure_time = xmltof(e->Attribute("departure_time"), -1);
    // @todo: check these values for correctness e.g. arrival < departure
    Log::Info("Train time table:");
    Log::Info("   id: %d", id);
    Log::Info("   type: %s", type.c_str());
    Log::Info("   room_id: %d", room_id);
    Log::Info("   subroom_id: %d", subroom_id);
    Log::Info("   track_id: %d", track_id);
    Log::Info("   train_offset: %.2f", train_offset);
    Log::Info("   arrival_time: %.2f", arrival_time);
    Log::Info("departure_time: %.2f", departure_time);
    std::shared_ptr<TrainTimeTable> trainTimeTab = std::make_shared<TrainTimeTable>(TrainTimeTable{
        id,
        type,
        room_id,
        subroom_id,
        arrival_time,
        departure_time,
        Point(0, 0),
        Point(0, 0),
        Point(0, 0),
        Point(0, 0),
        track_id,
        false,
        false,
        reversed,
        train_offset,
        elevation,
        vtkSmartPointer<vtkPolyDataMapper>::New(),
        vtkSmartPointer<vtkActor>::New(),
        vtkSmartPointer<vtkTextActor3D>::New(),
    });

    return trainTimeTab;
}
std::shared_ptr<TrainType> parseTrainTypeNode(TiXmlElement * node)
{
    Log::Info("Loading train type");

    std::string type = xmltoa(node->Attribute("type"), "NO_TYPE");
    if(type == "NO_TYPE") {
        Log::Warning("No train type name given. Use 'NO_TYPE' instead.");
    }
    Log::Info("type: {}", type.c_str());

    int agents_max = xmltoi(node->Attribute("agents_max"), std::numeric_limits<int>::max());
    if(agents_max == std::numeric_limits<int>::max()) {
        Log::Warning("No agents_max given. Set to default: {}.", agents_max);
    }
    Log::Info("max Agents: {}", agents_max);

    // Read and check if correct value
    double length = -std::numeric_limits<double>::infinity();
    if(const char * attribute = node->Attribute("length"); attribute) {
        if(double value = xmltof(attribute, -std::numeric_limits<double>::infinity());
           value >= 0.) {
            length = value;
        } else {
            Log::Warning(
                "{}: input for length should be non-negative {}. Skip entry.", type.c_str(), value);
            return nullptr;
        }
    } else {
        Log::Warning("{}: input for length not found. Skip entry.", type.c_str());
        return nullptr;
    }
    Log::Info("length: {}", length);


    std::vector<TrainDoor> doors;
    for(TiXmlElement * xDoor = node->FirstChildElement("door"); xDoor != nullptr;
        xDoor                = xDoor->NextSiblingElement("door")) {
        // Read distance and check if correct value
        double distance = -std::numeric_limits<double>::infinity();
        if(const char * attribute = xDoor->Attribute("distance"); attribute) {
            if(double value = xmltof(attribute, -std::numeric_limits<double>::infinity());
               value >= 0.) {
                distance = value;
            } else {
                Log::Warning(
                    "{}: input for distance should be non-negative {}. Skip entry.",
                    type.c_str(),
                    value);
                continue;
            }
        } else {
            Log::Warning("{}: input for distance not found. Skip entry.", type.c_str());
            continue;
        }

        // Read width and check if correct value
        double width = -std::numeric_limits<double>::infinity();
        if(const char * attribute = xDoor->Attribute("width"); attribute) {
            if(double value = xmltof(attribute, -std::numeric_limits<double>::infinity());
               value > 0.) {
                width = value;
            } else {
                Log::Warning(
                    "{}: input for width should be non-negative {}. Skip entry.",
                    type.c_str(),
                    value);
                continue;
            }
        } else {
            Log::Warning("{}: input for width not found. Skip entry.", type.c_str());
            continue;
        }

        // Read flow and check if correct value
        double flow = -std::numeric_limits<double>::infinity();
        if(const char * attribute = xDoor->Attribute("flow"); attribute) {
            if(double value = xmltof(attribute, -std::numeric_limits<double>::infinity());
               value > 0.) {
                flow = value;
            } else {
                Log::Warning(
                    "{}: input for flow should be >0 but is {:5.2}. Skip entry.",
                    type.c_str(),
                    value);
                continue;
            }
        }

        doors.emplace_back(TrainDoor{distance, width, flow});
    }

    if(doors.empty()) {
        Log::Error("Train {}: no doors given. Train will be ignored.", type.c_str());
        return nullptr;
    }

    Log::Info("number of doors: {}", doors.size());
    for(const auto & d : doors) {
        Log::Info(
            "Door:\tdistance: {%5.2f}\twidth: {%5.2f}\toutflow: {%5.2f}",
            d._distance,
            d._width,
            d._flow);
    }

    std::shared_ptr<TrainType> Type = std::make_shared<TrainType>(TrainType{
        type,
        agents_max,
        length,
        doors,
    });
    return Type;
}
} // namespace Parsing
