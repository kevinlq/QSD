#ifndef BASEDIALOG_H
#define BASEDIALOG_H

#include <QWidget>

class QPushButton;

class QTCREATOR_UTILS_EXPORT BaseDialog : public QWidget
{
    Q_OBJECT
public:
    explicit BaseDialog(QWidget *parent = nullptr);
    virtual ~BaseDialog();

private:
    QPushButton *m_pBtnClose;
};

#endif // BASEDIALOG_H
