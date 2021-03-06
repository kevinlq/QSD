#include "QSMainWindow.h"

#include "../libs/extensionsystem/pluginmanager.h"
#include <utils/algorithm.h>
#include <utils/historycompleter.h>
#include <utils/hostosinfo.h>
#include <utils/mimetypes/mimedatabase.h>
#include <utils/qtcassert.h>
#include <utils/stylehelper.h>
#include <utils/theme/theme.h>
#include <utils/stringutils.h>
#include <utils/utilsicons.h>

#include <utils/baseDialog/BaseDialog.h>

#include <../app/app_version.h>
#include "id.h"
//#include "icore.h"


#include "coreconstants.h"
#include "QMenuManager.h"

#include <QEvent>
#include <QCloseEvent>
#include <QTimer>
#include <QDebug>
#include <QSettings>
#include <QMenuBar>

using namespace CorePlugin;
using namespace CorePlugin::Internal;

using namespace ExtensionSystem;
using namespace Utils;

QSMainWindow::QSMainWindow(QWidget *parent)
    : AppMainWindow(parent)
    ,m_pMenuManager(new QMenuManager(this))
    ,m_pAboutMeDialog(Q_NULLPTR)
{

    registerDefaultContainers();

    registerDefaultActions();
}

QSMainWindow::~QSMainWindow()
{
    if (m_pMenuManager != Q_NULLPTR)
    {
        delete m_pMenuManager;
        m_pMenuManager = Q_NULLPTR;
    }

    qDebug()<<"QSMainWindow destrouct";
}

QMenuManager *QSMainWindow::menuManager()
{
    return m_pMenuManager;
}

bool QSMainWindow::init(QString *errorMessage)
{
    Q_UNUSED(errorMessage);

    return true;
}

void QSMainWindow::extensionsInitialized()
{
    // Delay restoreWindowState, since it is overridden by LayoutRequest event
    QTimer::singleShot(0, this, &QSMainWindow::restoreWindowState);

    //QTimer::singleShot(0, m_coreImpl, &ICore::coreOpened);
}

void QSMainWindow::aboutToShutdown()
{

}

static const char settingsGroup[] = "QSMainWindow";
static const char colorKey[] = "Color";
static const char windowGeometryKey[] = "WindowGeometry";
static const char windowStateKey[] = "WindowState";
static const char modeSelectorVisibleKey[] = "ModeSelectorVisible";

void QSMainWindow::exit()
{
    QTimer::singleShot(0, this,&QWidget::close);
}

void QSMainWindow::closeEvent(QCloseEvent *event)
{
    // work around QTBUG-43344
    static bool alreadyClosed = false;
    if (alreadyClosed)
    {
        event->accept();
        return;
    }

    event->accept();

    alreadyClosed = false;
}

void QSMainWindow::restoreWindowState()
{
    QSettings *settings = PluginManager::settings();
    settings->beginGroup(QLatin1String(settingsGroup));

    if (!restoreGeometry(settings->value(QLatin1String(windowGeometryKey)).toByteArray()))
    {
        resize(1260, 700); // size without window decoration
    }

    restoreState(settings->value(QLatin1String(windowStateKey)).toByteArray());
    settings->endGroup();

    show();

    //m_statusBarManager->restoreSettings();
}

void QSMainWindow::registerDefaultContainers()
{
    /** 注册默认Menu*/
    m_pMenuManager->addMenu(Constants::HELP_MENU_NAME, Constants::HELP_MENU_PRIORITY);
}

void QSMainWindow::registerDefaultActions()
{
    /** 注册默认 Action*/

    QAction *pAboutMeAction = new QAction(tr("About Me"), this);
    m_pMenuManager->addAction(Constants::HELP_MENU_NAME, pAboutMeAction);

    connect (pAboutMeAction, &QAction::triggered, [this](){
        if (m_pAboutMeDialog == Q_NULLPTR)
        {
           m_pAboutMeDialog = new BaseDialog;
        }

        m_pAboutMeDialog->show ();
    });

    QAction *pAboutPluginsAction = new QAction(tr("About Plugins"), this);
    m_pMenuManager->addAction(Constants::HELP_MENU_NAME, pAboutPluginsAction);
}
