#ifndef QMENUMANAGER_H
#define QMENUMANAGER_H

#include <QObject>

class QMenuBar;
class QMenu;
class QAction;

namespace CorePlugin{

class QMenuManager : public QObject
{
    Q_OBJECT
public:
    explicit QMenuManager(QObject *parent = 0);
    ~QMenuManager();

    void addMenu(const QString &catalog, int priority);
    void addAction(const QString &catalog, QAction *action);
    void addOperator(const QString &catalog);
    QMenuBar *createMenuBar();
    QMenu *createMenu();

signals:

public slots:

private:
    QMap<QString, int> m_priorityMap;
    QList<QString> m_catalogs;
    QMap<QString, QList<QAction *> > m_actionListMap;
};
}   // namespace CorePlugin

#endif // QMENUMANAGER_H
