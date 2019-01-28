#ifndef QCOREPLUGIN_H
#define QCOREPLUGIN_H

#include "extensionsystem/iplugin.h"

namespace Core{

namespace Internal{

class QSMainWindow;

class QCorePlugin : public ExtensionSystem::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "Core.json")
public:
    explicit QCorePlugin();
    ~QCorePlugin();

    /// 初始化函数，在插件被加载时会被调用
    bool initialize(const QStringList &arguments, QString *errorMessage = 0);

    /// 在所有插件的 initialize 函数执行后，调用该函数，此时该插件依赖的插件已经初始化完成
    void extensionsInitialized();

    /// 延迟初始化--在所有插件的 extensionsInitialized 函数调用完成后调用此函数
    bool delayedInitialize();

    ShutdownFlag aboutToShutdown();

    QObject *remoteCommand(const QStringList & /* options */,
                           const QString &workingDirectory,
                           const QStringList &args);

private:
    QSMainWindow *m_mainWindow;
};
}//namespace Internal
}// namespace QCore

#endif // QCOREPLUGIN_H
