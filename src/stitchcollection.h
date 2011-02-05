/*************************************************\
| Copyright (c) 2011 Stitch Works Software        |
| Brian C. Milco <brian@stitchworkssoftware.com>  |
\*************************************************/
#ifndef STITCHCOLLECTION_H
#define STITCHCOLLECTION_H

#include <QObject>
#include <QStringList>

class StitchSet;
class Stitch;

class QComboBox;
class QXmlStreamWriter;

/*
    for every set of stitches the user has we'll load a StitchSet (including the user overlay).
    Once all the sets are loaded we want to combine all the stitches into one master list.

    This master list of stitches is what gets searched, and used. The collection acts as the
    gatekeeper and if the user 'imports' a stitch from one set that is already in use it turns
    the other stitch 'off'.

    Ideally the collection is just a collection of pointers to the real stitches in the other
    sets.
*/
class StitchCollection : public QObject
{
    Q_OBJECT

public:
    static StitchCollection* inst();
    ~StitchCollection();

    //return the list of stitch sets.
    QList<StitchSet *> stitchSets() { return mStitchSets; }
    //return the master stitch set.
    StitchSet* masterStitchSet() { return mMasterSet; }
    StitchSet* builtIn() { return mBuiltIn; }
    
    //load all known stitch sets.
    void loadStitchSets();
    //create links to the stitches being used from each set/overlay
    void populateMasterSet();

    //fill in a dropdown list for selecting a stitch set.
    void populateComboBox(QComboBox *cb);

    //this function finds the named stitch it can search the master set and any alternative sets.
    Stitch* findStitch(QString name);
    
    StitchSet* findStitchSet(QString setName);
    QStringList categoryList() const;
    QStringList stitchList() const;

protected:
    //TODO: figure out which stitches should be saved to which file...
    void saveMasterStitchSet(QString fileName);

private:
    StitchCollection();
    static StitchCollection* mInstance;

    QList<StitchSet*> mStitchSets;
    StitchSet* mMasterSet; //links to stitches in the other mStitchSets...
    StitchSet* mBuiltIn;

};

#endif //STITCHCOLLECTION_H
