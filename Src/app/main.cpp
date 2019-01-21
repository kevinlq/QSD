#include <QApplication>

#include <qtsingleapplication.h>

int main(int argc, char *argv[])
{
    SharedTools::QtSingleApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    SharedTools::QtSingleApplication app((QLatin1String(/*Core::Constants::IDE_DISPLAY_NAME*/)), argc, argv);

    return app.exec();
}
