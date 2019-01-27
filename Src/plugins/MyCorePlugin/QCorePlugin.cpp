#include "QCorePlugin.h"

#include "id.h"

#include <utils/algorithm.h>

#include "../libs/extensionsystem/pluginmanager.h"
#include "../libs/extensionsystem/pluginspec.h"
#include "../libs/utils/stringutils.h"
#include "../libs/utils/mimetypes/mimedatabase.h"

#include "QSMainWindow.h"

#include <QtPlugin>
#include <QDebug>
#include <QDateTime>
#include <QMenu>
#include <QUuid>

using namespace Core;
using namespace Core::Internal;

QCorePlugin::QCorePlugin()
    :m_mainWindow(Q_NULLPTR)
{
    qRegisterMetaType<Id>();
}

QCorePlugin::~QCorePlugin()
{
    if (m_mainWindow != Q_NULLPTR)
    {
        delete m_mainWindow;
        m_mainWindow = Q_NULLPTR;
    }
}

bool QCorePlugin::initialize(const QStringList &arguments, QString *errorMessage)
{
    Q_UNUSED(arguments);
    Q_UNUSED(errorMessage);
    // register all mime types from all plugins
    for (ExtensionSystem::PluginSpec *plugin : ExtensionSystem::PluginManager::plugins())
    {
        if (!plugin->isEffectivelyEnabled())
            continue;

        const QJsonObject metaData = plugin->metaData();
        const QJsonValue mimetypes = metaData.value("Mimetypes");

        QString mimetypeString;
        if (Utils::readMultiLineString(mimetypes, &mimetypeString))
        {
            Utils::addMimeTypes(plugin->name() + ".mimetypes", mimetypeString.trimmed().toUtf8());
        }
    }

    if (m_mainWindow == Q_NULLPTR)
    {
        m_mainWindow = new QSMainWindow;
    }

    qsrand(QDateTime::currentDateTime().toTime_t());

    const bool success = m_mainWindow->init(errorMessage);
    if (success)
    {
        qDebug()<<"m_mainWindow->init success....";
    }

    return true;
}

void QCorePlugin::extensionsInitialized()
{
    /// 其他初始化判断

    m_mainWindow->extensionsInitialized();

    if ( ExtensionSystem::PluginManager::hasError())
    {
        qWarning()<<"error.....";
    }
}

bool QCorePlugin::delayedInitialize()
{
    /// 其他初始化
    m_mainWindow->show();
    return true;
}

ExtensionSystem::IPlugin::ShutdownFlag QCorePlugin::aboutToShutdown()
{
    //Find::aboutToShutdown();
    m_mainWindow->aboutToShutdown();

    return SynchronousShutdown;
}

QObject *QCorePlugin::remoteCommand(const QStringList &, const QString &workingDirectory, const QStringList &args)
{
    if (!ExtensionSystem::PluginManager::isInitializationDone())
    {
        connect(ExtensionSystem::PluginManager::instance(), &ExtensionSystem::PluginManager::initializationDone,
                this, [this, workingDirectory, args]() {
                    remoteCommand(QStringList(), workingDirectory, args);
        });
        return nullptr;
    }

    m_mainWindow->raiseWindow();

    return nullptr;
}
