#ifndef QSMAINWINDOW_H
#define QSMAINWINDOW_H

#include "utils/appmainwindow.h"

class ICore;

namespace CorePlugin{

class QMenuManager;

namespace Internal{

class QSMainWindow : public Utils::AppMainWindow
{
    Q_OBJECT
public:
    explicit QSMainWindow(QWidget *parent = 0);
    ~QSMainWindow();
    QMenuManager *menuManager();

    bool init(QString *errorMessage);
    void extensionsInitialized();
    void aboutToShutdown();

public Q_SLOTS:
    void exit();

protected:
    virtual void closeEvent(QCloseEvent *event);

private:
    void restoreWindowState();

private:
    void registerDefaultContainers();

    void registerDefaultActions();

private:
    QMenuManager *m_pMenuManager;
};
}//Internal
}//namespace Core

#endif // QSMAINWINDOW_H
