#include "StdMain.h"

#include "NativeEventFilter.h"

#include <QCoreApplication>
#include <QScreen>
#include <QHash>

#ifdef Q_OS_WIN
#include <windows.h>
#include <QtWin>
#endif

#include "basicFramelessHelper.h"


void NativeEventFilter::deliver(QWindow *window, BasicFramelessHelper *helper)
{
    if (instance == Q_NULLPTR) {
        // instance  will be delete by application
        instance = new NativeEventFilter;
        if (instance) qApp->installNativeEventFilter(instance);
    }
    if (window && helper) {
        auto wid = window->winId();
        if (!helpers.contains(wid)) {
            auto hwnd = reinterpret_cast<HWND>(wid);
            // set new window style
            DWORD oldStyle = GetWindowLong(hwnd, GWL_STYLE);
            SetWindowLong(hwnd, GWL_STYLE, oldStyle | WS_CAPTION
                          | WS_MINIMIZEBOX |  WS_MAXIMIZEBOX | WS_THICKFRAME);
            helpers.insert(wid, helper);
            // 添加阴影效果并且使得缩放时有透明的效果
            if (QtWin::isCompositionEnabled())
                QtWin::extendFrameIntoClientArea(window, 1, 1, 1, 1);
        }
    }

    if (window && helper == Q_NULLPTR) {
        helpers.remove(window->winId());
    }
}

bool NativeEventFilter::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
{
    Q_UNUSED(eventType)
    auto lpMsg = (LPMSG)message;

    auto wid = reinterpret_cast<WId>(lpMsg->hwnd);

    if (auto helper = helpers.value(wid)) {
        return helper->nativeEventFilter(message, result);
    }

    return false;
}

NativeEventFilter *NativeEventFilter::instance = Q_NULLPTR;
QHash<WId, BasicFramelessHelper*> NativeEventFilter::helpers;
