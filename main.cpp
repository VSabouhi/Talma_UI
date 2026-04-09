#include "mainwindow.h"
#include <QApplication>
#include <QPalette>
#include <QColor>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // اول استایل رو Fusion کن
    a.setStyle("Fusion");
    // بعد کل برنامه رو با stylesheet مجبور کن دارک شه
    a.setStyleSheet(R"(
        QMainWindow {
            background-color: #121212;
        }

        QWidget {
            background-color: #121212;
            color: #E0E0E0;
        }

        QPushButton {
            background-color: #1E1E1E;
            border: 1px solid #333;
            padding: 6px;
            border-radius: 4px;
        }

        QPushButton:hover {
            background-color: #2A2A2A;
        }

        QTabWidget::pane {
            border: 1px solid #2C2C2C;
        }

        QTabBar::tab {
            background: #1E1E1E;
            padding: 6px;
            border: 1px solid #2C2C2C;
        }

        QTabBar::tab:selected {
            background: #007ACC;
        }
    )");

    MainWindow w;
    w.setWindowTitle("DARK TEST");
    w.resize(900, 600);
    w.show();

    return a.exec();
}
