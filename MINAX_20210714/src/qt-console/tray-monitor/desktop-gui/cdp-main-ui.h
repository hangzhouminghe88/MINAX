/*
   Bacula(R) - The Network Backup Solution

   Copyright (C) 2000-2020 Kern Sibbald

   The original author of Bacula is Kern Sibbald, with contributions
   from many others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   This notice must be preserved when any source code is
   conveyed and/or propagated.

   Bacula(R) is a registered trademark of Kern Sibbald.
*/

#ifndef CDPUI_H
#define CPDUI_H

#include "../tray-monitor/common.h"
#include "backupservice.h"

#include <QMainWindow>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QStatusBar>
#include <QApplication>

#include <QFileSystemModel>
#include <QTreeView>
#include <QDesktopWidget>
#include <QHeaderView>
#include <QListWidget>
#include <QPushButton>
#include <QInputDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include <QString>
#include <QStringList>

class CdpUi: public QWidget
{
    Q_OBJECT

private:
    QListWidget *_lw;
    BackupService *_bservice;

private slots:

    void handleAddFolder() {
        QFileDialog dialog(this);
        dialog.setLabelText(QFileDialog::Accept, "Watch");
        dialog.setDirectory("/");
        dialog.setFileMode(QFileDialog::Directory);
        dialog.setOption(QFileDialog::ShowDirsOnly);
        int rc = dialog.exec();

        if (rc) {
            QString folder = dialog.selectedFiles()[0];
            POOLMEM *err_msg = _bservice->watch(folder.toLatin1().constData());

            if (err_msg == NULL) {
                _lw->addItem(folder);
                int r = _lw->count() - 1;
                _lw->setCurrentRow(r);
            } else {
                QMessageBox::critical(this, "Error", err_msg);
                free_and_null_pool_memory(err_msg);
            }
        }
    }

    void handleRemoveFolder() {
        int r = _lw->currentRow();

        if (r != -1) {
          QListWidgetItem *item = _lw->takeItem(r);
          _bservice->stopWatching(item->text().toLatin1().constData());
          delete item;
        }
    }

    void handleRestoreFile() {
        QFileDialog dialog(this);
        //dialog.setLabelText(QFileDialog::Accept, "Restore");
        dialog.setDirectory(_bservice->_spoolPath);
        dialog.setFileMode(QFileDialog::ExistingFile);
        dialog.setViewMode(QFileDialog::Detail);
        dialog.setAcceptMode(QFileDialog::AcceptOpen);
        int rc = dialog.exec();

        if (rc) {
            QString file = dialog.selectedFiles()[0];

            QFileDialog dialog(this);
            dialog.setDirectory("/");
            dialog.setFileMode(QFileDialog::Directory);
            dialog.setOption(QFileDialog::ShowDirsOnly);
            rc = dialog.exec();

            if (rc) {
                QString folder = dialog.selectedFiles()[0];

                if (QFile::copy(file, folder)) {
                   Dmsg2(0, "Copied file %s into %s\n",
                           file.toLatin1().constData(),
                           folder.toLatin1().constData()
                         );
                } else {
                    Dmsg2(0, "Could not copy file %s into %s\n",
                           file.toLatin1().constData(),
                           folder.toLatin1().constData()
                         );
                }
            }
        }
    }

private:

    void setupButtons(QHBoxLayout *parentBox) {
        QPushButton *add = new QPushButton("Add", this);
        QPushButton *remove = new QPushButton("Remove", this);
        //QPushButton *restore = new QPushButton("Restore", this);

        QVBoxLayout *vbox = new QVBoxLayout();
        vbox->setSpacing(10);
        vbox->setSpacing(3);
        vbox->addStretch(1);
        vbox->addWidget(add);
        vbox->addWidget(remove);
        //vbox->addWidget(restore);
        vbox->addStretch(1);
        connect(add, SIGNAL(clicked()), this, SLOT(handleAddFolder()));
        connect(remove, SIGNAL(clicked()), this, SLOT(handleRemoveFolder()));
        //connect(restore, &QPushButton::clicked, this, &CdpUi::handleRestoreFile);
        parentBox->addLayout(vbox);
    }

    /*Reads watched folders inside the Backup Service and show them to the user */
    void setupFoldersList(QHBoxLayout *parentLayout) {
        _lw = new QListWidget(this);

        std::map<std::string, FolderWatcher *>::iterator it;

        for (it = _bservice->_watchers.begin(); it != _bservice->_watchers.end(); it++) {
            QString fpath = QString::fromStdString(it->first);
            _lw->addItem(fpath);
        }

        parentLayout->addWidget(_lw);
        parentLayout->addSpacing(15);
    }

    void setupSpooldirLink(QVBoxLayout *parentBox, const char *spoolPath) {
        POOLMEM *linkText = get_pool_memory(PM_FNAME);
        Mmsg(linkText, "Spool Directory = <a href=\"file:///%s\">%s</a>", spoolPath, spoolPath);
        QLabel *slink = new QLabel();
        slink->setText(linkText);
        slink->setOpenExternalLinks(true);
        slink->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
        parentBox->addWidget(slink);
    }

    void setupJournalLink(QVBoxLayout *parentBox, const char *journalPath) {
        POOLMEM *linkText = get_pool_memory(PM_FNAME);
        Mmsg(linkText, "Journal File = <a href=\"file:///%s\">%s</a>", journalPath, journalPath);
        QLabel *jlink = new QLabel();
        jlink->setText(linkText);
        jlink->setOpenExternalLinks(true);
        jlink->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
        parentBox->addWidget(jlink);
    }

    void setupTitle(QVBoxLayout *parentBox) {
        QLabel *title = new QLabel();
        title->setText("You can select folders to be watched for file changes.\n"
                       "The Tray Monitor will create copies of the modified files \n"
                       "so that they can be backed up by Bacula.");
        parentBox->addWidget(title);
    }

    void setupContent(QVBoxLayout *parentBox) {
        QHBoxLayout *hbox = new QHBoxLayout();
        this->setupButtons(hbox);
        this->setupFoldersList(hbox);
        parentBox->addLayout(hbox);
    }

public:

    CdpUi(BackupService *bs, QWidget *parent = NULL): QWidget(parent) {
      this->_bservice = bs;
      this->setWindowTitle(QApplication::translate("CDPClient", "Watched Folders", 0));
      this->setToolTip("Watched Folders");

      QVBoxLayout *rootBox = new QVBoxLayout(this);
      setupSpooldirLink(rootBox, _bservice->_spoolPath);
      setupJournalLink(rootBox, _bservice->_journal->_jPath);
      setupTitle(rootBox);
      setupContent(rootBox);
    }
};

#endif
