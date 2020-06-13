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
#include "updater.h"

#include <QtNetwork/QNetworkRequest>

#include <QApplication>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>

#include "appinfo.h"
#include "debug.h"
#include "settings.h"

Updater::Updater(QWidget* parent)
    : QWidget(parent)
    , mSilent(false)
{
    QString url = Settings::inst()->value("updatePage").toString();

    QString os, arch = "";
#if defined(Q_OS_WIN32)
    os = "windows";
#elif defined(Q_OS_LINUX)
    os = "linux";
#elif defined(Q_OS_DARWIN)
    os = "osx";
#endif

#if defined(__x86_64)
    arch = "amd64";
#elif defined(__i386)
    arch = "i386";
#endif

    // software, version, os, serial number, arch
    const auto serialNumber = Settings::inst()->value("serialNumber").toString();
    mUrl = QUrl(QString(url)
                    .arg(AppInfo::inst()->appName.toLower())
                    .arg(AppInfo::inst()->appVersion)
                    .arg(os)
                    .arg(serialNumber)
                    .arg(arch));

    mProgDialog = new QProgressDialog(tr("Checking for updates..."), tr("Cancel"),
                                      /*minimum*/ 0, /*maximum*/ 100, this);
    mProgDialog->setAutoClose(false);
    mProgDialog->setCancelButtonText(tr("Close"));
}

Updater::~Updater() = default;

void
Updater::checkForUpdates(bool silent)
{
    mSilent = silent;

    // schedule the request
    httpRequestAborted = false;
    startRequest();
}

void
Updater::startRequest()
{
    reply = qnam.get(QNetworkRequest(mUrl));
    connect(reply, SIGNAL(finished()), this, SLOT(httpFinished()));
    connect(reply, SIGNAL(readyRead()), this, SLOT(httpReadyRead()));

    mProgDialog->setValue(0);
}

void
Updater::httpFinished()
{
    if (httpRequestAborted)
    {
        mProgDialog->setValue(mProgDialog->maximum());
        mProgDialog->setLabelText(tr("Failed to get update metadata."));
    }
    else
    {
        const auto data = QString(mData);

        if (reply->error())
        {
            mProgDialog->setValue(mProgDialog->maximum());
            mProgDialog->setLabelText(tr("Failed to connect to server '%1'").arg(mUrl.host()));
        }
        else
        {
            mProgDialog->setValue(mProgDialog->maximum() - 5);
            mProgDialog->setLabelText(tr("Parsing response..."));

            const QStringList urls = data.split("::", QString::SkipEmptyParts);
            if (urls.count() == 2)
            {
                QMessageBox msgbox(this);
                msgbox.setIcon(QMessageBox::Information);
                msgbox.setText(tr("There is a new version of %1.").arg(AppInfo::inst()->appName));
                msgbox.setInformativeText(tr("Would you like to download the new version?"));
                msgbox.setDetailedText(urls.last());

                QPushButton* downloadNow
                    = msgbox.addButton(tr("Download the new version"), QMessageBox::ActionRole);
                QPushButton* remindLater
                    = msgbox.addButton(tr("Remind me later"), QMessageBox::RejectRole);

                msgbox.exec();

                if (msgbox.clickedButton() == remindLater)
                    return;

                if (msgbox.clickedButton() == downloadNow)
                    downloadInstaller(QUrl(urls.first()));
            }
            else
            {
                mProgDialog->setValue(mProgDialog->maximum());
                mProgDialog->setLabelText(
                    tr("No updates available for '%1'.").arg(AppInfo::inst()->appName));
            }
        }
    }

    reply->deleteLater();
    reply = nullptr;
}

void
Updater::httpReadyRead()
{
    const auto response = reply->readAll();
    mData.append(response);

    mProgDialog->setValue(mProgDialog->value() + 5);
}

void
Updater::downloadInstaller(QUrl url)
{
    QString fName = url.path().split("/").last();
    QString path = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    installer = new QFile(path + "/" + fName);

    if (!installer->open(QIODevice::WriteOnly))
    {
        QMessageBox::information(this, tr("HTTP"),
                                 tr("Unable to save the file %1: %2.")
                                     .arg(installer->fileName())
                                     .arg(installer->errorString()));
        delete installer;
        installer = nullptr;
        return;
    }

    mProgDialog->setLabelText(tr("Downloading %1").arg(fName));
    connect(mProgDialog, SIGNAL(canceled()), this, SLOT(cancelDownload()));
    mProgDialog->show();

    instReply = qnam.get(QNetworkRequest(url));
    connect(instReply, SIGNAL(downloadProgress(qint64, qint64)), this,
            SLOT(updateDataTransferProgress(qint64, qint64)));
    connect(instReply, SIGNAL(finished()), this, SLOT(httpFinishedInstaller()));
    connect(instReply, SIGNAL(readyRead()), this, SLOT(httpReadyReadInstaller()));
}

void
Updater::cancelDownload()
{
    httpRequestAborted = true;
    // FIXME:crash on cancel.
    // TODO: add some error handeling so it doesn't just sit there when it's not working.
    instReply->abort();
}

void
Updater::updateDataTransferProgress(qint64 readBytes, qint64 totalBytes)
{
    mProgDialog->setMaximum(totalBytes);
    mProgDialog->setValue(readBytes);
}

void
Updater::httpFinishedInstaller()
{
    if (httpRequestAborted)
    {
        if (installer)
        {
            installer->close();
            installer->remove();
            delete installer;
            installer = 0;
        }
        instReply->deleteLater();
        return;
    }

    if (installer->isOpen())
    {
        installer->flush();
        installer->close();
    }

    mProgDialog->hide();
    if (instReply->error())
    {
        installer->remove();
        // TODO: prompt user that the download failed.
        qWarning() << "failed to download the file.";
    }
    else
    {
        launchInstaller();
    }

    instReply->deleteLater();
    instReply = 0;

    installer->deleteLater();
}

void
Updater::httpReadyReadInstaller()
{
    if (installer)
        installer->write(instReply->readAll());
}

void
Updater::launchInstaller()
{
    if (!installer)
    {
        QMessageBox mbox;
        mbox.setText(
            "Could not load the installer. Please download it manually and run the update.");
        mbox.setIcon(QMessageBox::Warning);
        mbox.exec();
        return;
    }

#if defined(Q_OS_WIN32)
    QDesktopServices::openUrl(installer->fileName());

#elif defined(Q_OS_LINUX)
    QDesktopServices::openUrl(installer->fileName());
#elif defined(Q_OS_DARWIN)
    QProcess* installProc = new QProcess(this);
    QString program = "open " + installer->fileName();

    installProc->startDetached(program);
    installProc->waitForStarted();
#endif

    qApp->quit();
}
