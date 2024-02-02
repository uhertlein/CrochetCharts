/****************************************************************************\
 Copyright (c) 2011-2014 Stitch Works Software
 Brian C. Milco <bcmilco@gmail.com>

 This file is part of Crochet Charts.

 Crochet Charts is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Crochet Charts is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with Crochet Charts. If not, see <http://www.gnu.org/licenses/>.

 \****************************************************************************/
#include "file_v1.h"

#include "debug.h"

#include "appinfo.h"

#include <QTextDocument>
#include <QFileInfo>
#include <QDir>

#include <QDataStream>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <QStringList>

#include "stitchlibrary.h"
#include "mainwindow.h"
#include "scene.h"
#include "ChartItemTools.h"

#include "crochettab.h"
#include "settings.h"

File_v1::File_v1(MainWindow* mw, FileFactory* parent)
    : File(mw, parent)
{
}

FileFactory::FileError
File_v1::load(QDataStream* stream)
{
    mInternalStitchSet = new StitchSet();
    mInternalStitchSet->isTemporary = true;
    mInternalStitchSet->stitchSetFileName = StitchLibrary::inst()->nextSetSaveFile();
    QString dest = mInternalStitchSet->stitchSetFileName;
    QFileInfo info(dest);
    QDir(info.path()).mkpath(info.path() + "/" + info.baseName());

    mInternalStitchSet->loadIcons(stream);

    QByteArray docData;
    *stream >> docData;

    QXmlStreamReader xmlStream(docData);

    if (xmlStream.hasError())
    {
        qWarning() << "Error loading saved file: " << xmlStream.errorString();
        return FileFactory::Err_GettingFileContents;
    }

    while (!xmlStream.atEnd() && !xmlStream.hasError())
    {
        xmlStream.readNext();
        if (xmlStream.isStartElement())
        {
            QString name = xmlStream.name().toString();

            if (name == "colors")
            {
                loadColors(&xmlStream);
            }
            else if (name == "chart")
            {
                loadChart(&xmlStream);
            }
            else if (name == "stitch_set")
            {
                mInternalStitchSet->loadXmlStitchSet(&xmlStream, true);
            }
        }
    }

    StitchLibrary::inst()->addStitchSet(mInternalStitchSet);

    return FileFactory::No_Error;
}

FileFactory::FileError
File_v1::save(QDataStream* stream)
{
    *stream << (qint32)FileFactory::Version_1_0;
    stream->setVersion(QDataStream::Qt_4_7);

    if (!mInternalStitchSet)
    {
        mInternalStitchSet = new StitchSet();
        mInternalStitchSet->isTemporary = true;
        mInternalStitchSet->stitchSetFileName = StitchLibrary::inst()->nextSetSaveFile();
        StitchLibrary::inst()->addStitchSet(mInternalStitchSet);
    }
    else
    {
        mInternalStitchSet->clearStitches();
    }

    // start xml save...
    QString* data = new QString();
    QXmlStreamWriter xmlStream(data);
    xmlStream.setAutoFormatting(true);
    xmlStream.writeStartDocument();

    xmlStream.writeStartElement("pattern");  // start pattern
    // TODO: dont need to set the version when saving into a binary file.
    xmlStream.writeAttribute("version", QString::number(FileFactory::Version_1_2));

    // create the StitchSet then save the icons.
    saveCustomStitches(&xmlStream);
    mInternalStitchSet->saveIcons(stream);

    saveColors(&xmlStream);

    saveCharts(&xmlStream);
    xmlStream.writeEndElement();

    xmlStream.writeEndDocument();

    // put xml into binary file.
    *stream << data->toUtf8();

    delete data;
    data = nullptr;

    return FileFactory::No_Error;
}

void
File_v1::loadColors(QXmlStreamReader* stream)
{
    MainWindow* mw = qobject_cast<MainWindow*>(mParent->mParent);

    mw->patternColors().clear();

    while (!(stream->isEndElement() && stream->name() == "colors"))
    {
        stream->readNext();
        QString tag = stream->name().toString();

        if (tag == "color")
        {
            QMap<QString, qint64> properties;
            properties.insert("count", 0);  // count = 0 because we haven't added any cells yet.
            properties.insert("added",
                              (qint64)stream->attributes().value("added").toString().toLongLong());
            mw->mPatternColors.insert(stream->readElementText(), properties);
        }
    }
}

void
File_v1::loadChart(QXmlStreamReader* stream)
{
    MainWindow* mw = qobject_cast<MainWindow*>(mParent->mParent);
    CrochetTab* tab = nullptr;
    QString tabName = "", defaultSt = "";

    while (!(stream->isEndElement() && stream->name() == "chart"))
    {
        stream->readNext();
        QString tag = stream->name().toString();

        if (tag == "name")
        {
            tabName = stream->readElementText();
        }
        else if (tag == "style")
        {
            int style = stream->readElementText().toInt();
            tab = mw->createTab((Scene::ChartStyle)style);

            mParent->mTabWidget->addTab(tab, "");
            mParent->mTabWidget->widget(mParent->mTabWidget->indexOf(tab))->hide();
        }
        else if (tag == "defaultSt")
        {
            defaultSt = stream->readElementText();
            Q_ASSERT(tab != nullptr);
            tab->scene()->mDefaultStitch = defaultSt;
        }
        else if (tag == "chartCenter")
        {
            qreal x = stream->attributes().value("x").toString().toDouble();
            qreal y = stream->attributes().value("y").toString().toDouble();

            stream->readElementText();
            Q_ASSERT(tab != nullptr);
            tab->blockSignals(true);
            tab->setShowChartCenter(true);
            tab->scene()->mCenterSymbol->setPos(x, y);
            tab->blockSignals(false);
        }
        else if (tag == "grid")
        {
            Q_ASSERT(tab != nullptr);
            loadGrid(stream, tab->scene());
        }
        else if (tag == "rowSpacing")
        {
            qreal width = stream->attributes().value("width").toString().toDouble();
            qreal height = stream->attributes().value("height").toString().toDouble();
            Q_ASSERT(tab != nullptr);
            tab->scene()->mDefaultSize.setHeight(height);
            tab->scene()->mDefaultSize.setWidth(width);
        }
        else if (tag == "cell")
        {
            Q_ASSERT(tab != nullptr);
            loadCell(tab, stream);
        }
        else if (tag == "indicator")
        {
            Q_ASSERT(tab != nullptr);
            loadIndicator(tab, stream);
        }
        else if (tag == "group")
        {
            stream->readElementText().toInt();
            // create an empty group for future use.
            QList<QGraphicsItem*> items;
            Q_ASSERT(tab != nullptr);
            tab->scene()->group(items);
        }
    }

    Q_ASSERT(tab != nullptr);
    tab->updateRows();
    int index = mParent->mTabWidget->indexOf(tab);
    mParent->mTabWidget->setTabText(index, tabName);
    mParent->mTabWidget->widget(mParent->mTabWidget->indexOf(tab))->show();
    tab->scene()->updateSceneRect();

    if (tab->scene()->hasChartCenter())
    {
        tab->view()->centerOn(tab->scene()->mCenterSymbol->sceneBoundingRect().center());
    }
    else
    {
        tab->view()->centerOn(tab->scene()->itemsBoundingRect().center());
    }
}

void
File_v1::loadGrid(QXmlStreamReader* stream, Scene* scene)
{
    while (!(stream->isEndElement() && stream->name() == "grid"))
    {
        stream->readNext();
        QString tag = stream->name().toString();

        if (tag == "row")
        {
            int cols = stream->readElementText().toInt();
            QList<Cell*> row;
            for (int i = 0; i < cols; ++i)
            {
                row.append(0);
            }
            scene->grid.append(row);
        }
    }
}

void
File_v1::loadIndicator(CrochetTab* tab, QXmlStreamReader* stream)
{
    Indicator* i = new Indicator();

    qreal x = 0, y = 0;
    QString textColor, bgColor;
    QString text;

    while (!(stream->isEndElement() && stream->name() == "indicator"))
    {
        stream->readNext();
        QString tag = stream->name().toString();

        if (tag == "x")
        {
            x = stream->readElementText().toDouble();
        }
        else if (tag == "y")
        {
            y = stream->readElementText().toDouble();
        }
        else if (tag == "text")
        {
            text = stream->readElementText();
            // the text might be html formatted in old saves, so we need to strip it. A regex could
            // work, but is hard to make performant with inline css, and wouldn't work well with text
            // that has brackets in it.
            QTextDocument doc;
            doc.setHtml(text);
            text = doc.toPlainText();
        }
        else if (tag == "textColor")
        {
            textColor = stream->readElementText();
        }
        else if (tag == "bgColor")
        {
            bgColor = stream->readElementText();
        }
    }

    tab->scene()->addItem(i);
    i->setPos(x, y);
    i->setText(text);
    i->setTextColor(textColor);
    i->setBgColor(bgColor);
    ChartItemTools::recalculateTransformations(i);
}

void
File_v1::loadCell(CrochetTab* tab, QXmlStreamReader* stream)
{
    Cell* c = new Cell();
    Stitch* s = nullptr;
    int row = -1, column = -1;
    int group = -1;
    QString bgColor;
    QPointF position(0.0, 0.0);
    QPointF pivotPoint;
    qreal angle = 0.0;
    QPointF scale = QPointF(1.0, 1.0);
    QTransform transform;
    qreal m11 = 1, m12 = 0, m13 = 0, m21 = 0, m22 = 1, m23 = 0, m31 = 0, m32 = 0, m33 = 1;

    while (!(stream->isEndElement() && stream->name() == "cell"))
    {
        stream->readNext();
        QString tag = stream->name().toString();

        if (tag == "stitch")
        {
            QString st = stream->readElementText();
            s = StitchLibrary::inst()->findStitch(st, true);
        }
        else if (tag == "grid")
        {
            row = stream->attributes().value("row").toString().toDouble();
            column = stream->attributes().value("column").toString().toDouble();
            stream->readElementText();
        }
        else if (tag == "color")
        {
            bgColor = stream->readElementText();
        }
        else if (tag == "position")
        {
            position.rx() = stream->attributes().value("x").toString().toDouble();
            position.ry() = stream->attributes().value("y").toString().toDouble();
            stream->readElementText();
        }
        else if (tag == "angle")
        {
            angle = stream->readElementText().toDouble();
        }
        else if (tag == "scale")
        {
            scale.rx() = stream->attributes().value("x").toString().toDouble();
            scale.ry() = stream->attributes().value("y").toString().toDouble();
            stream->readElementText();
        }
        else if (tag == "pivotPoint")
        {
            pivotPoint.rx() = stream->attributes().value("x").toString().toDouble();
            pivotPoint.ry() = stream->attributes().value("y").toString().toDouble();
            stream->readElementText();
        }
        else if (tag == "group")
        {
            group = stream->readElementText().toInt();
        }
        else if (tag == "transformation")
        {
            m11 = stream->attributes().value("m11").toString().toDouble();
            m12 = stream->attributes().value("m12").toString().toDouble();
            m13 = stream->attributes().value("m13").toString().toDouble();
            m21 = stream->attributes().value("m21").toString().toDouble();
            m22 = stream->attributes().value("m22").toString().toDouble();
            m23 = stream->attributes().value("m23").toString().toDouble();
            m31 = stream->attributes().value("m31").toString().toDouble();
            m32 = stream->attributes().value("m32").toString().toDouble();
            m33 = stream->attributes().value("m33").toString().toDouble();
            transform.setMatrix(m11, m12, m13, m21, m22, m23, m31, m32, m33);
            stream->readElementText();
        }
    }

    tab->scene()->addItem(c);

    if (row > -1 && column > -1)
    {
        c->setStitch(s);
        if (row % 2)
        {
            QString colorName = Settings::inst()->value("stitchAlternateColor").toString();
            c->setColor(QColor(colorName));
        }
        tab->scene()->grid[row].replace(column, c);
        c->setZValue(100);
    }
    else
    {
        c->setStitch(s);
        c->setZValue(10);
    }

    c->setTransform(transform);
    c->setPos(position);
    c->setBgColor(QColor(bgColor));
    c->setTransformOriginPoint(pivotPoint);
    c->setRotation(angle);
    ChartItemTools::recalculateTransformations(c);

    if (group != -1)
        tab->scene()->addToGroup(group, c);
}

void
File_v1::saveCustomStitches(QXmlStreamWriter* stream)
{
    CrochetTab* tab = qobject_cast<CrochetTab*>(mTabWidget->widget(0));

    // FIXME: fileName includes the whole path.
    mInternalStitchSet->setName(QString("[%1]").arg(QFileInfo(mParent->fileName).fileName()));
    QStringList stitches = tab->patternStitches()->keys();

    foreach (QString st, stitches)
    {
        Stitch* s = StitchLibrary::inst()->findStitch(st);
        if (s)
            mInternalStitchSet->addStitch(s);
    }

    mInternalStitchSet->saveXmlStitchSet(stream, true);
}

bool
File_v1::saveCharts(QXmlStreamWriter* stream)
{
    int tabCount = mTabWidget->count();

    for (int i = 0; i < tabCount; ++i)
    {
        CrochetTab* tab = qobject_cast<CrochetTab*>(mTabWidget->widget(i));
        if (!tab)
            continue;

        stream->writeStartElement("chart");  // start chart

        stream->writeTextElement("name", mTabWidget->tabText(i));

        stream->writeTextElement("style", QString::number(tab->mChartStyle));
        stream->writeTextElement("defaultSt", tab->scene()->mDefaultStitch);

        bool showCenter = tab->scene()->showChartCenter();
        if (showCenter)
        {
            stream->writeStartElement("chartCenter");
            stream->writeAttribute("x",
                                   QString::number(tab->scene()->mCenterSymbol->scenePos().x()));
            stream->writeAttribute("y",
                                   QString::number(tab->scene()->mCenterSymbol->scenePos().y()));
            stream->writeEndElement();  // end chart center
        }

        stream->writeStartElement("rowSpacing");
        stream->writeAttribute("width", QString::number(tab->scene()->mDefaultSize.width()));
        stream->writeAttribute("height", QString::number(tab->scene()->mDefaultSize.height()));
        stream->writeEndElement();  // row spacing

        if (tab->scene()->rowCount() >= 1 && tab->scene()->maxColumnCount() >= 1)
        {
            stream->writeStartElement("grid");
            int rowCount = tab->scene()->rowCount();
            for (int i = 0; i < rowCount; ++i)
            {
                int colCount = tab->scene()->columnCount(i);
                stream->writeTextElement("row", QString::number(colCount));  // row, columns.
            }
            stream->writeEndElement();  // end grid.
        }

        foreach (ItemGroup* g, tab->scene()->mGroups)
        {
            stream->writeTextElement("group", QString::number(tab->scene()->mGroups.indexOf(g)));
        }

        foreach (QGraphicsItem* item, tab->scene()->items())
        {
            Cell* c = qgraphicsitem_cast<Cell*>(item);
            if (!c)
                continue;

            stream->writeStartElement("cell");  // start cell
            stream->writeTextElement("stitch", c->stitch()->name());

            // if the stitch is on the grid save the grid position.
            QPoint pt = tab->scene()->indexOf(c);
            if (pt != QPoint(-1, -1))
            {
                stream->writeStartElement("grid");
                stream->writeAttribute("row", QString::number(pt.y()));
                stream->writeAttribute("column", QString::number(pt.x()));
                stream->writeEndElement();  // grid
            }

            bool isGrouped = c->parentItem() ? true : false;
            ItemGroup* g = nullptr;
            if (isGrouped)
            {
                g = qgraphicsitem_cast<ItemGroup*>(c->parentItem());
                int groupNum = tab->scene()->mGroups.indexOf(g);
                stream->writeTextElement("group", QString::number(groupNum));

                // ungroup the items so that we can
                // take an acurate position of each stitch.
                g->removeFromGroup(c);
            }

            stream->writeStartElement("position");
            stream->writeAttribute("x", QString::number(c->pos().x()));
            stream->writeAttribute("y", QString::number(c->pos().y()));
            stream->writeEndElement();  // position

            stream->writeStartElement("transformation");
            QTransform trans = c->transform();

            stream->writeAttribute("m11", QString::number(trans.m11()));
            stream->writeAttribute("m12", QString::number(trans.m12()));
            stream->writeAttribute("m13", QString::number(trans.m13()));
            stream->writeAttribute("m21", QString::number(trans.m21()));
            stream->writeAttribute("m22", QString::number(trans.m22()));
            stream->writeAttribute("m23", QString::number(trans.m23()));
            stream->writeAttribute("m31", QString::number(trans.m31()));
            stream->writeAttribute("m32", QString::number(trans.m32()));
            stream->writeAttribute("m33", QString::number(trans.m33()));
            stream->writeEndElement();  // transformation

            // in case we haven't closed the
            // application we need to regroup the items.
            if (isGrouped)
                g->addToGroup(c);

            stream->writeTextElement("color", c->bgColor().name());
            stream->writeTextElement("angle", QString::number(c->rotation()));

            stream->writeStartElement("scale");
            stream->writeAttribute("x", QString::number(c->transform().m11()));
            stream->writeAttribute("y", QString::number(c->transform().m22()));
            stream->writeEndElement();  // end scale

            stream->writeStartElement("pivotPoint");
            stream->writeAttribute("x", QString::number(c->transformOriginPoint().x()));
            stream->writeAttribute("y", QString::number(c->transformOriginPoint().y()));
            stream->writeEndElement();  // end pivotPoint

            stream->writeEndElement();  // end cell
        }

        foreach (Indicator* i, tab->scene()->indicators())
        {
            stream->writeStartElement("indicator");

            stream->writeTextElement("x", QString::number(i->scenePos().x()));
            stream->writeTextElement("y", QString::number(i->scenePos().y()));
            stream->writeTextElement("text", i->text());
            stream->writeTextElement("textColor", i->textColor().name());
            stream->writeTextElement("bgColor", i->bgColor().name());

            stream->writeEndElement();  // end indicator
        }

        stream->writeEndElement();  // end chart
    }

    return true;
}

void
File_v1::saveColors(QXmlStreamWriter* stream)
{
    stream->writeStartElement("colors");  // start colors
    MainWindow* mw = static_cast<MainWindow*>(mParent->mParent);

    QStringList keys = mw->patternColors().keys();

    foreach (QString key, keys)
    {
        stream->writeStartElement("color");
        stream->writeAttribute("added",
                               QString::number(mw->patternColors().value(key).value("added")));
        stream->writeCharacters(key);
        stream->writeEndElement();  // end color
    }

    stream->writeEndElement();  // end colors
}

void
File_v1::cleanUp()
{
    if (mInternalStitchSet)
        StitchLibrary::inst()->removeSet(mInternalStitchSet);
}
