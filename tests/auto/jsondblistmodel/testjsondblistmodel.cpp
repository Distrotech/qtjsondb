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

#include <QtTest/QtTest>
#include <QJSEngine>
#include "testjsondblistmodel.h"

#include "../../shared/util.h"
#include <QDeclarativeEngine>
#include <QDeclarativeComponent>
#include <QDeclarativeContext>
#include <QDir>
#include "json.h"

static const char dbfile[] = "dbFile-jsondb-listmodel";
ModelData::ModelData(): engine(0), component(0), model(0)
{
}

ModelData::~ModelData()
{
    if (model)
        delete model;
    if (component)
        delete component;
    if (engine)
        delete engine;
}

QVariant get(QObject* object, int index, QString propertyName)
{
    QVariant retVal;
    QMetaObject::invokeMethod(object, "get", Qt::DirectConnection,
                              Q_RETURN_ARG(QVariant, retVal),
                              Q_ARG(int, index),
                              Q_ARG(QString, propertyName));
    return retVal;
}

void set(QObject* object, int index, QJSValue value)
{
    QMetaObject::invokeMethod(object, "set", Qt::DirectConnection,
                              Q_ARG(int, index),
                              Q_ARG(QJSValue, value));
}

void listSetProperty(QObject* object, int index, QString propertyName, QVariant value)
{
    QMetaObject::invokeMethod(object, "setProperty", Qt::DirectConnection,
                              Q_ARG(int, index),
                              Q_ARG(QString, propertyName),
                              Q_ARG(QVariant, value));
}

TestJsonDbListModel::TestJsonDbListModel()
    : mWaitingForNotification(false), mWaitingForDataChange(false), mWaitingForRowsRemoved(false)
{
    QDeclarativeEngine *engine = new QDeclarativeEngine();
    QStringList pluginPaths = engine->importPathList();
    for (int i=0; (i<pluginPaths.count() && mPluginPath.isEmpty()); i++) {
        QDir dir(pluginPaths[i]+"/QtJsonDb");
        dir.setFilter(QDir::Files | QDir::NoSymLinks);
        QFileInfoList list = dir.entryInfoList();
        for (int i = 0; i < list.size(); ++i) {
            QString error;
            if (engine->importPlugin(list.at(i).absoluteFilePath(), QString("QtJsonDb"), &error)) {
                mPluginPath = list.at(i).absoluteFilePath();
                break;
            }
        }
    }
    delete engine;
}

TestJsonDbListModel::~TestJsonDbListModel()
{
}

void TestJsonDbListModel::deleteDbFiles()
{
    // remove all the test files.
    QDir currentDir;
    QStringList nameFilter;
    nameFilter << QString("*.db");
    nameFilter << "objectFile.bin" << "objectFile2.bin";
    QFileInfoList databaseFiles = currentDir.entryInfoList(nameFilter, QDir::Files);
    foreach (QFileInfo fileInfo, databaseFiles) {
        QFile file(fileInfo.fileName());
        file.remove();
    }
}

QVariant TestJsonDbListModel::readJsonFile(const QString& filename)
{
    QString filepath = findFile(SRCDIR, filename);
    QFile jsonFile(filepath);
    jsonFile.open(QIODevice::ReadOnly);
    QByteArray json = jsonFile.readAll();
    JsonReader parser;
    bool ok = parser.parse(json);
    if (!ok) {
      qDebug() << filepath << parser.errorString();
    }
    return parser.result();
}

void TestJsonDbListModel::connectListModel(QAbstractItemModel *model)
{
    connect(model, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(dataChanged(QModelIndex,QModelIndex)));
    connect(model, SIGNAL(modelReset()), this, SLOT(modelReset()));
    connect(model, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(rowsInserted(QModelIndex,int,int)));
    connect(model, SIGNAL(rowsRemoved(QModelIndex,int,int)), this, SLOT(rowsRemoved(QModelIndex,int,int)));
    connect(model, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)),
            this, SLOT(rowsMoved(QModelIndex,int,int,QModelIndex,int)));
}

void TestJsonDbListModel::initTestCase()
{
    // make sure there is no old db files.
    deleteDbFiles();

    QString socketName = QString("testjsondb_%1").arg(getpid());
    mProcess = launchJsonDbDaemon(JSONDB_DAEMON_BASE, socketName, QStringList() << dbfile);

    mClient = new JsonDbClient(this);
    connect(mClient, SIGNAL(notified(QString,QtAddOn::JsonDb::JsonDbNotification)),
            this, SLOT(notified(QString,QtAddOn::JsonDb::JsonDbNotification)));
    connect( mClient, SIGNAL(response(int, const QVariant&)),
             this, SLOT(response(int, const QVariant&)));
    connect( mClient, SIGNAL(error(int, int, const QString&)),
             this, SLOT(error(int, int, const QString&)));

    // Create the shared Partitions
    QVariantMap item;
    item.insert("_type", "Partition");
    item.insert("name", "com.nokia.shared.1");
    int id = mClient->create(item, QString("com.nokia.qtjsondb.System"));
    waitForResponse1(id);

}

QAbstractItemModel *TestJsonDbListModel::createModel()
{
    ModelData *newModel = new ModelData();
    newModel->engine = new QDeclarativeEngine();
    QString error;
    if (!newModel->engine->importPlugin(mPluginPath, QString("QtJsonDb"), &error)) {
        qDebug()<<"Unable to load the plugin :"<<error;
        delete newModel->engine;
        return 0;
    }
    newModel->component = new QDeclarativeComponent(newModel->engine);
    newModel->component->setData("import QtQuick 2.0\nimport QtJsonDb 1.0 as JsonDb \n JsonDb.JsonDbListModel {id: contactsModel; partition: JsonDb.Partition {name: \"com.nokia.shared.1\"}}", QUrl());
    newModel->model = newModel->component->create();
    if (newModel->component->isError())
        qDebug() << newModel->component->errors();
    mModels.append(newModel);
    return (QAbstractItemModel*)(newModel->model);
}

void TestJsonDbListModel::deleteModel(QAbstractItemModel *model)
{
    for (int i = 0; i < mModels.count(); i++) {
        if (mModels[i]->model == model) {
            ModelData *modelData = mModels.takeAt(i);
            delete modelData;
            return;
        }
    }
}

void TestJsonDbListModel::cleanupTestCase()
{
    if (mProcess) {
        mProcess->kill();
        mProcess->waitForFinished();
        delete mProcess;
    }

    deleteDbFiles();
}

void TestJsonDbListModel::waitForItemsCreated(int items)
{
    mItemsCreated = 0;
    while(mItemsCreated != items)
        mEventLoop.processEvents(QEventLoop::AllEvents, mClientTimeout);
}

void TestJsonDbListModel::waitForExitOrTimeout()
{
    QTimer timer;
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timeout()));
    QObject::connect(&timer, SIGNAL(timeout()), &mEventLoop, SLOT(quit()));
    timer.start(mClientTimeout);
    mElapsedTimer.start();
    mEventLoop.exec(QEventLoop::AllEvents);
}

// Create items in the model.
void TestJsonDbListModel::createItem()
{
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Charlie");
    int id = mClient->create(item,  "com.nokia.shared.1");

    waitForResponse1(id);

    QAbstractItemModel *listModel = createModel();
    if (!listModel) return;
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    connectListModel(listModel);

    // now start it working
    QCOMPARE(listModel->property("count").toInt(), 0);

    waitForExitOrTimeout();
    QCOMPARE(listModel->property("count").toInt(), 1);

    item.insert("_type", __FUNCTION__);
    item.insert("name", "Baker");
    id = mClient->create(item, "com.nokia.shared.1");
    waitForResponse1(id);

    waitForExitOrTimeout();;
    QCOMPARE(listModel->property("count").toInt(), 2);
    deleteModel(listModel);
}

// Create an item and then update it.
void TestJsonDbListModel::updateItemClient()
{
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Charlie");
    int id = mClient->create(item, "com.nokia.shared.1");
    waitForResponse1(id);

    QAbstractItemModel *listModel = createModel();
    if (!listModel) return;
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    connectListModel(listModel);

    // now start it working
    QCOMPARE(listModel->property("count").toInt(), 0);

    waitForExitOrTimeout();
    QCOMPARE(listModel->property("count").toInt(), 1);

    item.insert("_uuid", mLastUuid);
    item.insert("name", "Baker");

    mWaitingForDataChange = true;
    id = mClient->update(item, "com.nokia.shared.1");
    waitForResponse1(id);

    while (mWaitingForDataChange)
        mEventLoop.processEvents(QEventLoop::AllEvents);

    QCOMPARE(listModel->property("count").toInt(), 1);
    QCOMPARE(get(listModel, 0, "_uuid").toString(), mLastUuid);
    QCOMPARE(get(listModel, 0, "_type").toString(), QLatin1String(__FUNCTION__));
    QCOMPARE(get(listModel, 0, "name").toString(), QLatin1String("Baker"));
    deleteModel(listModel);
}


// Create an item and then update it.
void TestJsonDbListModel::updateItemSet()
{
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Charlie");
    item.insert("phone", "123456789");
    int id = mClient->create(item, "com.nokia.shared.1");
    waitForResponse1(id);

    QAbstractItemModel *listModel = createModel();
    if (!listModel) return;
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name" << "phone");
    listModel->setProperty("roleNames", roleNames);
    connectListModel(listModel);

    // now start it working
    QCOMPARE(listModel->property("count").toInt(), 0);

    waitForExitOrTimeout();
    QCOMPARE(listModel->property("count").toInt(), 1);

    QJSEngine engine;
    QJSValue value = engine.newObject();

    value.setProperty("name", "Baker");
    value.setProperty("phone", "987654321");

    mWaitingForDataChange = true;

    set(listModel, 0, value);

    while (mWaitingForDataChange)
        mEventLoop.processEvents(QEventLoop::AllEvents);

    QCOMPARE(listModel->property("count").toInt(), 1);
    QCOMPARE(get(listModel, 0, "_type").toString(), QLatin1String(__FUNCTION__));
    QCOMPARE(get(listModel, 0, "name").toString(), QLatin1String("Baker"));
    QCOMPARE(get(listModel, 0, "phone").toString(), QLatin1String("987654321"));
    deleteModel(listModel);
}
void TestJsonDbListModel::updateItemSetProperty()
{
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Charlie");
    item.insert("phone", "123456789");
    int id = mClient->create(item, "com.nokia.shared.1");
    waitForResponse1(id);

    QAbstractItemModel *listModel = createModel();
    if (!listModel) return;
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name" << "phone");
    listModel->setProperty("roleNames", roleNames);
    connectListModel(listModel);

    // now start it working
    QCOMPARE(listModel->property("count").toInt(), 0);

    waitForExitOrTimeout();
    QCOMPARE(listModel->property("count").toInt(), 1);

    mWaitingForDataChange = true;

    listSetProperty(listModel, 0, QString("phone"), QVariant("987654321"));

    while (mWaitingForDataChange)
        mEventLoop.processEvents(QEventLoop::AllEvents);

    QCOMPARE(listModel->property("count").toInt(), 1);
    QCOMPARE(get(listModel, 0, "_type").toString(), QLatin1String(__FUNCTION__));
    QCOMPARE(get(listModel, 0, "name").toString(), QLatin1String("Charlie"));
    QCOMPARE(get(listModel, 0, "phone").toString(), QLatin1String("987654321"));
    deleteModel(listModel);
}

void TestJsonDbListModel::deleteItem()
{
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Charlie");
    int id = mClient->create(item, "com.nokia.shared.1");
    waitForResponse1(id);

    QAbstractItemModel *listModel = createModel();
    if (!listModel) return;
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name");
    listModel->setProperty("roleNames", roleNames);
    connectListModel(listModel);

    // now start it working
    waitForExitOrTimeout();
    QCOMPARE(listModel->property("count").toInt(), 1);

    item.insert("name", "Baker");
    id = mClient->create(item,"com.nokia.shared.1");
    waitForResponse1(id);
    waitForExitOrTimeout();
    QCOMPARE(listModel->property("count").toInt(), 2);

    mWaitingForRowsRemoved = true;
    item.insert("_uuid", mLastUuid);
    id = mClient->remove(item, "com.nokia.shared.1");

    waitForResponse1(id);
    while (mWaitingForRowsRemoved)
        mEventLoop.processEvents(QEventLoop::AllEvents);

    QCOMPARE(listModel->property("count").toInt(), 1);
    QCOMPARE(get(listModel, 0, "_type").toString(), QLatin1String(__FUNCTION__));
    QCOMPARE(get(listModel, 0, "name").toString(), QLatin1String("Charlie"));
    deleteModel(listModel);
}


void TestJsonDbListModel::sortedQuery()
{
    int id = 0;
    for (int i = 0; i < 1000; i++) {
        QVariantMap item;
        item.insert("_type", "RandNumber");
        item.insert("number", i);
        id = mClient->create(item, "com.nokia.shared.1");
        waitForResponse1(id);
    }

    QAbstractItemModel *listModel = createModel();
    if (!listModel) return;

    connectListModel(listModel);

    QStringList rolenames;
    rolenames << "_uuid" << "_type" << "number";
    listModel->setProperty("roleNames", rolenames);
    listModel->setProperty("query", "[?_type=\"RandNumber\"][/number]");

    waitForExitOrTimeout();
    QCOMPARE(listModel->property("count").toInt(), 1000);
    for (int i = 0; i < 1000; i++)
        QCOMPARE(get(listModel, i,"number").toInt(), i);

    listModel->setProperty("query","[?_type=\"RandNumber\"][\\number]");
    waitForExitOrTimeout();
    for (int i = 0; i < 1000; i++)
        QCOMPARE(get(listModel, i,"number").toInt(), 999-i);

    QCoreApplication::instance()->processEvents();
    deleteModel(listModel);
}


void TestJsonDbListModel::ordering()
{
    for (int i = 9; i >= 1; --i) {
        QVariantMap item;
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Charlie");
        item.insert("order", QString::number(i));
        int id = mClient->create(item, "com.nokia.shared.1");
        waitForResponse1(id);
    }

    QAbstractItemModel *listModel = createModel();
    if (!listModel) return;
    listModel->setProperty("query", QString("[?_type=\"%1\"][/order]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name" << "order");
    listModel->setProperty("roleNames",roleNames);
    connectListModel(listModel);

    // now start it working
    QCOMPARE(listModel->property("count").toInt(), 0);

    waitForExitOrTimeout();

    QStringList expectedOrder = QStringList() << "1" << "2" << "3" << "4" <<
        "5" << "6" << "7" << "8" << "9";
    QCOMPARE(getOrderValues(listModel), expectedOrder);
    mWaitingForDataChange = true;
    {
        QVariant uuid = get(listModel, 4, "_uuid");
        QVERIFY(!uuid.toString().isEmpty());

        QVariantMap item;
        item.insert("_uuid", uuid);
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Charlie");
        item.insert("order", "99");  // move it to the end
        int id = mClient->update(item, "com.nokia.shared.1");
        waitForResponse1(id);
    }

    while (mWaitingForDataChange)
        mEventLoop.processEvents(QEventLoop::AllEvents);

    expectedOrder = QStringList() << "1" << "2" << "3" <<
        "4" << "6" << "7" << "8" << "9" << "99";
    QCOMPARE(getOrderValues(listModel), expectedOrder);
    mWaitingForDataChange = true;
    {
        QVariant uuid =  get(listModel, 8, "_uuid");
        QVERIFY(!uuid.toString().isEmpty());

        QVariantMap item;
        item.insert("_uuid", uuid);
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Charlie");
        item.insert("order", "22");    // move it after "2"
        int id = mClient->update(item, "com.nokia.shared.1");
        waitForResponse1(id);
    }

    while (mWaitingForDataChange)
        mEventLoop.processEvents(QEventLoop::AllEvents);

    expectedOrder = QStringList() << "1" << "2" << "22" << "3" <<
        "4" << "6" << "7" << "8" << "9";
    QCOMPARE(getOrderValues(listModel), expectedOrder);
    mWaitingForDataChange = true;
    {
        QVariant uuid =  get(listModel, 5, "_uuid");
        QVERIFY(!uuid.toString().isEmpty());

        QVariantMap item;
        item.insert("_uuid", uuid);
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Charlie");
        item.insert("order", "0");    // move it to the beginning
        int id = mClient->update(item, "com.nokia.shared.1");
        waitForResponse1(id);
    }

    while (mWaitingForDataChange)
        mEventLoop.processEvents(QEventLoop::AllEvents);

    expectedOrder = QStringList() << "0" << "1" << "2" << "22" << "3" <<
        "4" << "7" << "8" << "9";
    QCOMPARE(getOrderValues(listModel), expectedOrder);
    deleteModel(listModel);

}

void TestJsonDbListModel::itemNotInCache()
{
    QVariantList itemList;
    for (int i = 0; i < 1000; i++) {
        QVariantMap item;
        item.insert("_type", __FUNCTION__);
        item.insert("name", "Number");
        item.insert("order", i);
        itemList << item;
    }
    int id = mClient->create(itemList, "com.nokia.shared.1");
    waitForResponse1(id);

    QAbstractItemModel *listModel = createModel();
    if (!listModel) return;
    connectListModel(listModel);
    listModel->setProperty("limit", 80);
    listModel->setProperty("query", QString("[?_type=\"%1\"][/order]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name" << "order");
    listModel->setProperty("roleNames",roleNames);
    waitForExitOrTimeout();

    QCOMPARE(listModel->property("count").toInt(), 1000);

    // Make sure that the first items in the list is in the cache.
    QVariant result = listModel->data(listModel->index(10,0), 3);
    QVERIFY(result.isValid());
    QCOMPARE(result.toInt(), 10);
    // This item should not be in the cache now.
    QVariant res = listModel->data(listModel->index(960,0), 3);
    QCOMPARE(res.toInt(), 960);
    deleteModel(listModel);
}

void TestJsonDbListModel::roles()
{
    QVariantMap item;
    item.insert("_type", __FUNCTION__);
    item.insert("name", "Charlie");
    item.insert("phone", "123456789");
    int id = mClient->create(item, "com.nokia.shared.1");

    waitForResponse1(id);

    QAbstractItemModel *listModel = createModel();
    if (!listModel) return;
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));

    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "name" << "phone");
    listModel->setProperty("roleNames",roleNames);
    connectListModel(listModel);

    // now start it working
    waitForExitOrTimeout();
    QCOMPARE(listModel->property("count").toInt(), 1);

    QVariantMap roles = listModel->property("roleNames").toMap();
    QCOMPARE(roles.size(), 4);
    QVERIFY(roles.contains("name")) ;
    QVERIFY(roles.contains("phone"));
    deleteModel(listModel);
}

void TestJsonDbListModel::totalRowCount()
{
    int id = 0;
    QVariantList insertedItems;
    for (int i = 0; i < 10; i++) {
        QVariantMap item;
        item.insert("_type", __FUNCTION__);
        item.insert("order", i);
        id = mClient->create(item, "com.nokia.shared.1");
        waitForResponse1(id);
        insertedItems << mData;
    }

    QAbstractItemModel *listModel = createModel();
    if (!listModel)
        return;
    connectListModel(listModel);

    listModel->setProperty("limit",10);
    listModel->setProperty("query", QString("[?_type=\"%1\"]").arg(__FUNCTION__));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "order");
    listModel->setProperty("roleNames",roleNames);

    waitForExitOrTimeout();
    QCOMPARE(listModel->property("count").toInt(), 10);

    for (int i = 10; i < 50; i++) {
        QVariantMap item;
        item.insert("_type", __FUNCTION__);
        item.insert("order", i);
        mClient->create(item,  "com.nokia.shared.1");
    }

    waitForItemsCreated(40);
    QCOMPARE(listModel->property("count").toInt(), 50);

    // Change query
    listModel->setProperty("query", QString("[?_type=\"%1\"][\\order]").arg(__FUNCTION__));
    waitForExitOrTimeout();
    QCOMPARE(listModel->property("count").toInt(), 50);

    // Delete the first 10 items
    foreach (QVariant item, insertedItems) {
        mWaitingForRowsRemoved = true;
        id = mClient->remove(item.toMap(), "com.nokia.shared.1");
        while(mWaitingForRowsRemoved)
            mEventLoop.processEvents(QEventLoop::AllEvents);
    }

    QCOMPARE(listModel->property("count").toInt(), 40);

    deleteModel(listModel);
}

void TestJsonDbListModel::listProperty()
{
    QVariant jsonData = readJsonFile("list-objects.json");
    QVariantList itemList = jsonData.toList();
    int id = 0;
    for (int i = 0; i < itemList.count(); i++) {
        id = mClient->create(itemList[i].toMap(), "com.nokia.shared.1");
        waitForResponse1(id);
    }

    QAbstractItemModel *listModel = createModel();
    if (!listModel)
        return;
    connectListModel(listModel);
    QString type = itemList[0].toMap()["_type"].toString();
    listModel->setProperty("limit",10);
    listModel->setProperty("query", QString("[?_type=\"%1\"][/features.0.properties.0.description]").arg(type));
    QStringList roleNames = (QStringList() << "_type" << "_uuid" << "features.0.properties.0.description"<< "features.0.feature");
    listModel->setProperty("roleNames",roleNames);
    waitForExitOrTimeout();

    QCOMPARE(listModel->property("count").toInt(), itemList.count());
    QCOMPARE(get(listModel, 0, "_type").toString(), type);
    QCOMPARE(get(listModel, 0, "features.0.properties.0.description").toString(), QLatin1String("Facebook account provider"));
    QCOMPARE(get(listModel, 0, "features.0.feature").toString(), QLatin1String("provide Facebook"));
    QCOMPARE(get(listModel, 1, "_uuid").toString(), mLastUuid);
    QCOMPARE(get(listModel, 1, "_type").toString(), type);
    QCOMPARE(get(listModel, 1, "features.0.properties.0.description").toString(), QLatin1String("Gmail account provider"));
    QCOMPARE(get(listModel, 1, "features.0.feature").toString(), QLatin1String("provide Gmail"));

    deleteModel(listModel);

    listModel = createModel();
    if (!listModel)
        return;
    connectListModel(listModel);
    type = itemList[0].toMap()["_type"].toString();
    listModel->setProperty("limit",10);
    listModel->setProperty("query", QString("[?_type=\"%1\"][/features.0.properties.0.description]").arg(type));
    roleNames.clear();
    roleNames = (QStringList() << "_type" << "_uuid" << "features[0].properties[0].description"<< "features[0].supported[0]");
    listModel->setProperty("roleNames",roleNames);
    waitForExitOrTimeout();

    QCOMPARE(listModel->property("count").toInt(), itemList.count());
    QCOMPARE(get(listModel, 0, "_type").toString(), type);
    QCOMPARE(get(listModel, 0, "features[0].properties[0].description").toString(), QLatin1String("Facebook account provider"));
    QCOMPARE(get(listModel, 0, "features[0].supported[0]").toString(), QLatin1String("share"));
    QCOMPARE(get(listModel, 1, "_type").toString(), type);
    QCOMPARE(get(listModel, 1, "features[0].properties[0].description").toString(), QLatin1String("Gmail account provider"));
    QCOMPARE(get(listModel, 1, "features[0].supported[0]").toString(), QLatin1String("share"));

    deleteModel(listModel);
}

QStringList TestJsonDbListModel::getOrderValues(QAbstractItemModel *listModel)
{
    QStringList vals;
    for (int i = 0; i < listModel->property("count").toInt(); ++i)
        vals << get(listModel, i, "order").toString();
    return vals;
}


void TestJsonDbListModel::modelReset()
{
    //qDebug() << "TestJsonDbListModel::modelReset";
    mEventLoop.exit(0);
}
void TestJsonDbListModel::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Q_UNUSED(topLeft);
    Q_UNUSED(bottomRight);
    //qDebug() << "TestJsonDbListModel::dataChanged";
    mWaitingForDataChange = false;
}
void TestJsonDbListModel::rowsInserted(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent);
    Q_UNUSED(first);
    Q_UNUSED(last);
    mItemsCreated++;
    //qDebug() << "TestJsonDbListModel::rowsInserted";
    mEventLoop.exit(0);
}
void TestJsonDbListModel::rowsRemoved(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent);
    Q_UNUSED(first);
    Q_UNUSED(last);
    mWaitingForRowsRemoved = false;
    //qDebug() << "TestJsonDbListModel::rowsRemoved";
}
void TestJsonDbListModel::rowsMoved( const QModelIndex &parent, int start, int end, const QModelIndex &destination, int row )
{
    Q_UNUSED(parent);
    Q_UNUSED(start);
    Q_UNUSED(end);
    Q_UNUSED(destination);
    Q_UNUSED(row);
    //qDebug() << "TestJsonDbListModel::rowsMoved";
}

QTEST_MAIN(TestJsonDbListModel)
