#ifndef CROCHETCELL_H
#define CROCHETCELL_H

#include "cell.h"

class CrochetCell : public Cell
{
    Q_OBJECT
public:
    CrochetCell(const QString fileName);
    ~CrochetCell();

    QRectF boundingRect () const;
    void paint ( QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget = 0 );
    int type () const;

signals:

public slots:


private:


};

#endif // CROCHETCELL_H
