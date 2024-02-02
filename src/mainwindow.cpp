/****************************************************************************\
 Copyright (c) 2010-2014 Stitch Works Software
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
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "stitchlibraryui.h"
#include "exportui.h"

#include "application.h"

#include "appinfo.h"
#include "settings.h"
#include "settingsui.h"

#include "crochettab.h"
#include "stitchlibrary.h"
#include "stitchset.h"
#include "stitchpalettedelegate.h"

#include "stitchreplacerui.h"
#include "colorreplacer.h"

#include "debug.h"
#include <QDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QColorDialog>
#include <QStandardItemModel>
#include <QStandardItem>

#include <QPrinter>
#include <QPrintDialog>
#include <QPrintPreviewDialog>

#include <QActionGroup>
#include <QCloseEvent>
#include <QUndoStack>
#include <QUndoView>
#include <QTimer>

#include <QSortFilterProxyModel>
#include <QDesktopServices>

MainWindow::MainWindow(QStringList fileNames, QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , mUpdater(0)
    , mResizeUI(0)
    , mAlignDock(0)
    , mRowsDock(0)
    , mMirrorDock(0)
    , mPropertiesDock(0)
    , mEditMode(10)
    , mStitch("ch")
    , mFgColor(QColor(Qt::black))
    , mBgColor(QColor(Qt::white))
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    ui->setupUi(this);

    connect(ui->tabWidget, SIGNAL(currentChanged(int)), this, SLOT(tabChanged(int)));
    connect(ui->tabWidget, SIGNAL(tabCloseRequested(int)), this, SLOT(removeTab(int)));

    setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

    setUnifiedTitleAndToolBarOnMac(true);

#ifndef APPLE_APP_STORE
    bool checkForUpdates = Settings::inst()->value("checkForUpdates").toBool();
    if (checkForUpdates)
        checkUpdates();
#endif

    setupStitchPalette();
    setupLayersDock();
    setupDocks();

    mFile = new FileFactory(this);
    loadFiles(fileNames);

    setAcceptDrops(true);

    setApplicationTitle();
    setupNewTabDialog();

    setupMenus();
    readSettings();

#ifdef Q_OS_MACX
    // File icon for titlebar
    fileIcon = QIcon(":/images/stitchworks-pattern.svg");
#else
    fileIcon = QIcon(":/images/CrochetCharts.png");
    setWindowIcon(fileIcon);
#endif
    QApplication::restoreOverrideCursor();
}

MainWindow::~MainWindow()
{
    delete mModeGroup;
    delete mSelectGroup;
    delete mGridGroup;
    delete ui;
    delete mFile;

    if (mUpdater)
        delete mUpdater;
}

void
MainWindow::dragEnterEvent(QDragEnterEvent* e)
{
    if (e->mimeData()->hasUrls())
    {
        e->acceptProposedAction();
    }
}

void
MainWindow::dropEvent(QDropEvent* e)
{
    QStringList files;
    foreach (const QUrl& url, e->mimeData()->urls())
    {
        const QString& fileName = url.toLocalFile();
        files.append(fileName);
    }
    loadFiles(files);
}

void
MainWindow::loadFiles(QStringList fileNames)
{
    if (fileNames.count() < 1)
        return;

    if (ui->tabWidget->count() < 1)
    {
        mFile->fileName = fileNames.takeFirst();
        int error = mFile->load();

        if (error != FileFactory::No_Error)
        {
            showFileError(error);
            return;
        }

        Settings::inst()->files.insert(mFile->fileName.toLower(), this);
        addToRecentFiles(mFile->fileName);
        ui->newDocument->hide();
    }

    foreach (QString fileName, fileNames)
    {
        QStringList files;
        files.append(fileName);
        MainWindow* newWin = new MainWindow(files);
        newWin->move(x() + 40, y() + 40);
        newWin->show();
        newWin->raise();
        newWin->activateWindow();
        Settings::inst()->files.insert(mFile->fileName.toLower(), newWin);
        addToRecentFiles(fileName);
    }
}

void
MainWindow::checkUpdates(bool silent)
{
    if (mUpdater)
    {
        delete mUpdater;
        mUpdater = nullptr;
    }

    // TODO: check for updates in a separate thread.
    mUpdater = new Updater(this);
    // append the updater to the centralWidget to keep it out of the way of the menus.
    ui->centralWidget->layout()->addWidget(mUpdater);

    mUpdater->checkForUpdates(silent);  // check at startup is always silent.
}

void
MainWindow::setApplicationTitle()
{
    QString curFile = mFile->fileName;
    setWindowModified(false);

    QString shownName = "";
    QString join = "";
    QIcon icon;

    if (curCrochetTab())
    {
        if (curFile.isEmpty())
        {
            shownName = "my design.pattern[*]";
        }
        else
        {
            shownName = QFileInfo(curFile).fileName() + "[*]";
#ifdef Q_OS_MACX
            icon = fileIcon;
#else
            icon = QIcon(":/images/CrochetCharts.png");
#endif
        }
        join = " - ";
    }
    QString title;

#ifdef Q_OS_MACX
    title = tr("%1%3%2").arg(shownName).arg(qApp->applicationName()).arg(join);
#else
    title = tr("%2%3%1").arg(shownName).arg(qApp->applicationName()).arg(join);
#endif

    setWindowTitle(title);
    setWindowFilePath(curFile);
    setWindowIcon(icon);
}

void
MainWindow::setupNewTabDialog()
{
    int rows = Settings::inst()->value("rowCount").toInt();
    int stitches = Settings::inst()->value("stitchCount").toInt();
    QString defSt = Settings::inst()->value("defaultStitch").toString();
    QString defStyle = Settings::inst()->value("chartStyle").toString();
    int incBy = Settings::inst()->value("increaseBy").toInt();

    ui->rows->setValue(rows);
    ui->stitches->setValue(stitches);
    ui->increaseBy->setValue(incBy);

    ui->defaultStitch->addItems(StitchLibrary::inst()->stitchList());
    ui->defaultStitch->setCurrentIndex(ui->defaultStitch->findText(defSt));

    ui->chartStyle->setCurrentIndex(ui->chartStyle->findText(defStyle));

    newChartUpdateStyle(defStyle);
    connect(ui->chartStyle, SIGNAL(currentIndexChanged(QString)),
            SLOT(newChartUpdateStyle(QString)));

    connect(ui->newDocBttnBox, SIGNAL(accepted()), this, SLOT(newChart()));
    connect(ui->newDocBttnBox, SIGNAL(rejected()), ui->newDocument, SLOT(hide()));
}

void
MainWindow::newChartUpdateStyle(QString style)
{
    if (style == tr("Blank"))
    {
        ui->rows->setVisible(false);
        ui->rowsLbl->setVisible(false);
        ui->stitches->setVisible(false);
        ui->stitchesLbl->setVisible(false);
        ui->rowSpacing->setVisible(false);
        ui->rowSpacingLbl->setVisible(false);
        ui->defaultStitch->setVisible(false);
        ui->defaultStitchLbl->setVisible(false);
        ui->increaseBy->setVisible(false);
        ui->increaseByLbl->setVisible(false);
    }
    else if (style == tr("Rounds"))
    {
        ui->rows->setVisible(true);
        ui->rowsLbl->setVisible(true);
        ui->stitches->setVisible(true);
        ui->stitchesLbl->setVisible(true);
        ui->rowSpacing->setVisible(true);
        ui->rowSpacingLbl->setVisible(true);
        ui->defaultStitch->setVisible(true);
        ui->defaultStitchLbl->setVisible(true);
        ui->rowsLbl->setText(tr("Rounds:"));
        ui->stitchesLbl->setText(tr("Starting Stitches:"));
        ui->increaseBy->setVisible(true);
        ui->increaseByLbl->setVisible(true);
    }
    else if (style == tr("Rows"))
    {
        ui->rows->setVisible(true);
        ui->rowsLbl->setVisible(true);
        ui->stitches->setVisible(true);
        ui->stitchesLbl->setVisible(true);
        ui->rowSpacing->setVisible(true);
        ui->rowSpacingLbl->setVisible(true);
        ui->defaultStitch->setVisible(true);
        ui->defaultStitchLbl->setVisible(true);
        ui->rowsLbl->setText(tr("Rows:"));
        ui->stitchesLbl->setText(tr("Stitches:"));
        ui->increaseBy->setVisible(false);
        ui->increaseByLbl->setVisible(false);
    }
}

void
MainWindow::propertiesUpdate(QString property, QVariant newValue)
{
    if (!curCrochetTab())
        return;

    curCrochetTab()->propertiesUpdate(property, newValue);
}

void
MainWindow::reloadLayerContent(QList<ChartLayer*>& layers, ChartLayer* selected)
{
    CrochetTab* tab = curCrochetTab();
    if (!tab)
        return;

    QTreeView* view = ui->layersView;
    QStandardItemModel* model = dynamic_cast<QStandardItemModel*>(view->model());

    // if there isn't a model yet, create one
    if (!model)
    {
        model = new QStandardItemModel(0, 2, this);
        // and connect the signals
        connect(model, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&)), this,
                SLOT(layerModelChanged(const QModelIndex&)));
        view->setModel(model);
    }

    // cleanup previous content
    model->clear();

    // set up the header of the model
    model->setHeaderData(0, Qt::Horizontal, QObject::tr("Visible"));
    model->setHeaderData(1, Qt::Horizontal, QObject::tr("Name"));

    // dont emit signals while populating the model
    model->blockSignals(true);

    QStandardItem* selecteditem = nullptr;
    for (int i = 0; i < layers.count(); i++)
    {
        // create the item
        ChartLayer* layer = layers[i];

        QStandardItem* item = new QStandardItem(layer->name());

        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);

        // set the checkbox
        item->setCheckable(true);

        // and add it to the list
        model->appendRow(item);

        if (layer->visible())
            item->setCheckState(Qt::Checked);
        else
            item->setCheckState(Qt::Unchecked);

        item->setData(QVariant::fromValue((void*)layer), Qt::UserRole + 5);

        if (layer == selected)
            selecteditem = item;
    }
    // let the model send signals again
    model->blockSignals(false);

    // finally, select the currently selected layer
    if (selecteditem)
        view->setCurrentIndex(selecteditem->index());
}

void
MainWindow::setupStitchPalette()
{
    StitchSet* set = StitchLibrary::inst()->masterStitchSet();
    mProxyModel = new QSortFilterProxyModel(this);

    mProxyModel->setSourceModel(set);
    mProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    ui->allStitches->setModel(mProxyModel);

    // TODO: setup a proxywidget that can hold header sections?
    StitchPaletteDelegate* delegate = new StitchPaletteDelegate(ui->allStitches);
    ui->allStitches->setItemDelegate(delegate);
    ui->allStitches->hideColumn(2);
    ui->allStitches->hideColumn(3);
    ui->allStitches->hideColumn(4);
    ui->allStitches->hideColumn(5);

    connect(ui->allStitches, SIGNAL(clicked(QModelIndex)), SLOT(selectStitch(QModelIndex)));
    connect(ui->patternStitches, SIGNAL(clicked(QModelIndex)), SLOT(selectStitch(QModelIndex)));

    connect(ui->patternColors, SIGNAL(clicked(QModelIndex)), SLOT(selectColor(QModelIndex)));
    connect(ui->fgColor, SIGNAL(colorChanged(QColor)), SLOT(addColor(QColor)));
    connect(ui->bgColor, SIGNAL(colorChanged(QColor)), SLOT(addColor(QColor)));
    connect(ui->patternColors, SIGNAL(bgColorSelected(QModelIndex)),
            SLOT(selectColor(QModelIndex)));

    connect(ui->stitchFilter, SIGNAL(textChanged(QString)), SLOT(filterStitchList(QString)));
}

void
MainWindow::setupLayersDock()
{
    connect(ui->addLayerBtn, SIGNAL(released()), this, SLOT(addLayer()));
    connect(ui->removeLayerBtn, SIGNAL(released()), this, SLOT(removeLayer()));
    connect(ui->mergeBtn, SIGNAL(released()), this, SLOT(mergeLayer()));
    connect(ui->layersView, SIGNAL(clicked(const QModelIndex&)), this,
            SLOT(selectLayer(const QModelIndex&)));
}

void
MainWindow::setupDocks()
{
    // Undo Dock.
    mUndoDock = new QDockWidget(this);
    mUndoDock->setVisible(false);
    mUndoDock->setObjectName("undoHistory");
    QUndoView* view = new QUndoView(&mUndoGroup, mUndoDock);
    mUndoDock->setWidget(view);
    mUndoDock->setWindowTitle(tr("Undo History"));
    mUndoDock->setFloating(true);

    // Resize Dock
    mResizeUI = new ResizeUI(ui->tabWidget, this);
    connect(mResizeUI, SIGNAL(visibilityChanged(bool)), ui->actionShowResizeDock,
            SLOT(setChecked(bool)));
    connect(mResizeUI, SIGNAL(resize(QRectF)), SLOT(resize(QRectF)));
    connect(ui->tabWidget, SIGNAL(currentChanged(int)), mResizeUI, SLOT(updateContent(int)));

    // Align & Distribute Dock
    mAlignDock = new AlignDock(this);
    connect(mAlignDock, SIGNAL(align(int)), SLOT(alignSelection(int)));
    connect(mAlignDock, SIGNAL(distribute(int)), SLOT(distributeSelection(int)));
    connect(mAlignDock, SIGNAL(visibilityChanged(bool)), ui->actionShowAlignDock,
            SLOT(setChecked(bool)));

    // Rows & Stitches Dock.
    mRowsDock = new RowsDock(this);
    mRowsDock->setEnabled(false);  // TODO: remove this line when this is working again.
    connect(mRowsDock, SIGNAL(arrangeGrid(QSize, QSize, QSize, bool)),
            SLOT(arrangeGrid(QSize, QSize, QSize, bool)));
    connect(mRowsDock, SIGNAL(visibilityChanged(bool)), ui->actionShowRowsDock,
            SLOT(setChecked(bool)));

    // Mirror & Rotate.
    mMirrorDock = new MirrorDock(this);
    connect(mMirrorDock, SIGNAL(mirror(int)), SLOT(mirror(int)));
    connect(mMirrorDock, SIGNAL(copy(int)), SLOT(copy(int)));
    connect(mMirrorDock, SIGNAL(rotate(qreal)), SLOT(rotate(qreal)));
    connect(mMirrorDock, SIGNAL(visibilityChanged(bool)), ui->actionShowMirrorDock,
            SLOT(setChecked(bool)));

    mPropertiesDock = new PropertiesDock(ui->tabWidget, this);
    connect(mPropertiesDock, SIGNAL(visibilityChanged(bool)), ui->actionShowProperties,
            SLOT(setChecked(bool)));
    connect(mPropertiesDock, SIGNAL(propertiesUpdated(QString, QVariant)),
            SLOT(propertiesUpdate(QString, QVariant)));
    connect(mPropertiesDock, SIGNAL(setGridType(QString)), SLOT(setSelectedGridMode(QString)));
}

void
MainWindow::setupMenus()
{
    // File Menu
    connect(ui->menuFile, SIGNAL(aboutToShow()), SLOT(menuFileAboutToShow()));
    connect(ui->menuFile, SIGNAL(aboutToShow()), SLOT(menuRecentFilesAboutToShow()));
    connect(ui->actionNew, SIGNAL(triggered()), SLOT(fileNew()));
    connect(ui->actionOpen, SIGNAL(triggered()), SLOT(fileOpen()));
    connect(ui->actionSave, SIGNAL(triggered()), SLOT(fileSave()));
    connect(ui->actionSaveAs, SIGNAL(triggered()), SLOT(fileSaveAs()));

    connect(ui->actionPrint, SIGNAL(triggered()), SLOT(filePrint()));
    connect(ui->actionPrintPreview, SIGNAL(triggered()), SLOT(filePrintPreview()));
    connect(ui->actionExport, SIGNAL(triggered()), SLOT(fileExport()));

    connect(ui->actionQuit, SIGNAL(triggered()), SLOT(close()));

    ui->actionOpen->setIcon(QIcon::fromTheme("document-open", QIcon(":/images/fileopen.png")));
    ui->actionNew->setIcon(QIcon::fromTheme("document-new", QIcon(":/images/filenew.png")));
    ui->actionSave->setIcon(QIcon::fromTheme("document-save", QIcon(":/images/filesave.png")));
    ui->actionSaveAs->setIcon(QIcon::fromTheme("document-save-as", QIcon(":/images/filesave.png")));

    ui->actionPrint->setIcon(QIcon::fromTheme("document-print", QIcon(":/images/fileprint.png")));
    ui->actionPrintPreview->setIcon(
        QIcon::fromTheme("document-print-preview", QIcon(":/images/document-print-preview.png")));

    /*document-export*/
    ui->actionQuit->setIcon(
        QIcon::fromTheme("application-exit", QIcon(":/images/application-exit.png")));

    setupRecentFiles();

    // Edit Menu
    connect(ui->menuEdit, SIGNAL(aboutToShow()), SLOT(menuEditAboutToShow()));

    mActionUndo = mUndoGroup.createUndoAction(this, tr("Undo"));
    mActionRedo = mUndoGroup.createRedoAction(this, tr("Redo"));

    ui->menuEdit->insertAction(ui->actionCopy, mActionUndo);
    ui->menuEdit->insertAction(ui->actionCopy, mActionRedo);
    ui->menuEdit->insertSeparator(ui->actionCopy);

    ui->mainToolBar->insertAction(0, mActionUndo);
    ui->mainToolBar->insertAction(0, mActionRedo);
    ui->mainToolBar->insertSeparator(mActionUndo);

    mActionUndo->setIcon(QIcon::fromTheme("edit-undo", QIcon(":/images/editundo.png")));
    mActionRedo->setIcon(QIcon::fromTheme("edit-redo", QIcon(":/images/editredo.png")));
    mActionUndo->setShortcut(QKeySequence::Undo);
    mActionRedo->setShortcut(QKeySequence::Redo);

    ui->actionCopy->setIcon(QIcon::fromTheme("edit-copy", QIcon(":/images/editcopy.png")));
    ui->actionCut->setIcon(QIcon::fromTheme("edit-cut", QIcon(":/images/editcut.png")));
    ui->actionPaste->setIcon(QIcon::fromTheme("edit-paste", QIcon(":/images/editpaste.png")));

    connect(ui->actionCopy, SIGNAL(triggered()), SLOT(copy()));
    connect(ui->actionPaste, SIGNAL(triggered()), SLOT(paste()));
    connect(ui->actionCut, SIGNAL(triggered()), SLOT(cut()));
    connect(ui->actionInsertImage, SIGNAL(triggered()), SLOT(insertImage()));

    ui->fgColor->setColor(QColor(Settings::inst()->value("stitchPrimaryColor").toString()));
    ui->bgColor->setColor(QColor(Qt::white));

    // View Menu
    connect(ui->menuView, SIGNAL(aboutToShow()), SLOT(menuViewAboutToShow()));
    connect(ui->actionShowStitches, SIGNAL(triggered()), SLOT(viewShowStitches()));
    connect(ui->actionShowPatternColors, SIGNAL(triggered()), SLOT(viewShowPatternColors()));
    connect(ui->actionShowPatternStitches, SIGNAL(triggered()), SLOT(viewShowPatternStitches()));
    connect(ui->actionShowLayers, SIGNAL(triggered()), SLOT(viewShowLayers()));

    connect(ui->actionShowUndoHistory, SIGNAL(triggered()), SLOT(viewShowUndoHistory()));

    connect(ui->actionShowMainToolbar, SIGNAL(triggered()), SLOT(viewShowMainToolbar()));
    connect(ui->actionShowEditModeToolbar, SIGNAL(triggered()), SLOT(viewShowEditModeToolbar()));

    connect(ui->actionViewFullScreen, SIGNAL(triggered(bool)), SLOT(viewFullScreen(bool)));

    connect(ui->actionZoomIn, SIGNAL(triggered()), SLOT(viewZoomIn()));
    connect(ui->actionZoomOut, SIGNAL(triggered()), SLOT(viewZoomOut()));

    ui->actionZoomIn->setIcon(QIcon::fromTheme("zoom-in", QIcon(":/images/zoomin.png")));
    ui->actionZoomOut->setIcon(QIcon::fromTheme("zoom-out", QIcon(":/images/zoomout.png")));
    ui->actionZoomIn->setShortcut(QKeySequence::ZoomIn);
    ui->actionZoomOut->setShortcut(QKeySequence::ZoomOut);

    connect(ui->actionShowProperties, SIGNAL(triggered()), SLOT(viewShowProperties()));

    // Modes menu
    connect(ui->menuModes, SIGNAL(aboutToShow()), SLOT(menuModesAboutToShow()));

    mModeGroup = new QActionGroup(this);
    mModeGroup->addAction(ui->actionStitchMode);
    mModeGroup->addAction(ui->actionColorMode);
    mModeGroup->addAction(ui->actionAngleMode);
    mModeGroup->addAction(ui->actionStretchMode);
    mModeGroup->addAction(ui->actionCreateRows);
    mModeGroup->addAction(ui->actionIndicatorMode);

    connect(mModeGroup, SIGNAL(triggered(QAction*)), SLOT(changeTabMode(QAction*)));

    mSelectGroup = new QActionGroup(this);
    mSelectGroup->addAction(ui->actionBoxSelectMode);
    mSelectGroup->addAction(ui->actionLassoSelectMode);
    mSelectGroup->addAction(ui->actionLineSelectMode);
    ui->actionBoxSelectMode->setChecked(true);

    connect(mSelectGroup, SIGNAL(triggered(QAction*)), SLOT(changeSelectMode(QAction*)));
    connect(ui->actionNextSelectMode, SIGNAL(triggered()), SLOT(nextSelectMode()));

    addAction(ui->actionNextSelectMode);

    mGridGroup = new QActionGroup(this);
    mGridGroup->addAction(ui->actionGridNone);
    mGridGroup->addAction(ui->actionGridSquare);
    mGridGroup->addAction(ui->actionGridRound);
    mGridGroup->addAction(ui->actionGridTriangle);
    ui->actionGridNone->setChecked(true);

    connect(mGridGroup, SIGNAL(triggered(QAction*)), SLOT(changeGridMode(QAction*)));
    connect(mGridGroup, SIGNAL(triggered(QAction*)), mPropertiesDock, SLOT(propertyUpdated()));
    connect(ui->actionNextGridMode, SIGNAL(triggered()), SLOT(nextGridMode()));
    connect(ui->actionNextGridMode, SIGNAL(triggered()), mPropertiesDock, SLOT(propertyUpdated()));

    addAction(ui->actionNextGridMode);

    // Charts Menu
    connect(ui->actionAddChart, SIGNAL(triggered()), SLOT(documentNewChart()));
    connect(ui->actionRemoveTab, SIGNAL(triggered()), SLOT(removeCurrentTab()));

    connect(ui->actionShowChartCenter, SIGNAL(triggered()), SLOT(chartsShowChartCenter()));

    ui->actionRemoveTab->setIcon(QIcon::fromTheme("tab-close", QIcon(":/images/tabclose.png")));

    connect(ui->menuChart, SIGNAL(aboutToShow()), SLOT(menuChartAboutToShow()));
    connect(ui->actionEditName, SIGNAL(triggered()), SLOT(chartEditName()));
    // TODO: get more icons from the theme for use with table editing.
    // http://doc.qt.nokia.com/4.7/qstyle.html#StandardPixmap-enum

    connect(ui->actionCreateRows, SIGNAL(toggled(bool)), SLOT(chartCreateRows(bool)));

    // Stitches Menu
    connect(ui->actionShowAlignDock, SIGNAL(triggered()), SLOT(viewShowAlignDock()));
    connect(ui->actionShowRowsDock, SIGNAL(triggered()), SLOT(viewShowRowsDock()));
    connect(ui->actionShowMirrorDock, SIGNAL(triggered()), SLOT(viewShowMirrorDock()));
    connect(ui->actionShowResizeDock, SIGNAL(triggered()), SLOT(viewShowResizeDock()));

    connect(ui->actionGroup, SIGNAL(triggered()), SLOT(group()));
    connect(ui->actionUngroup, SIGNAL(triggered()), SLOT(ungroup()));

    connect(ui->actionReplaceStitch, SIGNAL(triggered()), SLOT(stitchesReplaceStitch()));
    connect(ui->actionColorReplacer, SIGNAL(triggered()), SLOT(stitchesReplaceColor()));

    // stitches menu
    connect(ui->menuStitches, SIGNAL(aboutToShow()), SLOT(menuStitchesAboutToShow()));

    // Tools Menu
    connect(ui->menuTools, SIGNAL(aboutToShow()), SLOT(menuToolsAboutToShow()));
    connect(ui->actionOptions, SIGNAL(triggered()), SLOT(toolsOptions()));
    connect(ui->actionStitchLibrary, SIGNAL(triggered()), SLOT(toolsStitchLibrary()));
    connect(ui->actionCheckForUpdates, SIGNAL(triggered()), SLOT(toolsCheckForUpdates()));

#ifdef APPLE_APP_STORE
    ui->actionCheckForUpdates->setVisible(false);
#endif

    // Help Menu
    connect(ui->actionAbout, SIGNAL(triggered()), SLOT(helpAbout()));
    connect(ui->actionCrochetHelp, SIGNAL(triggered()), SLOT(helpCrochetHelp()));

    // misc items
    connect(&mUndoGroup, SIGNAL(isModified(bool)), SLOT(documentIsModified(bool)));
    connect(ui->clearBttn, SIGNAL(clicked()), SLOT(clearStitchFilter()));

    updateMenuItems();
}

void
MainWindow::openRecentFile()
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    QAction* action = qobject_cast<QAction*>(sender());
    if (action && QFileInfo(action->data().toString()).exists())
    {
        QStringList files;
        files.append(action->data().toString());
        loadFiles(files);
    }

    setApplicationTitle();
    updateMenuItems();
    QApplication::restoreOverrideCursor();
}

void
MainWindow::addToRecentFiles(QString fileName)
{
    Settings::inst()->addRecentFile(fileName);
}

void
MainWindow::menuRecentFilesAboutToShow()
{
    setupRecentFiles();
}

void
MainWindow::setupRecentFiles()
{
    QStringList files;
    QStringList list = Settings::inst()->recentFiles();

    int maxRecentFiles = Settings::inst()->value("maxRecentFiles").toInt();
    mRecentFilesActs.clear();

    int i = 0;
    // remove any files that have been deleted or are not available.
    foreach (QString file, list)
    {
        if (QFileInfo(file).exists())
        {
            files.append(file);
            i++;
            if (i >= maxRecentFiles)
                break;
        }
    }

    for (int i = 0; i < files.count(); ++i)
    {
        QString text = tr("&%1 %2").arg(i + 1).arg(QFileInfo(files[i]).fileName());
        QAction* a = new QAction(this);
        connect(a, SIGNAL(triggered()), SLOT(openRecentFile()));

        a->setText(text);
        a->setData(files[i]);
        if (i < maxRecentFiles)
            a->setVisible(true);
        else
            a->setVisible(false);
        mRecentFilesActs.append(a);
    }

    ui->menuOpenRecent->clear();
    ui->menuOpenRecent->addActions(mRecentFilesActs);

    // update the master list.
    Settings::inst()->setRecentFiles(files);
}

void
MainWindow::updateMenuItems()
{
    menuFileAboutToShow();
    menuEditAboutToShow();
    menuViewAboutToShow();
    menuModesAboutToShow();
    menuChartAboutToShow();
    menuStitchesAboutToShow();
}

void
MainWindow::filePrint()
{
    // TODO: page count isn't working...
    QPrinter printer;
    QPrintDialog* dialog = new QPrintDialog(&printer, this);

    if (dialog->exec() != QDialog::Accepted)
        return;

    print(&printer);
}

void
MainWindow::print(QPrinter* printer)
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    int tabCount = ui->tabWidget->count();
    QPainter* p = new QPainter();

    p->begin(printer);

    bool firstPass = true;
    for (int i = 0; i < tabCount; ++i)
    {
        if (!firstPass)
            printer->newPage();

        CrochetTab* tab = qobject_cast<CrochetTab*>(ui->tabWidget->widget(i));
        tab->renderChart(p);
        firstPass = false;
    }
    p->end();

    QApplication::restoreOverrideCursor();
}

void
MainWindow::filePrintPreview()
{
    // FIXME: this isn't working
    QPrinter* printer = new QPrinter(QPrinter::HighResolution);
    QPrintPreviewDialog* dialog = new QPrintPreviewDialog(printer, this);
    connect(dialog, SIGNAL(paintRequested(QPrinter*)), this, SLOT(print(QPrinter*)));

    dialog->exec();
}

void
MainWindow::fileExport()
{
    if (!hasTab())
        return;

    ExportUi d(ui->tabWidget, &mPatternStitches, &mPatternColors, this);
    d.exec();
}

void
MainWindow::addColor(QColor color)
{
    for (int i = 0; i < ui->tabWidget->count(); ++i)
    {
        CrochetTab* tab = qobject_cast<CrochetTab*>(ui->tabWidget->widget(i));
        if (!tab)
            return;

        if (sender() == ui->fgColor)
        {
            tab->setEditFgColor(color);
        }
        else if (sender() == ui->bgColor)
        {
            tab->setEditBgColor(color);
        }
    }
}

void
MainWindow::updateDefaultStitchColor(QColor originalColor, QColor newColor)
{
    for (int i = 0; i < ui->tabWidget->count(); ++i)
    {
        CrochetTab* tab = qobject_cast<CrochetTab*>(ui->tabWidget->widget(i));
        if (!tab)
            continue;

        tab->updateDefaultStitchColor(originalColor, newColor);
    }
}

void
MainWindow::selectStitch(QModelIndex index)
{
    QModelIndex idx;

    if (sender() == ui->allStitches)
    {
        const QSortFilterProxyModel* model
            = static_cast<const QSortFilterProxyModel*>(index.model());
        idx = model->mapToSource(model->index(index.row(), 0));
    }
    else
        idx = index;

    QString stitch = idx.data(Qt::DisplayRole).toString();

    if (stitch.isEmpty())
        return;

    for (int i = 0; i < ui->tabWidget->count(); ++i)
    {
        CrochetTab* tab = qobject_cast<CrochetTab*>(ui->tabWidget->widget(i));
        if (tab)
            tab->setEditStitch(stitch);
    }

    setEditMode(10);
}

void
MainWindow::selectColor(QModelIndex index)
{
    QString color = index.data(Qt::ToolTipRole).toString();

    for (int i = 0; i < ui->tabWidget->count(); ++i)
    {
        CrochetTab* tab = qobject_cast<CrochetTab*>(ui->tabWidget->widget(i));
        if (tab)
        {
            tab->setEditFgColor(color);
            ui->fgColor->setColor(color);
        }
    }
}

void
MainWindow::filterStitchList(QString newText)
{
    QRegExp regExp(newText, Qt::CaseInsensitive, QRegExp::FixedString);
    mProxyModel->setFilterRegExp(regExp);
}

void
MainWindow::clearStitchFilter()
{
    ui->stitchFilter->clear();
}

void
MainWindow::documentNewChart()
{
    int rows = Settings::inst()->value("rowCount").toInt();
    int stitches = Settings::inst()->value("stitchCount").toInt();
    int incBy = Settings::inst()->value("increaseBy").toInt();

    ui->rows->setValue(rows);
    ui->stitches->setValue(stitches);
    ui->increaseBy->setValue(incBy);

    ui->chartTitle->setText(nextChartName());

    if (ui->newDocument->isVisible())
    {
        QPalette pal = ui->newDocument->palette();
        mNewDocWidgetColor = ui->newDocument->palette().color(ui->newDocument->backgroundRole());
        pal.setColor(ui->newDocument->backgroundRole(),
                     ui->newDocument->palette().highlight().color());
        ui->newDocument->setPalette(pal);
        QTimer::singleShot(1500, this, SLOT(flashNewDocDialog()));
    }
    else
        ui->newDocument->show();
}

void
MainWindow::helpCrochetHelp()
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    QString path = QApplication::applicationDirPath();
    QString file = "";
#ifdef Q_WS_WIN
    file = QString("file://%1/Crochet_Charts_User_Guide_%2.pdf")
               .arg(path)
               .arg(AppInfo::inst()->appVersionShort);
    bool r = QDesktopServices::openUrl(QUrl::fromLocalFile(file));
#endif

#ifdef Q_WS_MAC
    file = QString("file://%1/Crochet Charts_User_Guide_%2.pdf")
               .arg(path)
               .arg(AppInfo::inst()->appVersionShort);
    QDesktopServices::openUrl(QUrl::fromLocalFile(file));
#endif

#ifdef Q_WS_X11
    file = QString("%1/../share/CrochetCharts/CrochetCharts_User_Guide_%2.pdf")
               .arg(path)
               .arg(AppInfo::inst()->appVersionShort);
    QDesktopServices::openUrl(QUrl::fromLocalFile(file));
#endif  // Q_WS_WIN

    QApplication::restoreOverrideCursor();
}

void
MainWindow::helpAbout()
{
    QString aboutInfo = QString(tr("<h1>%1</h1>"
                                   "<p>Version: %2 (built on %3)</p>"
                                   "<p>Copyright (c) %4 %5</p>"
                                   "<p>This software is for creating crochet charts that"
                                   " can be exported in many different file types.</p>")
                                    .arg(qApp->applicationName())
                                    .arg(qApp->applicationVersion())
                                    .arg(AppInfo::inst()->appBuildInfo)
                                    .arg(AppInfo::inst()->projectLife)
                                    .arg(qApp->organizationName()));
    QString fName = Settings::inst()->value("firstName").toString();
    QString lName = Settings::inst()->value("lastName").toString();
    QString email = Settings::inst()->value("email").toString();
    QString sn = Settings::inst()->value("serialNumber").toString();

    QString dedication = tr(
        "<p>This version is dedicated to:<br /> My Grandmother (Aug 20, 1926 - Jan 8, 2013)</p>");
    aboutInfo.append(dedication);

    QString licenseInfo;

    licenseInfo
        = QString(tr("<p>This version is released under the GPLv3 open source license.</p>"));

    aboutInfo.append(licenseInfo);
    QMessageBox::about(this, tr("About Crochet Charts"), aboutInfo);
}

void
MainWindow::closeEvent(QCloseEvent* event)
{
    if (safeToClose())
    {
        Settings::inst()->setValue("geometry", saveGeometry());
        Settings::inst()->setValue("windowState", saveState());

        if (Settings::inst()->files.contains(mFile->fileName.toLower()))
            Settings::inst()->files.remove(mFile->fileName.toLower());

        mFile->cleanUp();

        mPropertiesDock->closing = true;
        QMainWindow::closeEvent(event);
    }
    else
    {
        event->ignore();
    }
}

bool
MainWindow::safeToClose()
{
    // only prompt to save the file if it has tabs.
    if (ui->tabWidget->count() > 0)
    {
        if (isWindowModified())
            return promptToSave();
    }

    return true;
}

bool
MainWindow::promptToSave()
{
    QString niceName = QFileInfo(mFile->fileName).baseName();
    if (niceName.isEmpty())
        niceName = "my design";

    QMessageBox msgbox(this);
    msgbox.setText(tr("The document '%1' has unsaved changes.").arg(niceName));
    msgbox.setInformativeText(tr("Do you want to save the changes?"));
    msgbox.setIcon(QMessageBox::Warning);
    msgbox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    int results = msgbox.exec();

    if (results == QMessageBox::Cancel)
        return false;
    else if (results == QMessageBox::Discard)
        return true;
    else if (results == QMessageBox::Save)
    {
        // FIXME: if the user cancels the fileSave() we should drop them back to the window not
        // close it.
        fileSave();
        return true;
    }

    return false;
}

void
MainWindow::readSettings()
{
    // TODO: For full session restoration reimplement QApplication::commitData()
    // See: http://doc.qt.nokia.com/stable/session.html
    restoreGeometry(Settings::inst()->value("geometry").toByteArray());
    restoreState(Settings::inst()->value("windowState").toByteArray());
}

void
MainWindow::menuToolsAboutToShow()
{
}

void
MainWindow::changeSelectMode(QAction* action)
{
    CrochetTab* tab = curCrochetTab();
    if (tab)
    {
        if (action == ui->actionBoxSelectMode)
            tab->setSelectMode(Scene::BoxSelect);
        else if (action == ui->actionLassoSelectMode)
            tab->setSelectMode(Scene::LassoSelect);
        else if (action == ui->actionLineSelectMode)
            tab->setSelectMode(Scene::LineSelect);
    }
}

void
MainWindow::nextSelectMode()
{
    if (ui->actionBoxSelectMode->isChecked())
        ui->actionLassoSelectMode->trigger();
    else if (ui->actionLassoSelectMode->isChecked())
        ui->actionLineSelectMode->trigger();
    else if (ui->actionLineSelectMode->isChecked())
        ui->actionBoxSelectMode->trigger();
}

void
MainWindow::changeGridMode(QAction* action)
{
    CrochetTab* tab = curCrochetTab();
    if (tab)
    {
        if (action == ui->actionGridNone)
            tab->setGuidelinesType("None");
        else if (action == ui->actionGridSquare)
            tab->setGuidelinesType("Rows");
        else if (action == ui->actionGridRound)
            tab->setGuidelinesType("Rounds");
        else if (action == ui->actionGridTriangle)
            tab->setGuidelinesType("Triangles");
    }
}

void
MainWindow::setSelectedGridMode(QString gmode)
{
    CrochetTab* tab = curCrochetTab();
    if (!tab)
        return;

    ui->actionGridNone->blockSignals(true);
    ui->actionGridSquare->blockSignals(true);
    ui->actionGridRound->blockSignals(true);
    ui->actionGridTriangle->blockSignals(true);

    ui->actionGridNone->setChecked(false);
    ui->actionGridSquare->setChecked(false);
    ui->actionGridRound->setChecked(false);
    ui->actionGridTriangle->setChecked(false);

    if (gmode.compare("None") == 0)
    {
        ui->actionGridNone->setChecked(true);
    }
    else if (gmode.compare("Rows") == 0)
    {
        ui->actionGridSquare->setChecked(true);
    }
    else if (gmode.compare("Rounds") == 0)
    {
        ui->actionGridRound->setChecked(true);
    }
    else if (gmode.compare("Triangles") == 0)
    {
        ui->actionGridTriangle->setChecked(true);
    }
    ui->actionGridNone->blockSignals(false);
    ui->actionGridSquare->blockSignals(false);
    ui->actionGridRound->blockSignals(false);
    ui->actionGridTriangle->blockSignals(false);
}

void
MainWindow::nextGridMode()
{
    if (ui->actionGridNone->isChecked())
        ui->actionGridSquare->trigger();
    else if (ui->actionGridSquare->isChecked())
        ui->actionGridRound->trigger();
    else if (ui->actionGridRound->isChecked())
        ui->actionGridTriangle->trigger();
    else if (ui->actionGridTriangle->isChecked())
        ui->actionGridNone->trigger();
}

void
MainWindow::toolsOptions()
{
    SettingsUi dialog(this);

    dialog.exec();
    if (curCrochetTab())
    {
        curCrochetTab()->sceneUpdate();
        if (dialog.stitchColorUpdated)
            updateDefaultStitchColor(dialog.mOriginalColor, dialog.mNewColor);
    }
}

void
MainWindow::fileOpen()
{
    QString fileLoc = Settings::inst()->value("fileLocation").toString();

    QFileDialog* fd = new QFileDialog(this, tr("Open Pattern File"), fileLoc,
                                      tr("Pattern File (*.pattern);; All files (*.*)"));
    fd->setWindowFlags(Qt::Sheet);
    fd->setObjectName("fileopendialog");
    fd->setViewMode(QFileDialog::List);
    fd->setFileMode(QFileDialog::ExistingFile);
    fd->setAcceptMode(QFileDialog::AcceptOpen);
    fd->open(this, SLOT(loadFile(QString)));
}

void
MainWindow::loadFile(QString fileName)
{
    if (fileName.isEmpty() || fileName.isNull())
        return;

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    if (!Settings::inst()->files.contains(fileName.toLower()))
    {
        if (ui->tabWidget->count() > 0)
        {
            QStringList files;
            files.append(fileName);
            MainWindow* newWin = new MainWindow(files);
            newWin->move(x() + 40, y() + 40);
            newWin->show();
            newWin->raise();
            newWin->activateWindow();
            Settings::inst()->files.insert(fileName.toLower(), newWin);
        }
        else
        {
            ui->newDocument->hide();
            mFile->fileName = fileName;
            int error = mFile->load();

            if (error != FileFactory::No_Error)
            {
                showFileError(error);
                return;
            }

            Settings::inst()->files.insert(mFile->fileName.toLower(), this);
        }

        addToRecentFiles(fileName);

        setApplicationTitle();
        updateMenuItems();
    }
    else
    {
        // show the window if it's already open.
        MainWindow* win = Settings::inst()->files.find(fileName.toLower()).value();
        win->raise();
    }
    QApplication::restoreOverrideCursor();
}

void
MainWindow::fileSave()
{
    if (ui->tabWidget->count() <= 0)
    {
        QMessageBox msgbox;
        msgbox.setText(
            tr("%1 cannot save a document without at least one (1) chart.").arg(qAppName()));
        msgbox.exec();
        return;
    }

    if (mFile->fileName.isEmpty())
        fileSaveAs();
    else
    {
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
        FileFactory::FileError err = mFile->save();
        if (err != FileFactory::No_Error)
        {
            qWarning() << "There was an error saving the file: " << err;
            QMessageBox msgbox;
            msgbox.setText(tr("There was an error saving the file."));
            msgbox.setIcon(QMessageBox::Critical);
            msgbox.exec();
        }

        documentIsModified(false);
        QApplication::restoreOverrideCursor();
    }
}

void
MainWindow::fileSaveAs()
{
    QString fileLoc = Settings::inst()->value("fileLocation").toString();

    QFileDialog* fd
        = new QFileDialog(this, tr("Save Pattern File"), fileLoc,
                          tr("Pattern v1.2 (*.pattern);;Pattern v1.0/v1.1 (*.pattern)"));
    fd->setWindowFlags(Qt::Sheet);
    fd->setObjectName("filesavedialog");
    fd->setViewMode(QFileDialog::List);
    fd->setFileMode(QFileDialog::AnyFile);
    fd->setAcceptMode(QFileDialog::AcceptSave);
    if (mFile->fileName.isEmpty())
        fd->selectFile("my design.pattern");
    else
        fd->selectFile(mFile->fileName);

    fd->open(this, SLOT(saveFileAs(QString)));
}

void
MainWindow::saveFileAs(QString fileName)
{
    if (fileName.isEmpty())
        return;

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    QFileDialog* fd = qobject_cast<QFileDialog*>(sender());

    FileFactory::FileVersion fver = FileFactory::Version_1_2;
    if (fd->selectedNameFilter() == "Pattern v1.0/v1.1 (*.pattern)")
        fver = FileFactory::Version_1_0;

    if (!fileName.endsWith(".pattern", Qt::CaseInsensitive))
    {
        fileName += ".pattern";
    }

    // update the list of open files.
    if (Settings::inst()->files.contains(mFile->fileName.toLower()))
        Settings::inst()->files.remove(mFile->fileName.toLower());
    Settings::inst()->files.insert(fileName.toLower(), this);
    addToRecentFiles(fileName);

    mFile->fileName = fileName;
    mFile->save(fver);

    setApplicationTitle();
    documentIsModified(false);
    QApplication::restoreOverrideCursor();
}

void
MainWindow::showFileError(int error)
{
    QApplication::restoreOverrideCursor();
    QMessageBox msgbox;
    msgbox.setText(tr("There was an error loading the file %1.").arg(mFile->fileName));
    msgbox.setIcon(QMessageBox::Critical);
    if (error == FileFactory::Err_NewerFileVersion)
    {
        msgbox.setInformativeText(tr("It appears to have been created with a newer version of %1.")
                                      .arg(AppInfo::inst()->appName));
    }
    else if (error == FileFactory::Err_WrongFileType)
    {
        msgbox.setInformativeText(
            tr("This file does not appear to be a %1 file.").arg(AppInfo::inst()->appName));
    }
    msgbox.exec();
}

void
MainWindow::viewFullScreen(bool state)
{
    if (state)
        showFullScreen();
    else
        showNormal();
}

void
MainWindow::menuFileAboutToShow()
{
    bool state = hasTab();

    ui->actionSave->setEnabled(state);
    ui->actionSaveAs->setEnabled(state);

    ui->actionPrint->setEnabled(state);
    ui->actionPrintPreview->setEnabled(state);

    ui->actionExport->setEnabled(state);
}

void
MainWindow::menuEditAboutToShow()
{
    bool state = hasTab();

    ui->actionCopy->setEnabled(state);
    ui->actionCut->setEnabled(state);
    ui->actionPaste->setEnabled(state);
    ui->actionInsertImage->setEnabled(state);
}

void
MainWindow::menuViewAboutToShow()
{
    ui->actionShowStitches->setChecked(ui->allStitchesDock->isVisible());
    ui->actionShowPatternColors->setChecked(ui->patternColorsDock->isVisible());
    ui->actionShowPatternStitches->setChecked(ui->patternStitchesDock->isVisible());
    ui->actionShowLayers->setChecked(ui->layersDock->isVisible());

    ui->actionShowUndoHistory->setChecked(mUndoDock->isVisible());

    ui->actionShowEditModeToolbar->setChecked(ui->editModeToolBar->isVisible());
    ui->actionShowMainToolbar->setChecked(ui->mainToolBar->isVisible());

    ui->actionViewFullScreen->setChecked(isFullScreen());

    bool state = hasTab();
    ui->actionZoomIn->setEnabled(state);
    ui->actionZoomOut->setEnabled(state);

    ui->actionShowRowsDock->setChecked(mRowsDock->isVisible());
    ui->actionShowProperties->setChecked(mPropertiesDock->isVisible());
}

void
MainWindow::menuStitchesAboutToShow()
{
    ui->actionShowAlignDock->setChecked(mAlignDock->isVisible());
    ui->actionShowMirrorDock->setChecked(mMirrorDock->isVisible());

    bool hasItems = (mPatternStitches.count() > 0 ? true : false);
    ui->actionReplaceStitch->setEnabled(hasTab() && curCrochetTab() && hasItems);

    hasItems = (mPatternColors.count() > 0 ? true : false);
    ui->actionColorReplacer->setEnabled(hasTab() && curCrochetTab() && hasItems);

    ui->actionGroup->setEnabled(hasTab() && curCrochetTab());
    ui->actionUngroup->setEnabled(hasTab() && curCrochetTab());
}

void
MainWindow::stitchesReplaceStitch()
{
    CrochetTab* tab = curCrochetTab();
    if (!tab)
        return;

    if (mPatternStitches.count() <= 0)
        return;

    QString curStitch = Settings::inst()->value("defaultStitch").toString();

    if (curCrochetTab())
    {
        // Check if the list is empty. If this is not the case, the .first() statement will crash.
        if (!curCrochetTab()->selectedItems().empty())
        {
            QGraphicsItem* i = curCrochetTab()->selectedItems().first();
            if (i && i->type() == Cell::Type)
            {
                Cell* c = qgraphicsitem_cast<Cell*>(i);
                if (c)
                    curStitch = c->name();
            }
        }
    }

    StitchReplacerUi* sr = new StitchReplacerUi(curStitch, mPatternStitches.keys(), this);

    if (sr->exec() == QDialog::Accepted)
    {
        if (!sr->original.isEmpty())
            tab->replaceStitches(sr->original, sr->replacement);
    }
}

void
MainWindow::stitchesReplaceColor()
{
    CrochetTab* tab = curCrochetTab();
    if (!tab)
        return;

    if (mPatternColors.count() <= 0)
        return;

    ColorReplacer* cr = new ColorReplacer(mPatternColors.keys(), this);

    if (cr->exec() == QDialog::Accepted)
    {
        tab->replaceColor(cr->originalColor, cr->newColor, cr->selection);
    }
}

void
MainWindow::fileNew()
{
    if (ui->tabWidget->count() > 0)
    {
        MainWindow* newWin = new MainWindow;
        newWin->move(x() + 40, y() + 40);
        newWin->show();
        newWin->ui->newDocument->show();
    }
    else
    {
        if (ui->newDocument->isVisible())
        {
            QPalette pal = ui->newDocument->palette();
            mNewDocWidgetColor
                = ui->newDocument->palette().color(ui->newDocument->backgroundRole());
            pal.setColor(ui->newDocument->backgroundRole(),
                         ui->newDocument->palette().highlight().color());
            ui->newDocument->setPalette(pal);
            QTimer::singleShot(1500, this, SLOT(flashNewDocDialog()));
        }
        else
            ui->newDocument->show();
    }
}

void
MainWindow::flashNewDocDialog()
{
    QPalette pal = ui->newDocument->palette();
    pal.setColor(ui->newDocument->backgroundRole(), mNewDocWidgetColor);
    ui->newDocument->setPalette(pal);
}

void
MainWindow::newChart()
{
    ui->newDocument->hide();

    int rows = ui->rows->text().toInt();
    int cols = ui->stitches->text().toInt();
    QString defStitch = ui->defaultStitch->currentText();
    QString name = ui->chartTitle->text();
    int incBy = ui->increaseBy->text().toInt();

    QString style = ui->chartStyle->currentText();

    Scene::ChartStyle st = Scene::Rows;

    if (style == tr("Blank"))
        st = Scene::Blank;
    else if (style == tr("Rounds"))
        st = Scene::Rounds;
    else
        st = Scene::Rows;

    if (docHasChartName(name))
        name = nextChartName(name);

    CrochetTab* tab = createTab(st);

    if (name.isEmpty())
        name = nextChartName();

    ui->tabWidget->addTab(tab, name);
    ui->tabWidget->setCurrentWidget(tab);

    QString ddValue = ui->rowSpacing->currentText();
    qreal rowHeight = 96;

    if (ddValue == tr("1 Chain"))
        rowHeight = 32;
    else if (ddValue == tr("2 Chains"))
        rowHeight = 64;
    else if (ddValue == tr("3 Chains"))
        rowHeight = 96;
    else if (ddValue == tr("4 Chains"))
        rowHeight = 128;
    else if (ddValue == tr("5 Chains"))
        rowHeight = 160;
    else if (ddValue == tr("6 Chains"))
        rowHeight = 182;

    tab->createChart(st, rows, cols, defStitch, QSizeF(32, rowHeight), incBy);

    tab->setEditFgColor(ui->fgColor->color());
    tab->setEditBgColor(ui->bgColor->color());

    updateMenuItems();

    setApplicationTitle();
    // Only mark a document as modified if we're adding another tab to it.
    if (ui->tabWidget->count() > 1)
        documentIsModified(true);
}

CrochetTab*
MainWindow::createTab(Scene::ChartStyle style)
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    CrochetTab* tab = new CrochetTab(style, mEditMode, mStitch, mFgColor, mBgColor, ui->tabWidget);
    tab->setPatternStitches(&mPatternStitches);
    tab->setPatternColors(&mPatternColors);

    connect(tab, SIGNAL(chartStitchChanged()), SLOT(updatePatternStitches()));
    connect(tab, SIGNAL(chartColorChanged()), SLOT(updatePatternColors()));
    connect(tab, SIGNAL(chartColorChanged()), mPropertiesDock, SLOT(propertyUpdated()));
    connect(tab, SIGNAL(tabModified(bool)), SLOT(documentIsModified(bool)));
    connect(tab, SIGNAL(guidelinesUpdated(Guidelines)), SLOT(updateGuidelines(Guidelines)));
    connect(tab, SIGNAL(layersChanged(QList<ChartLayer*>&, ChartLayer*)), this,
            SLOT(reloadLayerContent(QList<ChartLayer*>&, ChartLayer*)));
    connect(tab->scene(), SIGNAL(sceneRectChanged(const QRectF&)), mResizeUI,
            SLOT(updateContent()));
    connect(tab->scene(), SIGNAL(showPropertiesSignal()), SLOT(viewMakePropertiesVisible()));

    mUndoGroup.addStack(tab->undoStack());

    QApplication::restoreOverrideCursor();

    return tab;
}

QString
MainWindow::nextChartName(QString baseName)
{
    QString nextName = baseName;

    int i = 1;

    while (docHasChartName(nextName))
    {
        nextName = baseName + QString::number(i);
        i++;
    }

    return nextName;
}

bool
MainWindow::docHasChartName(QString name)
{
    int tabCount = ui->tabWidget->count();
    for (int i = 0; i < tabCount; ++i)
    {
        if (ui->tabWidget->tabText(i) == name)
            return true;
    }

    return false;
}

void
MainWindow::viewShowStitches()
{
    ui->allStitchesDock->setVisible(ui->actionShowStitches->isChecked());
}

void
MainWindow::viewShowPatternColors()
{
    ui->patternColorsDock->setVisible(ui->actionShowPatternColors->isChecked());
}

void
MainWindow::viewShowLayers()
{
    ui->layersDock->setVisible(ui->actionShowLayers->isChecked());
}

void
MainWindow::viewShowPatternStitches()
{
    ui->patternStitchesDock->setVisible(ui->actionShowPatternStitches->isChecked());
}

void
MainWindow::viewShowUndoHistory()
{
    mUndoDock->setVisible(ui->actionShowUndoHistory->isChecked());
}

void
MainWindow::viewMakePropertiesVisible()
{
    ui->actionShowProperties->setChecked(true);
    viewShowProperties();
}

void
MainWindow::viewShowProperties()
{
    mPropertiesDock->setVisible(ui->actionShowProperties->isChecked());
}

void
MainWindow::viewShowResizeDock()
{
    mResizeUI->setVisible(ui->actionShowResizeDock->isChecked());
}

void
MainWindow::viewShowEditModeToolbar()
{
    ui->editModeToolBar->setVisible(ui->actionShowEditModeToolbar->isChecked());
}

void
MainWindow::viewShowMainToolbar()
{
    ui->mainToolBar->setVisible(ui->actionShowMainToolbar->isChecked());
}

void
MainWindow::viewShowAlignDock()
{
    mAlignDock->setVisible(ui->actionShowAlignDock->isChecked());
}

void
MainWindow::viewShowRowsDock()
{
    mRowsDock->setVisible(ui->actionShowRowsDock->isChecked());
}

void
MainWindow::viewShowMirrorDock()
{
    mMirrorDock->setVisible(ui->actionShowMirrorDock->isChecked());
}

void
MainWindow::menuModesAboutToShow()
{
    bool enabled = false;
    bool selected = false;
    bool checkedItem = false;
    bool used = false;

    QStringList modes;
    if (hasTab())
    {
        CrochetTab* tab = qobject_cast<CrochetTab*>(ui->tabWidget->currentWidget());
        modes = tab->editModes();
    }

    foreach (QAction* a, mModeGroup->actions())
    {
        if (modes.contains(a->text()))
            enabled = true;
        else
            enabled = false;

        if (mModeGroup->checkedAction() && mModeGroup->checkedAction() == a)
            checkedItem = true;

        if (enabled && !used && (!mModeGroup->checkedAction() || checkedItem))
        {
            selected = true;
            used = true;
        }

        a->setEnabled(enabled);
        a->setChecked(selected);
        selected = false;
    }

    bool state = hasTab();

    ui->actionBoxSelectMode->setEnabled(state);
    ui->actionLassoSelectMode->setEnabled(state);
    ui->actionLineSelectMode->setEnabled(state);
}

void
MainWindow::changeTabMode(QAction* a)
{
    int mode = -1;

    if (a == ui->actionStitchMode)
        mode = 10;
    if (a == ui->actionColorMode)
        mode = 11;
    else if (a == ui->actionCreateRows)
        mode = 12;
    else if (a == ui->actionAngleMode)
        mode = 14;
    else if (a == ui->actionStretchMode)
        mode = 15;
    else if (a == ui->actionIndicatorMode)
        mode = 16;

    setEditMode(mode);
}

void
MainWindow::setEditMode(int mode)
{
    mEditMode = mode;

    if (mode == 10)
        ui->actionStitchMode->setChecked(true);
    else if (mode == 1)
        ui->actionColorMode->setChecked(true);
    else if (mode == 12)
        ui->actionCreateRows->setChecked(true);
    else if (mode == 14)
        ui->actionAngleMode->setChecked(true);
    else if (mode == 15)
        ui->actionStretchMode->setChecked(true);
    else if (mode == 16)
        ui->actionIndicatorMode->setChecked(true);

    for (int i = 0; i < ui->tabWidget->count(); ++i)
    {
        CrochetTab* tab = qobject_cast<CrochetTab*>(ui->tabWidget->widget(i));
        if (tab)
        {
            tab->setEditMode(mEditMode);
            bool state = mode == 12 ? true : false;
            tab->showRowEditor(state);
        }
    }
}

void
MainWindow::menuChartAboutToShow()
{
    bool state = hasTab();
    ui->actionRemoveTab->setEnabled(state);
    ui->actionEditName->setEnabled(state);
    ui->actionShowChartCenter->setEnabled(state);

    CrochetTab* tab = curCrochetTab();
    if (tab)
    {
        ui->actionShowChartCenter->setChecked(tab->hasChartCenter());
    }
    else
    {
        ui->actionShowChartCenter->setChecked(false);
    }
}

void
MainWindow::chartsShowChartCenter()
{
    CrochetTab* tab = curCrochetTab();
    if (tab)
    {
        tab->setChartCenter(ui->actionShowChartCenter->isChecked());
    }
}

void
MainWindow::chartEditName()
{
    if (!ui->tabWidget->currentWidget())
        return;

    int curTab = ui->tabWidget->currentIndex();
    QString currentName = ui->tabWidget->tabText(curTab);
    bool ok;
    QString newName = QInputDialog::getText(this, tr("Set Chart Name"), tr("Chart name:"),
                                            QLineEdit::Normal, currentName, &ok);
    if (ok && !newName.isEmpty())
    {
        ui->tabWidget->setTabText(curTab, newName);
        if (newName != currentName)
            documentIsModified(true);
    }
}

void
MainWindow::toolsCheckForUpdates()
{
    bool silent = false;
    checkUpdates(silent);
}

void
MainWindow::toolsStitchLibrary()
{
    StitchLibraryUi d(this);
    d.exec();

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    StitchLibrary::inst()->saveAllSets();
    QApplication::restoreOverrideCursor();

    StitchLibrary::inst()->reloadAllStitchIcons();
}

void
MainWindow::viewZoomIn()
{
    CrochetTab* tab = curCrochetTab();
    if (!tab)
        return;
    tab->zoomIn();
}

void
MainWindow::viewZoomOut()
{
    CrochetTab* tab = curCrochetTab();
    if (!tab)
        return;
    tab->zoomOut();
}

CrochetTab*
MainWindow::curCrochetTab()
{
    CrochetTab* tab = qobject_cast<CrochetTab*>(ui->tabWidget->currentWidget());
    return tab;
}

bool
MainWindow::hasTab()
{
    CrochetTab* cTab = qobject_cast<CrochetTab*>(ui->tabWidget->currentWidget());
    if (!cTab)
        return false;

    return true;
}

QTabWidget*
MainWindow::tabWidget()
{
    return ui->tabWidget;
}

void
MainWindow::tabChanged(int newTab)
{
    if (newTab == -1)
        return;

    CrochetTab* tab = qobject_cast<CrochetTab*>(ui->tabWidget->widget(newTab));
    if (!tab)
        return;

    mUndoGroup.setActiveStack(tab->undoStack());
}

void
MainWindow::removeCurrentTab()
{
    removeTab(ui->tabWidget->currentIndex());
}

void
MainWindow::removeTab(int tabIndex)
{
    if (tabIndex < 0)
        return;

    QMessageBox msgbox;

    if (ui->tabWidget->count() == 1)
    {
        msgbox.setText(tr("A document must have at least one (1) chart."));
        msgbox.setIcon(QMessageBox::Information);
        msgbox.exec();
        return;
    }

    msgbox.setWindowTitle(tr("Delete Chart"));
    msgbox.setText(tr("Are you sure you want to delete this chart from the document?"));
    msgbox.setInformativeText(tr("Deleting a chart from the document is a permanent procedure."));
    msgbox.setIcon(QMessageBox::Question);
    /*QPushButton* removeChart =*/msgbox.addButton(tr("Delete the chart"), QMessageBox::AcceptRole);
    QPushButton* keepChart = msgbox.addButton(tr("Keep the chart"), QMessageBox::RejectRole);

    msgbox.exec();

    if (msgbox.clickedButton() == keepChart)
        return;

    // FIXME: Make removing a tab undo-able, using a *tab and chart name.
    ui->tabWidget->removeTab(tabIndex);

    documentIsModified(true);

    // update the title and menus
    setApplicationTitle();
    updateMenuItems();
}

void
MainWindow::addLayer()
{
    QString name = ui->layerName->text();
    CrochetTab* tab = curCrochetTab();
    if (tab)
        tab->addLayer(name);
}

void
MainWindow::removeLayer()
{
    CrochetTab* tab = curCrochetTab();
    if (tab)
        tab->removeSelectedLayer();
}

void
MainWindow::mergeLayer()
{
    QTreeView* view = ui->layersView;
    QStandardItemModel* model = dynamic_cast<QStandardItemModel*>(view->model());

    if (!model)
        return;

    // first we get the currently selected item
    QStandardItem* itemfrom = model->itemFromIndex(view->selectionModel()->currentIndex());
    ChartLayer* from = static_cast<ChartLayer*>(itemfrom->data(Qt::UserRole + 5).value<void*>());

    // and then we fetch the next item
    QList<QStandardItem*> nextRow
        = model->takeRow(view->selectionModel()->currentIndex().row() + 1);
    if (nextRow.count() == 0)
        return;
    QStandardItem* itemto = nextRow.first();
    ChartLayer* to = static_cast<ChartLayer*>(itemto->data(Qt::UserRole + 5).value<void*>());

    // and call the function
    CrochetTab* tab = curCrochetTab();
    if (tab)
        tab->mergeLayer(from->uid(), to->uid());
}

void
MainWindow::selectLayer(const QModelIndex& index)
{
    QTreeView* view = ui->layersView;
    QStandardItemModel* model = dynamic_cast<QStandardItemModel*>(view->model());

    if (!model)
        return;

    QStandardItem* item = model->itemFromIndex(index);
    ChartLayer* layer = static_cast<ChartLayer*>(item->data(Qt::UserRole + 5).value<void*>());

    CrochetTab* tab = curCrochetTab();
    if (tab)
        tab->selectLayer(layer->uid());
}

void
MainWindow::layerModelChanged(const QModelIndex& index)
{
    QTreeView* view = ui->layersView;
    QStandardItemModel* model = dynamic_cast<QStandardItemModel*>(view->model());

    if (!model)
        return;

    QStandardItem* item = model->itemFromIndex(index);

    // scrape the data of the item
    ChartLayer* layer = static_cast<ChartLayer*>(item->data(Qt::UserRole + 5).value<void*>());
    QString name = item->text();
    bool checked = item->checkState() == Qt::Checked;  // item->data(Qt::CheckStateRole).toBool();

    layer->setName(name);
    layer->setVisible(checked);

    curCrochetTab()->editedLayer(layer);
}

void
MainWindow::updatePatternStitches()
{
    if (ui->tabWidget->count() <= 0)
        return;

    // FIXME: this whole thing needs to be worked out, but the very least is make this use a shared
    // icon.
    ui->patternStitches->clear();
    QMapIterator<QString, int> i(mPatternStitches);
    while (i.hasNext())
    {
        i.next();
        QList<QListWidgetItem*> items = ui->patternStitches->findItems(i.key(), Qt::MatchExactly);
        if (items.count() == 0)
        {
            Stitch* s = StitchLibrary::inst()->findStitch(i.key(), true);
            QPixmap pix = QPixmap(QSize(32, 32));

            pix.load(s->file());
            QIcon icon = QIcon(pix);
            QListWidgetItem* item = new QListWidgetItem(icon, i.key(), ui->patternStitches);
            ui->patternStitches->addItem(item);
        }
    }
}

void
MainWindow::updatePatternColors()
{
    if (ui->tabWidget->count() <= 0)
        return;

    ui->patternColors->clear();

    QString prefix = Settings::inst()->value("colorPrefix").toString();

    QStringList keys = mPatternColors.keys();
    QMap<qint64, QString> sortedColors;

    foreach (QString key, keys)
    {
        qint64 added = mPatternColors.value(key).value("added");
        sortedColors.insert(added, key);
    }

    int i = 1;
    QList<qint64> sortedKeys = sortedColors.keys();
    foreach (qint64 sortedKey, sortedKeys)
    {
        QString color = sortedColors.value(sortedKey);
        QList<QListWidgetItem*> items = ui->patternColors->findItems(color, Qt::MatchExactly);
        if (items.count() == 0)
        {
            QPixmap pix = ColorListWidget::drawColorBox(color, QSize(32, 32));
            QIcon icon = QIcon(pix);

            QListWidgetItem* item
                = new QListWidgetItem(icon, prefix + QString::number(i), ui->patternColors);
            item->setToolTip(color);
            item->setData(Qt::UserRole, QVariant(color));
            ui->patternColors->addItem(item);
            ++i;
        }
    }
}

void
MainWindow::documentIsModified(bool isModified)
{
#ifdef Q_OS_MACX
    QString curFile = mFile->fileName;

    if (!curFile.isEmpty())
    {
        if (!isModified)
        {
            setWindowIcon(fileIcon);
        }
        else
        {
            static QIcon darkIcon;

            if (darkIcon.isNull())
                darkIcon = QIcon(":/images/stitchworks-pattern-dark.svg");
            setWindowIcon(darkIcon);
        }
    }
#endif
    setWindowModified(isModified);
}

void
MainWindow::chartCreateRows(bool state)
{
    CrochetTab* tab = curCrochetTab();
    if (tab)
        tab->showRowEditor(state);
}

void
MainWindow::alignSelection(int style)
{
    CrochetTab* tab = curCrochetTab();
    if (tab)
        tab->alignSelection(style);
}

void
MainWindow::distributeSelection(int style)
{
    CrochetTab* tab = curCrochetTab();
    if (tab)
        tab->distributeSelection(style);
}

void
MainWindow::arrangeGrid(QSize grid, QSize alignment, QSize spacing, bool useSelection)
{
    CrochetTab* tab = curCrochetTab();
    if (tab)
        tab->arrangeGrid(grid, alignment, spacing, useSelection);
}

void
MainWindow::copy(int direction)
{
    CrochetTab* tab = curCrochetTab();
    if (tab)
        tab->copy(direction);
}

void
MainWindow::mirror(int direction)
{
    CrochetTab* tab = curCrochetTab();
    if (tab)
        tab->mirror(direction);
}

void
MainWindow::rotate(qreal degrees)
{
    CrochetTab* tab = curCrochetTab();
    if (tab)
        tab->rotate(degrees);
}

void
MainWindow::resize(QRectF scenerect)
{
    CrochetTab* tab = curCrochetTab();
    if (tab)
        tab->resizeScene(scenerect);
}

void
MainWindow::updateGuidelines(Guidelines guidelines)
{
    mPropertiesDock->loadProperties(guidelines);
}

void
MainWindow::copy()
{
    CrochetTab* tab = curCrochetTab();
    if (tab)
        tab->copy();
}

void
MainWindow::cut()
{
    CrochetTab* tab = curCrochetTab();
    if (tab)
        tab->cut();
}

void
MainWindow::paste()
{
    CrochetTab* tab = curCrochetTab();
    if (tab)
        tab->paste();
}

void
MainWindow::insertImage()
{
    // first choose the image we want to add
    QString file = QFileDialog::getOpenFileName(this, tr("Open Image"), QString(),
                                                tr("Image Files (*.png *.jpg *.bmp *.tga *.gif)"));

    CrochetTab* tab = curCrochetTab();

    // get the location of the current center of the scene
    if (!tab)
        return;

    ChartView* view = tab->view();

    if (!view)
        return;

    QPointF pos = view->sceneRect().center();

    tab->insertImage(file, pos);
}

void
MainWindow::group()
{
    CrochetTab* tab = curCrochetTab();
    if (tab)
        tab->group();
}

void
MainWindow::ungroup()
{
    CrochetTab* tab = curCrochetTab();
    if (tab)
        tab->ungroup();
}
