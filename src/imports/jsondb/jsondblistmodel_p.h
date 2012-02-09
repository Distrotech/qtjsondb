/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
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
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef JSONDBLISTMODEL_P_H
#define JSONDBLISTMODEL_P_H

#include <QHash>
#include <QMultiMap>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QPointer>

#include "jsondb-client.h"
#include "private/jsondb-connection_p.h"
#include "jsondbmodelutils.h"

QT_BEGIN_NAMESPACE_JSONDB

class JsonDbListModelPrivate
{
    Q_DECLARE_PUBLIC(JsonDbListModel)
public:
    JsonDbListModel *q_ptr;

    QPointer<JsonDbPartition> partitionObject;
    QPointer<JsonDbPartition> defaultPartitionObject;

    int chunkSize;
    int lowWaterMark;
    int maxCacheSize;
    int totalRowCount;
    int cacheStart;
    int cacheEnd;
    int newChunkOffset;

    QSet<int> requestIds;
    QMap<int, CallbackInfo> updateRequestIds;
    QMap<int, CallbackInfo> sectionIndexRequestIds;
    int totalCountRequestId;

    QHash<QString,QVariantMap> data; // cache that holds all the items.
    QMultiMap<JsonDbSortKey,QString> objectUuids; // sort value -> uuid
    QMap<QString,JsonDbSortKey> objectSortValues; // uuid -> sort value
    QList<QString> cachedUuids; // in the correct sort order.

    QStringList orderDirections;
    QStringList orderProperties;
    QList<QStringList> orderPaths;
    QString query;
    QString queryWithoutSort;

    QVariantMap lastFetchedItem;
    int lastFetchedIndex;

    bool requestInProgress;
    bool componentComplete;
    bool resetModel;
    bool updateRecieved;
    bool totalRowCountRecieved;

    QString notifyUuid;
    QVariantMap roleMap;

    QHash<int,QByteArray> roleNames;
    QHash<int,QStringList> properties;
    QList<NotifyItem> pendingNotifications;

    enum State {
        None,
        Querying,
        Ready
    };
    State state;

    JsonDbClient jsonDb;
    JsonDbConnection *jsonDbConnection;

public:
    JsonDbListModelPrivate(JsonDbListModel *q);
    ~JsonDbListModelPrivate();
    void init();
    void clearCache(int newStart = 0);
    int makeSpaceFor(int count, int insertAt);
    void removeItem(int index);
    int findSortedPosition(const QString& uuid);
    JsonDbSortKey sortKey(const QVariantMap &object);
    void insertItem(const QVariantMap &item, bool emitCountChanged = true);
    void deleteItem(const QVariantMap &item, bool emitCountChanged = true);
    void updateItem(const QVariantMap &item);
    QVariantMap getItem(int index, bool handleCacheMiss, bool &cacheMiss);
    QVariantMap getItem(const QModelIndex &modelIndex, int role, bool handleCacheMiss, bool &cacheMiss);
    void set(int index, const QJSValue& valuemap, const QJSValue &successCallback,
             const QJSValue &errorCallback);
    void setProperty(int index, const QString& property, const QVariant& value, const QJSValue &successCallback,
                     const QJSValue &errorCallback);

    void fetchChunkSynchronous(int offset);
    void updateCache(const QVariantMap &v);
    void resetModelFinished();

    // private slots
    void _q_jsonDbResponse(int , const QVariant &);
    void _q_jsonDbNotified(const QString&, const QVariant &, const QString &);
    void _q_jsonDbErrorResponse(int , int, const QString&);

    void _q_requestAnotherChunk(int offset);
    void _q_dbNotified(const QString &notify_uuid, const QtAddOn::JsonDb::JsonDbNotification &_notification);
    void _q_dbNotifyReadyResponse(int /* id */, const QVariant &/* result */);
    void _q_dbNotifyErrorResponse(int id, int code, const QString &message);

    void sendNotifications(const QVariant &v, JsonDbClient::NotifyType action);

    void populateModel();
    void createOrUpdateNotification();
    bool findSortOrder();

    inline int itemsInCache() { return cacheEnd - cacheStart;}
};

QT_END_NAMESPACE_JSONDB

#endif // JSONDBLISTMODEL_P_H
