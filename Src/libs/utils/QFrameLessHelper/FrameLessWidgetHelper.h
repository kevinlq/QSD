#ifndef FRAMELESSWIDGETHELPER_H
#define FRAMELESSWIDGETHELPER_H

#include "BasicFrameLessHelper.H"

class QTCREATOR_UTILS_EXPORT FramelessWidgetHelper : public BasicFramelessHelper
{
public:
    explicit FramelessWidgetHelper(QWidget *parent);

    // QObject interface
public:
    virtual bool eventFilter(QObject *watched, QEvent *event) Q_DECL_FINAL;

    // BasicFramelessHelper interface
public:
    virtual bool nativeEventFilter(void *message, long *result) Q_DECL_FINAL;

protected:
    bool isCaption(int x, int y);
private:
    QWidget *m_widget;
};

#endif // FRAMELESSWIDGETHELPER_H
