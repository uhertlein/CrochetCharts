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
#include "guideline.h"

#include <QDebug>
#include <QGraphicsSceneMouseEvent>

Guideline::Guideline(const QRectF& rect, QGraphicsItem* parent, QGraphicsScene* /*scene*/)
    : QGraphicsEllipseItem(rect, parent)
{
    setAcceptedMouseButtons(Qt::MouseButtons{});
}

void
Guideline::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    event->ignore();
}

void
Guideline::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    event->ignore();
}

void
Guideline::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    event->ignore();
}
