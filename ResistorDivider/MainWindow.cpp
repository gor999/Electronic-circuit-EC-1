#include "MainWindow.h"

#include "DividerPage.h"
#include "RcFilterPage.h"
#include "mosfet/ui/MosfetPage.h"
#include "simulator/SimulatorPage.h"

#include <QGuiApplication>
#include <QScreen>
#include <QTabWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("Electronics Lab"));

    auto *tabs = new QTabWidget;
    tabs->setObjectName(QStringLiteral("modeTabs"));
    tabs->setDocumentMode(true);
    tabs->addTab(new SimulatorPage, tr("Circuit Simulator"));
    tabs->addTab(new DividerPage, tr("Resistor Divider"));
    tabs->addTab(new RcFilterPage, tr("RC Filter"));
    tabs->addTab(new MosfetPage, tr("MOSFET 3D"));
    setCentralWidget(tabs);

    // Large enough for the MOSFET lab's three columns, but never bigger than the screen.
    QSize size(1480, 900);
    if (const QScreen *screen = QGuiApplication::primaryScreen())
        size = size.boundedTo(screen->availableGeometry().size() * 0.95);
    resize(size);
    setMinimumHeight(640);
}
