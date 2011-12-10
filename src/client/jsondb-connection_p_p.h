/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the QtAddOn.JsonDb module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef JSONDB_CONNECTION_P_P_H
#define JSONDB_CONNECTION_P_P_H

#include <QObject>
#include <QVariant>
#include <QSet>
#include <QStringList>
#include <QLocalSocket>
#include <QTcpSocket>
#include <QScopedPointer>

#include "jsondb-global.h"
#include "jsondb-connection_p.h"

#include "qsonstream.h"

namespace QtAddOn { namespace JsonDb {

class QsonObject;
class QsonMap;

class JsonDbConnectionPrivate
{
    Q_DECLARE_PUBLIC(JsonDbConnection)
public:
    JsonDbConnectionPrivate(JsonDbConnection *q)
        : q_ptr(q), socket(0), tcpSocket(0), mStream(0, q), mId(1), status(JsonDbConnection::Null),
          tokenRequestId(-1)
    { }
    ~JsonDbConnectionPrivate()
    { }

    void _q_onConnected();
    void _q_onDisconnected();
    void _q_onReceiveMessage(const QsonObject &);
    void _q_onError(QLocalSocket::LocalSocketError error);

    static QString sDefaultToken;

    JsonDbConnection *q_ptr;
    QLocalSocket *socket;
    QTcpSocket *tcpSocket;
    QsonStream mStream;
    int mId;
    QString mToken;
    JsonDbConnection::Status status;
    int tokenRequestId;
};

/*!
 * \internal
 * The sync class forces a response before the program will continue
 */
class JsonDbSyncCall : public QObject
{
    Q_OBJECT
    friend class JsonDbConnection;
public:
    QT_DEPRECATED
    JsonDbSyncCall(const QVariantMap &dbrequest, QVariant &result);
    JsonDbSyncCall(const QVariantMap *dbrequest, QVariant *result);
    JsonDbSyncCall(const QsonMap *dbrequest, QsonObject *result);
    ~JsonDbSyncCall();
public slots:
    void createSyncRequest();
    void createSyncQsonRequest();
    void handleResponse( int id, const QVariant& data );
    void handleResponse( int id, const QsonObject& data );
    void handleError( int id, int code, const QString& message );
private:
    int                 mId;
    const QVariantMap   *mDbRequest;
    const QsonMap       *mDbQsonRequest;
    QVariant            *mResult;
    QsonObject          *mQsonResult;
    JsonDbConnection    *mSyncJsonDbConnection;
};

} } // end namespace QtAddOn::JsonDb

#endif /* JSONDB_CONNECTION_P_P_H */