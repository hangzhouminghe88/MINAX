#include "fd-config-ui-controller.h"

FdConfigUiController::FdConfigUiController(QObject *parent):
    QObject(parent)
{}

void FdConfigUiController::readFileDaemonConfig()
{
    QString fileName = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + "/etc/bacula-fd.conf";
    if (QFile::exists(fileName)) {
        QFile file(fileName);
        if (file.open(QFile::ReadOnly)) {
            QTextStream stream(&file);
            QString fdConfig = stream.readAll();
            file.close();
            emit fileDaemonConfigRead(fdConfig);
        }
    } else {
        emit fileDaemonConfigRead("ERROR - File Daemon config file not found");
    }
}

void FdConfigUiController::writeFileDaemonConfig(QString fcontents)
{

    QString filePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + "/etc/bacula-fd.conf";
    QFile file(filePath);

    if (!file.open(QFile::WriteOnly | QFile::Truncate | QFile::Text)) {
        return;
    }

    file.write(fcontents.toUtf8().constData());
    file.close();
}

FdConfigUiController::~FdConfigUiController()
{}
