/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
 *
 * ----
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, readStatusite to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "tagclient.h"
#include "tags.h"
#include "config.h"
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QDataStream>
#include <QApplication>
#include <QLocalSocket>
#include <QLocalServer>
#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

#include <QDebug>
static bool debugEnabled=false;
#define DBUG if (debugEnabled) qWarning() << "TagClient" << __FUNCTION__

void TagClient::enableDebug()
{
    debugEnabled=true;
}

static void stopHelper();

static struct TagClientClose { ~TagClientClose() { stopHelper(); } } closer;

static const int constMaxWait=5000;
static QMutex mutex;
static QProcess *proc=0;
static QLocalServer *server=0;
static QLocalSocket *sock=0;

enum ReadStatus {
    Read_Ok,
    Read_Timeout,
    Read_Closed,
    Read_Error
};

static inline bool isRunning() { return proc && QProcess::Running==proc->state() && sock && QLocalSocket::ConnectedState==sock->state(); }

static void stopHelper()
{
    if (sock) {
        DBUG << "socket" << (void *)sock;
        sock->flush();
        sock->close();
        sock->deleteLater();
//        delete socket;
        sock=0;
    }
    if (server) {
        DBUG << "server" << (void *)server;
        server->close();
        server->deleteLater();
//        delete server;
        server=0;
    }
    if (proc) {
        DBUG << "process" << (void *)proc;
        proc->terminate();
        proc->kill();
        proc->deleteLater();
//        delete proc;
        proc=0;
    }
}

static bool startHelper()
{
    DBUG << (void *)proc;
    if (!isRunning()) {
        stopHelper();
        #ifdef Q_OS_WIN
        int currentPid=GetCurrentProcessId();
        #else
        int currentPid=getpid();
        #endif
        DBUG << "create server";
        server=new QLocalServer;

        forever {
            QString name="cantata-tags-"+QString::number(currentPid)+QLatin1Char('-')+QString::number(Utils::random());
            QLocalServer::removeServer(name);
            if (server->listen(name)) {
                DBUG << "Listening on" << server->fullServerName();
                break;
            }
        }

        for (int i=0; i<5; ++i) { // Max5 start attempts...
            DBUG << "start process";
            proc=new QProcess;
            #ifdef Q_OS_WIN
            proc->start(qApp->applicationDirPath()+"/helpers/cantata-tags.exe", QStringList() << server->fullServerName() << QString::number(currentPid));
            #else
            proc->start(INSTALL_PREFIX"/lib/cantata/cantata-tags", QStringList() << server->fullServerName() << QString::number(currentPid));
            #endif
            if (proc->waitForStarted(constMaxWait)) {
                DBUG << "process started, on pid" << proc->pid() << "- wait for helper to connect";
                if (server->waitForNewConnection(constMaxWait)) {
                    sock=server->nextPendingConnection();
                }
                if (sock) {
                    return true;
                } else {
                    DBUG << "helper did not connect";
                }
            } else {
                DBUG << "Failed to start process";
            }
        }
        DBUG << "failed to start";
        stopHelper();
        return false;
    }
    return true;
}

static ReadStatus readReply(QByteArray &data)
{
    DBUG << (void *)proc;
    if (!isRunning()) {
        DBUG << "not running?";
        stopHelper();
        return Read_Closed;
    }
    if (sock->waitForReadyRead(constMaxWait)) {
        data=sock->readAll();
        DBUG << "read reply, bytes:" << data.length();
        return data.isEmpty() ? Read_Error : Read_Ok;
    }
    DBUG << "wait for read failed " << isRunning() << (proc ? (int)proc->state() : 12345);
    stopHelper();
    return !isRunning() ? Read_Closed : Read_Timeout;
}

void TagClient::stop()
{
    QMutexLocker locker(&mutex);
    stopHelper();
}

Song TagClient::read(const QString &fileName)
{
    QMutexLocker locker(&mutex);
    DBUG << fileName;
    Song resp;
    if (startHelper()) {
        QByteArray message;
        QDataStream outStream(&message, QIODevice::WriteOnly);
        outStream << QString(__FUNCTION__) << fileName;
        sock->write(message);

        QByteArray data;
        if (Read_Ok==readReply(data)) {
            QDataStream inStream(data);
            inStream >> resp;
        }
    }
    return resp;
}

QImage TagClient::readImage(const QString &fileName)
{
    QMutexLocker locker(&mutex);
    DBUG << fileName;
    QImage resp;
    if (startHelper()) {
        QByteArray message;
        QDataStream outStream(&message, QIODevice::WriteOnly);
        outStream << QString(__FUNCTION__) << fileName;
        sock->write(message);

        QByteArray data;
        if (Read_Ok==readReply(data)) {
            QDataStream inStream(data);
            inStream >> resp;
        }
    }
    return resp;
}

QString TagClient::readLyrics(const QString &fileName)
{
    QMutexLocker locker(&mutex);
    DBUG << fileName;
    QString resp;
    if (startHelper()) {
        QByteArray message;
        QDataStream outStream(&message, QIODevice::WriteOnly);
        outStream << QString(__FUNCTION__) << fileName;
        sock->write(message);

        QByteArray data;
        if (Read_Ok==readReply(data)) {
            QDataStream inStream(data);
            inStream >> resp;
        }
    }
    return resp;
}

QString TagClient::readComment(const QString &fileName)
{
    QMutexLocker locker(&mutex);
    DBUG << fileName;
    QString resp;
    if (startHelper()) {
        QByteArray message;
        QDataStream outStream(&message, QIODevice::WriteOnly);
        outStream << QString(__FUNCTION__) << fileName;
        sock->write(message);

        QByteArray data;
        if (Read_Ok==readReply(data)) {
            QDataStream inStream(data);
            inStream >> resp;
            DBUG << "resp" << resp;
        }
    }
    return resp;
}

int TagClient::updateArtistAndTitle(const QString &fileName, const Song &song)
{
    QMutexLocker locker(&mutex);
    DBUG << fileName;
    int resp=Tags::Update_Failed;
    if (startHelper()) {
        QByteArray message;
        QDataStream outStream(&message, QIODevice::WriteOnly);
        outStream << QString(__FUNCTION__) << fileName << song;
        sock->write(message);

        QByteArray data;
        ReadStatus readStatus=readReply(data);
        if (Read_Ok==readStatus) {
            QDataStream inStream(data);
            inStream >> resp;
        } else {
            resp=Read_Timeout==readStatus ? Tags::Update_Timedout : Tags::Update_BadFile;
        }
    }
    return resp;
}

int TagClient::update(const QString &fileName, const Song &from, const Song &to, int id3Ver, bool saveComment)
{
    QMutexLocker locker(&mutex);
    DBUG << fileName;
    int resp=Tags::Update_Failed;
    if (startHelper()) {
        QByteArray message;
        QDataStream outStream(&message, QIODevice::WriteOnly);
        outStream << QString(__FUNCTION__) << fileName << from << to << id3Ver << saveComment;
        sock->write(message);

        QByteArray data;
        ReadStatus readStatus=readReply(data);
        if (Read_Ok==readStatus) {
            QDataStream inStream(data);
            inStream >> resp;
        } else {
            resp=Read_Timeout==readStatus ? Tags::Update_Timedout : Tags::Update_BadFile;
        }
    }
    return resp;
}

Tags::ReplayGain TagClient::readReplaygain(const QString &fileName)
{
    QMutexLocker locker(&mutex);
    DBUG << fileName;
    Tags::ReplayGain resp;
    if (startHelper()) {
        QByteArray message;
        QDataStream outStream(&message, QIODevice::WriteOnly);
        outStream << QString(__FUNCTION__) << fileName;
        sock->write(message);

        QByteArray data;
        if (Read_Ok==readReply(data)) {
            QDataStream inStream(data);
            inStream >> resp;
        }
    }
    return resp;
}

int TagClient::updateReplaygain(const QString &fileName, const Tags::ReplayGain &rg)
{
    QMutexLocker locker(&mutex);
    DBUG << fileName;
    int resp=Tags::Update_Failed;
    if (startHelper()) {
        QByteArray message;
        QDataStream outStream(&message, QIODevice::WriteOnly);
        outStream << QString(__FUNCTION__) << fileName << rg;
        sock->write(message);

        QByteArray data;
        ReadStatus readStatus=readReply(data);
        if (Read_Ok==readStatus) {
            QDataStream inStream(data);
            inStream >> resp;
        } else {
            resp=Read_Timeout==readStatus ? Tags::Update_Timedout : Tags::Update_BadFile;
        }
    }
    return resp;
}

int TagClient::embedImage(const QString &fileName, const QByteArray &cover)
{
    QMutexLocker locker(&mutex);
    DBUG << fileName;
    int resp=Tags::Update_Failed;
    if (startHelper()) {
        QByteArray message;
        QDataStream outStream(&message, QIODevice::WriteOnly);
        outStream << QString(__FUNCTION__) << fileName << cover;
        sock->write(message);

        QByteArray data;
        ReadStatus readStatus=readReply(data);
        if (Read_Ok==readStatus) {
            QDataStream inStream(data);
            inStream >> resp;
        } else {
            resp=Read_Timeout==readStatus ? Tags::Update_Timedout : Tags::Update_BadFile;
        }
    }
    return resp;
}

QString TagClient::oggMimeType(const QString &fileName)
{
    QMutexLocker locker(&mutex);
    DBUG << fileName;
    QString resp;
    if (startHelper()) {
        QByteArray message;
        QDataStream outStream(&message, QIODevice::WriteOnly);
        outStream << QString(__FUNCTION__) << fileName;
        sock->write(message);

        QByteArray data;
        if (Read_Ok==readReply(data)) {
            QDataStream inStream(data);
            inStream >> resp;
        }
    }
    return resp;
}
