#pragma once

#include <QMainWindow>

// Top-level window. Hosts one tab per tool; each tab is a self-contained page
// widget (SimulatorPage, DividerPage, RcFilterPage, MosfetPage).
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
};
