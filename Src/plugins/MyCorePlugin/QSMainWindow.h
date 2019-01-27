#ifndef QSMAINWINDOW_H
#define QSMAINWINDOW_H

#include <QMainWindow>

class ICore;
namespace Core{
namespace Internal{

class QSMainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit QSMainWindow(QWidget *parent = 0);
    ~QSMainWindow();

    bool init(QString *errorMessage);
    void extensionsInitialized();
    void aboutToShutdown();

public Q_SLOTS:
    void exit();

    void raiseWindow();

protected:
    virtual void closeEvent(QCloseEvent *event);

private:
};
}//Internal
}//namespace Core

#endif // QSMAINWINDOW_H
