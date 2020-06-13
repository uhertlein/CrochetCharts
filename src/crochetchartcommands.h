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
#ifndef CROCHETCHARTCOMMANDS_H
#define CROCHETCHARTCOMMANDS_H

#include <QUndoCommand>

#include "cell.h"
#include "ChartImage.h"
#include "scene.h"

class SetIndicatorText : public QUndoCommand
{
public:
    enum
    {
        Id = 1100
    };

    SetIndicatorText(Indicator* i, QString otext, QString ntext, QUndoCommand* parent = nullptr);

    void undo();
    void redo();

    int
    id() const
    {
        return Id;
    }

    static void setText(Indicator* i, QString text);

private:
    QString oldText;
    QString newText;
    Indicator* i = nullptr;
};

class SetCellStitch : public QUndoCommand
{
public:
    enum
    {
        Id = 1110
    };

    SetCellStitch(Cell* cell, QString newSt, QUndoCommand* parent = nullptr);

    void undo();
    void redo();

    int
    id() const
    {
        return Id;
    }

    static void setStitch(Cell* cell, QString stitch);

private:
    QString oldStitch;
    QString newStitch;
    Cell* c = nullptr;
};

class SetChartZLayer : public QUndoCommand
{
public:
    enum
    {
        Id = 1120
    };
    SetChartZLayer(ChartImage* ci, const QString& zlayer, QUndoCommand* parent = nullptr);

    void undo();
    void redo();

    int
    id() const
    {
        return Id;
    }

    static void setZLayer(ChartImage* ci, const QString& layer);

private:
    QString newLayer;
    QString oldLayer;
    ChartImage* ci = nullptr;
};

class SetChartImagePath : public QUndoCommand
{
public:
    enum
    {
        Id = 1130
    };
    SetChartImagePath(ChartImage* ci, const QString& path, QUndoCommand* parent = nullptr);

    void undo();
    void redo();

    int
    id() const
    {
        return Id;
    }

    static void setPath(ChartImage* ci, const QString& path);

private:
    QString newPath;
    QString oldPath;
    ChartImage* ci = nullptr;
};

class SetCellBgColor : public QUndoCommand
{
public:
    enum
    {
        Id = 1140
    };

    SetCellBgColor(Cell* cell, QColor newCl, QUndoCommand* parent = nullptr);

    void undo();
    void redo();

    int
    id() const
    {
        return Id;
    }

    static void setBgColor(Cell* cell, QColor color);

private:
    QColor oldColor;
    QColor newColor;
    Cell* c = nullptr;
};

class SetCellColor : public QUndoCommand
{
public:
    enum
    {
        Id = 1150
    };

    SetCellColor(Cell* cell, QColor newCl, QUndoCommand* parent = nullptr);

    void undo();
    void redo();

    int
    id() const
    {
        return Id;
    }

    static void setColor(Cell* cell, QColor color);

private:
    QColor oldColor;
    QColor newColor;
    Cell* c = nullptr;
};

class SetItemRotation : public QUndoCommand
{
public:
    enum
    {
        Id = 1160
    };

    SetItemRotation(QGraphicsItem* item, qreal oldAngl, QPointF pivotPt, QUndoCommand* parent = nullptr);

    void undo();
    void redo();

    int
    id() const
    {
        return Id;
    }

    static void setRotation(QGraphicsItem* item, qreal angle, QPointF pivot);

private:
    QGraphicsItem* i = nullptr;
    qreal oldAngle;
    qreal newAngle;
    QPointF pvtPt;
};

/**
 * No matter what the angle is convert it to a number between 0, and 360 degrees.
 */
static void
qNormalizeAngle(qreal& angle)
{
    while (angle < 0.0)
        angle += 360.0;
    while (angle > 360.0)
        angle -= 360.0;
}

class SetSelectionRotation : public QUndoCommand
{
public:
    enum
    {
        Id = 1170
    };

    SetSelectionRotation(Scene* scene,
                         QList<QGraphicsItem*> itms,
                         qreal degrees,
                         QUndoCommand* parent = nullptr);

    void undo();
    void redo();

    int
    id() const
    {
        return Id;
    }

    static void rotate(Scene* scene,
                       qreal degrees,
                       QList<QGraphicsItem*> items,
                       QPointF pivotPoint);

private:
    QList<QGraphicsItem*> items;
    qreal newAngle;

    QPointF pivotPoint;
    Scene* s = nullptr;
};

class SetItemCoordinates : public QUndoCommand
{
public:
    enum
    {
        Id = 1180
    };

    SetItemCoordinates(QGraphicsItem* item, QPointF oldPos, QUndoCommand* parent = nullptr);

    void undo();
    void redo();

    int
    id() const
    {
        return Id;
    }

    static void setPosition(QGraphicsItem* item, QPointF position);

private:
    QPointF oldCoord;
    QPointF newCoord;
    QGraphicsItem* i = nullptr;
};

class SetItemScale : public QUndoCommand
{
public:
    enum
    {
        Id = 1190
    };

    SetItemScale(QGraphicsItem* item, QPointF oldScle, QPointF pivotPt, QUndoCommand* parent = nullptr);

    void undo();
    void redo();

    int
    id() const
    {
        return Id;
    }

    static void setScale(QGraphicsItem* item, QPointF scale, QPointF pivot);

private:
    QPointF oldScale;
    QPointF newScale;
    QPointF mPivot;

    QGraphicsItem* i = nullptr;
};

class AddItem : public QUndoCommand
{
public:
    enum
    {
        Id = 1200
    };

    AddItem(Scene* scene, QGraphicsItem* item, QUndoCommand* parent = nullptr);
    ~AddItem();
    void redo();
    void undo();

    int
    id() const
    {
        return Id;
    }

    static void add(Scene* scene, QGraphicsItem* item);

private:
    QGraphicsItem* i = nullptr;
    Scene* s = nullptr;
};

class RemoveItem : public QUndoCommand
{
public:
    enum
    {
        Id = 1210
    };

    RemoveItem(Scene* scene, QGraphicsItem* item, QUndoCommand* parent = nullptr);

    void redo();
    void undo();

    int
    id() const
    {
        return Id;
    }

    static void remove(Scene* scene, QGraphicsItem* item);

private:
    QGraphicsItem* i = nullptr;
    QPointF position;

    Scene* s = nullptr;
};

class RemoveItems : public QUndoCommand
{
public:
    enum
    {
        Id = 1220
    };

    RemoveItems(Scene* scene, QList<QGraphicsItem*> items, QUndoCommand* parent = nullptr);
    ~RemoveItems();

    void redo();
    void undo();

    int
    id() const
    {
        return Id;
    }

private:
    QList<QGraphicsItem*> items;
    QGraphicsItemGroup* removegroup = nullptr;

    Scene* s = nullptr;
};

class GroupItems : public QUndoCommand
{
public:
    enum
    {
        Id = 1230
    };

    GroupItems(Scene* scene, QList<QGraphicsItem*> itemList, QUndoCommand* parent = nullptr);

    void undo();
    void redo();

    int
    id() const
    {
        return Id;
    }

    ItemGroup*
    group()
    {
        return g;
    }

private:
    QList<QGraphicsItem*> items;
    ItemGroup* g = nullptr;
    Scene* s = nullptr;
};

class UngroupItems : public QUndoCommand
{
public:
    enum
    {
        Id = 1240
    };

    UngroupItems(Scene* scene, ItemGroup* group, QUndoCommand* parent = nullptr);

    void undo();
    void redo();

    int
    id() const
    {
        return Id;
    }

private:
    QList<QGraphicsItem*> items;
    ItemGroup* g = nullptr;
    Scene* s = nullptr;
};

class AddLayer : public QUndoCommand
{
public:
    enum
    {
        Id = 1250
    };

    AddLayer(Scene* scene, ChartLayer* layer, QUndoCommand* parent = nullptr);
    ~AddLayer();

    void undo();
    void redo();

    int
    id() const
    {
        return Id;
    }

private:
    ChartLayer* mLayer = nullptr;
    Scene* s = nullptr;
};

class RemoveLayer : public QUndoCommand
{
public:
    enum
    {
        Id = 1260
    };

    RemoveLayer(Scene* scene, ChartLayer* layer, QUndoCommand* parent = nullptr);

    void undo();
    void redo();

    int
    id() const
    {
        return Id;
    }

private:
    ChartLayer* mLayer = nullptr;
    Scene* s = nullptr;
};

class SetLayerStitch : public QUndoCommand
{
public:
    enum
    {
        Id = 1270
    };

    SetLayerStitch(Scene* scene, Cell* cell, unsigned int layer, QUndoCommand* parent = nullptr);

    void undo();
    void redo();

    int
    id() const
    {
        return Id;
    }

private:
    Scene* s = nullptr;
    Cell* c = nullptr;
    unsigned int mNew = 0;
    unsigned int mOld = 0;
};

class SetLayerIndicator : public QUndoCommand
{
public:
    enum
    {
        Id = 1280
    };

    SetLayerIndicator(Scene* scene, Indicator* cell, unsigned int layer, QUndoCommand* parent = nullptr);

    void undo();
    void redo();

    int
    id() const
    {
        return Id;
    }

private:
    Scene* s = nullptr;
    Indicator* c = nullptr;
    unsigned int mNew = 0;
    unsigned int mOld = 0;
};

class SetLayerGroup : public QUndoCommand
{
public:
    enum
    {
        Id = 1290
    };

    SetLayerGroup(Scene* scene, ItemGroup* cell, unsigned int layer, QUndoCommand* parent = nullptr);

    void undo();
    void redo();

    int
    id() const
    {
        return Id;
    }

private:
    Scene* s = nullptr;
    ItemGroup* c = nullptr;
    unsigned int mNew = 0;
    unsigned int mOld = 0;
};

class SetLayerImage : public QUndoCommand
{
public:
    enum
    {
        Id = 1300
    };

    SetLayerImage(Scene* scene, ChartImage* cell, unsigned int layer, QUndoCommand* parent = nullptr);

    void undo();
    void redo();

    int
    id() const
    {
        return Id;
    }

private:
    Scene* s = nullptr;
    ChartImage* c = nullptr;
    unsigned int mNew = 0;
    unsigned int mOld = 0;
};

#endif  // CROCHETCHARTCOMMANDS_H
