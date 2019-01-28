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

#include <../app/app_version.h>
#include "id.h"
//#include "icore.h"


#include "coreconstants.h"

#include <QEvent>
#include <QCloseEvent>
#include <QTimer>
#include <QDebug>
#include <QSettings>
#include <QMenuBar>

using namespace Core;
using namespace Core::Internal;

using namespace ExtensionSystem;
using namespace Utils;

QSMainWindow::QSMainWindow()
    : AppMainWindow()
{
    registerDefaultContainers();

    registerDefaultActions();
}

QSMainWindow::~QSMainWindow()
{
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
//    QMenuBar *pMenuBar = new QMenuBar(this);
//    this->setMenuBar(pMenuBar);

//    QMenu *pFileMenu = new QMenu("file", pMenuBar);
//    pMenuBar->addMenu(pFileMenu);
}

void QSMainWindow::registerDefaultActions()
{
    //
}
