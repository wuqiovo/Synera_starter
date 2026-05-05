#include <QApplication>
#include "gui/gamewindow.h"
#include "gui/startwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("Synera Starter");
    app.setApplicationVersion("1.0");

    // 启动初始窗口。
    StartWindow startWindow;
    startWindow.resize(500, 320);

    QObject::connect(&startWindow, &StartWindow::startNewGame, [&startWindow]() {
        // 进入新游戏。
        auto* window = new GameWindow();
        window->setWindowTitle("Synera - Starter");
        window->resize(900, 700);
        window->show();
        startWindow.close();
    });

    QObject::connect(&startWindow, &StartWindow::loadGameRequested, [&startWindow](const QString& path) {
        // 进入读档流程。
        auto* window = new GameWindow();
        window->setWindowTitle("Synera - Starter");
        window->resize(900, 700);
        if (!window->loadFromFile(path)) {
            delete window;
            return;
        }
        window->show();
        startWindow.close();
    });

    startWindow.show();

    return app.exec();
}