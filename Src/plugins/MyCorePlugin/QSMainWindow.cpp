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

#include <QEvent>
#include <QCloseEvent>
#include <QTimer>
#include <QDebug>

using namespace Core;
using namespace Core::Internal;

QSMainWindow::QSMainWindow(QWidget *parent) : QMainWindow(parent)
{

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

}

void QSMainWindow::aboutToShutdown()
{

}

void QSMainWindow::exit()
{
    QTimer::singleShot(0, this,&QWidget::close);
}

void QSMainWindow::raiseWindow()
{
    setWindowState(windowState() & ~Qt::WindowMinimized);

    raise();

    activateWindow();
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
