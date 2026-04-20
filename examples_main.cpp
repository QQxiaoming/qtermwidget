/*  Copyright (C) 2008 e_k (e_k@users.sourceforge.net)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/


#include <QApplication>
#include <QtDebug>
#include <QIcon>
#include <QMainWindow>
#include <QMenuBar>
#include <QDir>
#include <QProcessEnvironment>
#include <QProcess>
#include "ptyqt.h"
#include "qtermwidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QMainWindow *mainWindow = new QMainWindow();
    QTermWidget *console = new QTermWidget(mainWindow,mainWindow);
    QMenuBar *menuBar = new QMenuBar(mainWindow);
    QMenu *actionsMenu = new QMenu(QStringLiteral("Actions"), menuBar);
    menuBar->addMenu(actionsMenu);
    actionsMenu->addAction(QStringLiteral("Find..."), console, &QTermWidget::toggleShowSearchBar);
    actionsMenu->addAction(QStringLiteral("Copy"), console, &QTermWidget::copyClipboard);
    actionsMenu->addAction(QStringLiteral("Paste"), console, &QTermWidget::pasteClipboard);
    actionsMenu->addAction(QStringLiteral("About Qt"), &app, &QApplication::aboutQt);
    mainWindow->setMenuBar(menuBar);

    QFont font = QApplication::font();
#ifdef Q_OS_MACOS
    font.setFamily(QStringLiteral("Monaco"));
#elif defined(Q_WS_QWS)
    font.setFamily(QStringLiteral("fixed"));
#else
    font.setFamily(QStringLiteral("Monospace"));
#endif
    font.setPointSize(12);

    console->setTerminalFont(font);
    console->setScrollBarPosition(QTermWidget::ScrollBarRight);
    console->setBlinkingCursor(true);
    console->setMargin(0);
    console->setDrawLineChars(false);
    console->setColorScheme("QuardCRT");

    const auto arguments = QApplication::arguments();
    for (const QString& arg : arguments)
    {
        if (console->availableColorSchemes().contains(arg))
            console->setColorScheme(arg);
        if (console->availableKeyBindings().contains(arg))
            console->setKeyBindings(arg);
    }

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    QString shellPath = qEnvironmentVariable("SHELL");
#elif defined(Q_OS_WIN)
    QString shellPath = "c:\\Windows\\system32\\WindowsPowerShell\\v1.0\\powershell.exe";
#endif
    IPtyProcess *localShell = PtyQt::createPtyProcess();
    QObject::connect(console, &QTermWidget::termSizeChange, [=](int lines, int columns){
        localShell->resize(columns,lines);
    });
    QStringList args;
    localShell->startProcess(shellPath, args, QDir::homePath(), QProcessEnvironment::systemEnvironment().toStringList(),  console->screenColumnsCount(), console->screenLinesCount());
    QObject::connect(localShell->notifier(), &QIODevice::readyRead, [=](){
        QByteArray data = localShell->readAll();
        if (!data.isEmpty()) {
            console->recvData(data.data(), data.size());
        }
    });
    QObject::connect(localShell->notifier(), &QIODevice::aboutToClose, [=](){
        if (localShell) {
            QByteArray restOfOutput = localShell->readAll();
            if (!restOfOutput.isEmpty()) {
                console->recvData(restOfOutput.data(), restOfOutput.size());
                localShell->notifier()->disconnect();
            }
        }
    });
    QObject::connect(console, &QTermWidget::sendData, [=](const char *data, int size){
        localShell->write(QByteArray(data, size));
    });

    mainWindow->setCentralWidget(console);
    mainWindow->resize(600, 400);

    QObject::connect(console, &QTermWidget::finished, mainWindow, &QMainWindow::close);

    mainWindow->show();
    return app.exec();
}
