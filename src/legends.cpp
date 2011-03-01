/*************************************************\
| Copyright (c) 2011 Stitch Works Software        |
| Brian C. Milco <brian@stitchworkssoftware.com>  |
\*************************************************/
#include "legends.h"
#include <QPainter>

#include "settings.h"

#include <QDebug>
#include <QGraphicsScene>

#include "stitchlibrary.h"
#include "stitch.h"
#include <QSvgRenderer>

#include <math.h>

QPixmap Legend::drawColorBox(QColor color, QSize size)
{
    QPixmap pix = QPixmap(size);
    QPainter p;
    p.begin(&pix);
    p.fillRect(QRect(QPoint(0, 0), size), QColor(color));
    p.drawRect(0, 0, size.width() - 1, size.height() - 1);
    p.end();

    return pix;
}


ColorLegend::ColorLegend(QMap<QString, QMap<QString, qint64> > *colors, QGraphicsItem *parent)
    : QGraphicsWidget(parent)
{
    mPatternColors = colors;

    QStringList keys = mPatternColors->keys();
    
    foreach(QString key, keys) {
        qint64 added = mPatternColors->value(key).value("added");
        sortedColors.insert(added, key);
    }

    showHexValues = Settings::inst()->value("showHexValues").toBool();
    columnCount = Settings::inst()->value("colorColumnCount").toInt();
    colorNumber = Settings::inst()->value("colorPrefix").toString();
    prefix = Settings::inst()->value("colorPrefix").toString();
    
}

ColorLegend::~ColorLegend()
{
}

void ColorLegend::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    
    int textHeight = painter->fontMetrics().height();
    
    QList<qint64> sortedKeys = sortedColors.keys();
        
    int colWidth = Legend::margin + Legend::iconWidth + Legend::margin +
                     painter->fontMetrics().width(prefix + sortedColors.count()) +
                     painter->fontMetrics().width(" - #FFFFFF") + Legend::margin;

    //if we have more columns then items don't draw a really large white space.
    int items = (sortedKeys.count() < columnCount) ? sortedKeys.count() : columnCount;
    
    int itemsPerCol = ceil(double(sortedKeys.count()) / double(items));

    int imageWidth =  colWidth * items;
    int imageHeight = itemsPerCol * (Legend::margin + Legend::iconHeight) + Legend::margin;
    
    painter->fillRect(QRect(0,0,imageWidth,imageHeight), Qt::white);

    for(int i = 0; i < sortedKeys.count(); ++i) {
        QString hex = sortedColors.value(sortedKeys.at(i));

        int x = Legend::margin + ceil(i/itemsPerCol + 0.0) *colWidth;
        int y = Legend::margin + ((Legend::margin + Legend::iconHeight) * (i%itemsPerCol));
        
        painter->drawPixmap(x, y, Legend::drawColorBox(QColor(hex), QSize(Legend::iconWidth, Legend::iconHeight)));
        x += Legend::iconWidth + Legend::margin;
        y +=  + (.5 * (Legend::iconHeight + textHeight));
        painter->drawText(x, y, prefix + QString::number(i + 1));
        
        if(showHexValues) {
            x += painter->fontMetrics().width(prefix + QString::number(i + 1));
            painter->drawText(x, y, " - " + hex.toUpper());
        }
    }

    scene()->setSceneRect(0, 0, imageWidth, imageHeight);
}


/**********************************************************************************\
| Stitch Legend                                                                    |
\**********************************************************************************/
StitchLegend::StitchLegend(QMap<QString, int> *stitches, QGraphicsItem* parent)
    : QGraphicsWidget(parent)
{
    mPatternStitches = stitches;

    drawDescription = Settings::inst()->value("showStitchDescription").toBool();
    drawWrongSide = Settings::inst()->value("showWrongSideDescription").toBool();
    columnCount = Settings::inst()->value("stitchColumnCount").toInt();
    
}

StitchLegend::~StitchLegend()
{
}

void StitchLegend::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    
    QStringList keys = mPatternStitches->keys();
    int textHeight = painter->fontMetrics().height();

    int imageHeight = 0;
    int imageWidth = 0;

    double totalHeight = 0.0;
    QList <int> heights;
    QList <int> widths;
    QMap<int, QMap<QString, int> > columns; //"count", and "height", and "width";

    int widestCol = 0;
    foreach(QString key, keys) {
        Stitch *s = StitchLibrary::inst()->findStitch(key);
        if(!s)
            continue;
        
        QSize size = s->isSvg() ? s->renderSvg()->defaultSize() : s->renderPixmap()->size();
        totalHeight += size.height() + Legend::margin;
        heights.append(size.height());
        //FIXME: set a reasonable max width for each column and default to it if bigger & cut the text into multiple rows.
        //TODO: also check the width of the wrong side...
        //FIXME: doesn't always give correct results.
        int itemWidth = Legend::margin + size.width() + Legend::margin + painter->fontMetrics().width(s->name()) + painter->fontMetrics().width(" - " + s->description());
        widths.append(itemWidth);
        if(itemWidth > widestCol)
            widestCol = itemWidth;
    }

    //if we have more columns then items don't draw a really large white space.
    int items = (keys.count() < columnCount) ? keys.count() : columnCount;

    int avgColHeight = ceil(totalHeight / items);

    int tallestCol = 0;
    while(heights.count() > 0) {
        int i = 0;
        int stackHeight = 0;
        int stackWidth = 0;

        while(stackHeight < avgColHeight && heights.count() > 0) {
            
            stackHeight += heights.takeFirst() + Legend::margin;
            int w = widths.takeFirst() + Legend::margin;
            stackWidth = (w > stackWidth ? w : stackWidth);
            ++i;
        }
        QMap <QString, int> props;
        props.insert("count", i);
        props.insert("height", stackHeight);
        props.insert("width", stackWidth);
        imageWidth += stackWidth;
        if(stackHeight > tallestCol)
            tallestCol = stackHeight;
        columns.insert(columns.count(), props);
    }

    imageHeight = tallestCol + Legend::margin;
    imageWidth += Legend::margin;
    
    painter->fillRect(QRect(0, 0, imageWidth, imageHeight), Qt::white);

    int i = 0;
    int iconWidth = 0;
    int column = 0;
    int columnStart = 0;
    int curColHeight = Legend::margin;
    int prevItems = 0;
    int itemsPerCol = columns.value(0).value("count");
    
    foreach(QString key, keys) {
        Stitch *s = StitchLibrary::inst()->findStitch(key);
        if(!s) {
            qWarning() << "Couldn't find stitch while generating legend: " << key;
            continue;
        }
        
        if(floor(double(i - prevItems)/itemsPerCol) > 0) {
            curColHeight = Legend::margin;
            prevItems += itemsPerCol;
            columnStart += columns.value(column).value("width");
            column++;
            itemsPerCol = columns.value(column).value("count");
        }

        int x = Legend::margin + ceil(double(i - prevItems)/itemsPerCol) + columnStart;
        int y = ((i - prevItems)%itemsPerCol) * Legend::margin + curColHeight;
        
        int iconHeight = 0;
        
        if(s->isSvg()) {
            s->renderSvg()->render(painter, QRect(QPoint(x, y), s->renderSvg()->defaultSize()));
            iconHeight = s->renderSvg()->defaultSize().height();
            iconWidth = s->renderSvg()->defaultSize().width();
        } else {
            painter->drawPixmap(x,y, *(s->renderPixmap()));
            iconHeight = s->renderPixmap()->height();
            iconWidth = s->renderPixmap()->width();
        }
        curColHeight += iconHeight;
        
        //Draw lines around each "cell" of the stitch.
        int y1 = y;
        int counter = iconHeight;
        while(counter > 0) {
            painter->drawRect(x, y1, 64, 32);
            counter -=32;
            y1 += 32;
        }
        
        x += iconWidth + Legend::margin;
        y += textHeight;
        
        painter->drawText(x,y, s->name());
        
        if(drawDescription) {
            int x1 = x + painter->fontMetrics().width(s->name());
            painter->drawText(x1,y, " - " + s->description());
        }
        
        y += textHeight;
        
        if(drawWrongSide) {
            if(s->wrongSide() != s->name()) {
                Stitch *ws = StitchLibrary::inst()->findStitch(s->wrongSide());
                if(ws) {
                    painter->drawText(x,y, ws->name());
                    if(drawDescription) {
                        int x1 = x + painter->fontMetrics().width(s->name());
                        painter->drawText(x1,y," - " + ws->description());
                    }
                }
            }
        }
        
        ++i;
    }
    

    
    scene()->setSceneRect(0,0, imageWidth, imageHeight);
}



