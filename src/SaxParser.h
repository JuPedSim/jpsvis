/**
 * @file    SaxParser.h
 * @author  Ulrich Kemloh <kemlohulrich@gmail.com>
 * @version 0.1
 * Created on: 6 Sep 2010
 * Copyright (C) <2009-2010>
 *
 * @section LICENSE
 * This file is part of OpenPedSim.
 *
 * OpenPedSim is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * OpenPedSim is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenPedSim. If not, see <http://www.gnu.org/licenses/>.
 *
 * @section DESCRIPTION
 *
 *
 *
 */

#ifndef SAXPARSER_H_
#define SAXPARSER_H_

#include <QtXml>
#include <QTreeWidget>
#include <vector>
#include "SyncData.h"
#include"geometry/GeometryFactory.h"



//forwarded classes
class JPoint;
class TrajectoryPoint;
class FrameElement;
class SyncData;
class GeometryFactory;


class SaxParser: public QXmlDefaultHandler
{
public:
    SaxParser(GeometryFactory& geoFac, SyncData& _dataset, double * fps);
    virtual ~SaxParser();
    bool startElement(const QString &namespaceURI,
                      const QString &localName,
                      const QString &qName,
                      const QXmlAttributes &attributes);
    bool endElement(const QString &namespaceURI,
                    const QString &localName,
                    const QString &qName);
    bool characters(const QString &str);
    bool fatalError(const QXmlParseException &exception);
    bool attributeDecl(const QString& eName,
                       const QString& aName,
                       const QString& type,
                       const QString& valueDefault,
                       const QString& value);

    /// provided for convenience and will be removed in the next version
    static bool parseGeometryJPS(QString content, GeometryFactory& geo);

    /// provided for convenience and will be removed in the next version
    static void parseGeometryXMLV04(QString content, GeometryFactory& geo);

    /// provided for convenience and will be removed in the next version
    static void parseGeometryTRAV(QString fileName, GeometryFactory& geoFac, QDomNode geoNode=QDomNode());

    /// take a large file and find the geometry file location.
    static QString extractGeometryFilename(QString& filename);

    /// parse the txt file format
    static bool ParseTxtFormat(const QString& fileName, SyncData* dataset, double * fps);

    /// parse a vtk file
    static bool ParseGradientFieldVTK(QString fileName, GeometryFactory& geoFac);

private:
    //clear the mo
    void clearPoints();
    void InitHeader(int major, int minor, int patch);

private:
    GeometryFactory& _geoFactory;
    std::shared_ptr<FacilityGeometry> _geometry;
    SyncData& _dataset;
    double* _para;
    QString _currentText;
    int _currentFrameID=-1;
    QStringList _initialPedestriansColors;
    QStringList _initialPedestriansHeights;
    std::vector<JPoint *> _currentPointsList;
    std::vector<FrameElement *> _currentFrame;
    bool _parsingWalls;
    bool _parsingCrossings;

    //wall and door and hlines parameters
    double _thickness;
    double _height;
    double _color;

    //actual caption of door/wall/hlines
    QString _caption;

    //header dependant variables
    QString _jps_xPos;
    QString _jps_yPos;
    QString _jps_zPos;
    QString _jps_xVel;
    QString _jps_yVel;
    QString _jps_zVel;
    QString _jps_radiusA;
    QString _jps_radiusB;
    QString _jps_ellipseOrientation;
    QString _jps_ellipseColor;
};

#endif /* SAXPARSER_H_ */
