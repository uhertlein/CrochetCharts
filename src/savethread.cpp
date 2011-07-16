    /*************************************************\
| Copyright (c) 2011 Stitch Works Software        |
| Brian C. Milco <brian@stitchworkssoftware.com>  |
\*************************************************/
#include "savethread.h"

#include "crochetcell.h"
#include "stitch.h"

#include <QObject>

#include <QString>
#include <QTransform>

#include "crochetscene.h"

SaveThread::SaveThread(CrochetTab* t, QXmlStreamReader* s)
{
    tab = t;
    stream = s;
}

SaveThread::~SaveThread()
{
}

void SaveThread::run()
{
    CrochetCell *c = new CrochetCell();
    Stitch *s = 0;
    int row, column;
    QString color;
    qreal x, y, anchorX = 0, anchorY = 0;
    QTransform transform;
    double angle;
    
    QObject::connect(c, SIGNAL(stitchChanged(QString,QString)), tab->scene(), SIGNAL(stitchChanged(QString,QString)));
    QObject::connect(c, SIGNAL(colorChanged(QString,QString)), tab->scene(), SIGNAL(colorChanged(QString,QString)));
    QObject::connect(c, SIGNAL(stitchChanged(QString,QString)), tab->scene(), SLOT(stitchUpdated(QString,QString)));
    
    while(!(stream->isEndElement() && stream->name() == "cell")) {
        stream->readNext();
        QString tag = stream->name().toString();
        
        if(tag == "stitch") {
            QString st = stream->readElementText();
            s = StitchLibrary::inst()->findStitch(st);
        } else if(tag == "row") {
            row = stream->readElementText().toInt();
        } else if(tag == "column") {
            column = stream->readElementText().toInt();
        } else if(tag == "color") {
            color = stream->readElementText();
        } else if(tag == "x") {
            x = stream->readElementText().toDouble();
        } else if(tag == "y") {
            y = stream->readElementText().toDouble();
        } else if(tag == "anchor_x") {
            anchorX = stream->readElementText().toDouble();
        } else if(tag == "anchor_y") {
            anchorY = stream->readElementText().toDouble();
        } else if(tag == "transformation") {
            transform = loadTransform(stream);
        } else if(tag == "angle") {
            angle = stream->readElementText().toDouble();
        }
    }
    
    c->setStitch(s, (row % 2));
    tab->scene()->appendCell(row, c, true);
    c->setObjectName(QString::number(row) + "," + QString::number(column));
    c->setColor(QColor(color));
    c->setAnchor(anchorX, anchorY);
    c->setPos(x, y);
    c->setTransform(transform);
    c->setAngle(angle);
}

QTransform SaveThread::loadTransform(QXmlStreamReader* stream)
{
    QTransform transform;
    
    qreal m11, m12, m13,
    m21, m22, m23,
    m31, m32, m33;
    
    while(!(stream->isEndElement() && stream->name() == "transformation")) {
        stream->readNext();
        QString tag = stream->name().toString();
        
        if(tag == "m11") {
            m11 = (qreal)stream->readElementText().toDouble();
        } else if(tag == "m12") {
            m12 = (qreal)stream->readElementText().toDouble();
        } else if(tag == "m13") {
            m13 = (qreal)stream->readElementText().toDouble();
        } else if(tag == "m21") {
            m21 = (qreal)stream->readElementText().toDouble();
        } else if(tag == "m22") {
            m22 = (qreal)stream->readElementText().toDouble();
        } else if(tag == "m23") {
            m23 = (qreal)stream->readElementText().toDouble();
        } else if(tag == "m31") {
            m31 = (qreal)stream->readElementText().toDouble();
        } else if(tag == "m32") {
            m32 = (qreal)stream->readElementText().toDouble();
        } else if(tag == "m33") {
            m33 = (qreal)stream->readElementText().toDouble();
        }
    }
    
    transform.setMatrix(m11, m12, m13, m21, m22, m23, m31, m32, m33);
    return transform;
}