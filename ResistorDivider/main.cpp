#include "MainWindow.h"

#include <QApplication>

namespace {

// Dark theme for the whole application.
const char *const kStyleSheet = R"(
QWidget {
    color: #e6eaf0;
    font-size: 10.5pt;
}
QMainWindow, QMessageBox, QDialog, QTabWidget, QStackedWidget, DividerPage, RcFilterPage,
SimulatorPage, MosfetPage {
    background-color: #12161c;
}
QTabWidget::pane {
    border: none;
    border-top: 1px solid #2a323e;
}
QTabBar {
    background-color: #12161c;
}
QTabBar::tab {
    background: transparent;
    color: #8a94a3;
    padding: 12px 22px 10px 22px;
    margin-left: 6px;
    border: none;
    border-bottom: 3px solid transparent;
    font-weight: 600;
}
QTabBar::tab:hover {
    color: #e6eaf0;
}
QTabBar::tab:selected {
    color: #4cc2ff;
    border-bottom-color: #4cc2ff;
}
QFrame#panel, QFrame#inputPanel {
    background-color: #1a2029;
    border: 1px solid #2a323e;
    border-radius: 12px;
}
QFrame#inputPanel {
    background-color: #1d242f;
    border-color: #33506a;
}
QFrame#card {
    background-color: #212936;
    border: 1px solid #2f3947;
    border-radius: 10px;
}
QWidget#transparentHost, QScrollArea, QScrollArea > QWidget > QWidget, CircuitWidget,
RcCircuitWidget, FrequencyResponseWidget {
    background: transparent;
}
QLabel {
    background: transparent;
    border: none;
}
QLabel#sectionTitle {
    color: #4cc2ff;
    font-size: 9pt;
    font-weight: 700;
    letter-spacing: 2px;
}
QLabel#fieldLabel, QLabel#cardTitle {
    color: #8a94a3;
    font-size: 9.5pt;
}
QLabel#cardDetail {
    color: #8a94a3;
    font-size: 9pt;
}
QLabel#cardValue {
    font-size: 16pt;
    font-weight: 600;
}
QLabel#cardValueCompact {
    font-size: 13pt;
    font-weight: 600;
}
QLabel#resistorName {
    font-weight: 600;
}
QLabel#unitLabel {
    color: #8a94a3;
}
QLineEdit, QSpinBox, QComboBox {
    background-color: #12161c;
    border: 1px solid #323c4a;
    border-radius: 8px;
    padding: 7px 10px;
    selection-background-color: #2f7fb3;
    selection-color: #ffffff;
}
QLineEdit:hover, QSpinBox:hover, QComboBox:hover {
    border-color: #46556a;
}
QLineEdit:focus, QSpinBox:focus, QComboBox:focus {
    border-color: #4cc2ff;
}
QPushButton {
    border-radius: 8px;
    padding: 9px 16px;
    font-weight: 600;
}
QComboBox::drop-down {
    border: none;
    width: 22px;
}
QComboBox QAbstractItemView {
    background-color: #1a2029;
    border: 1px solid #3a4658;
    selection-background-color: #24425a;
    selection-color: #ffffff;
    outline: none;
}
QPushButton#segmentLeft, QPushButton#segmentRight {
    background-color: #12161c;
    border: 1px solid #323c4a;
    color: #8a94a3;
    padding: 8px 10px;
}
QPushButton#segmentLeft {
    border-top-right-radius: 0;
    border-bottom-right-radius: 0;
}
QPushButton#segmentRight {
    border-top-left-radius: 0;
    border-bottom-left-radius: 0;
    border-left: none;
}
QPushButton#segmentLeft:hover, QPushButton#segmentRight:hover {
    color: #e6eaf0;
}
QPushButton#segmentLeft:checked, QPushButton#segmentRight:checked {
    background-color: #24425a;
    border-color: #4cc2ff;
    color: #ffffff;
}
QPushButton#calculateButton {
    background-color: #2f9fe0;
    border: 1px solid #2f9fe0;
    color: #08121a;
}
QPushButton#calculateButton:hover {
    background-color: #4cc2ff;
    border-color: #4cc2ff;
}
QPushButton#calculateButton:pressed {
    background-color: #2788c2;
}
QPushButton#clearButton, QMessageBox QPushButton {
    background-color: #262f3c;
    border: 1px solid #3a4658;
    color: #e6eaf0;
}
QPushButton#clearButton:hover, QMessageBox QPushButton:hover {
    background-color: #2f3a4a;
}
QPushButton:focus {
    border-color: #9bdcff;
}
QMessageBox QPushButton {
    min-width: 72px;
}
QToolButton#stepButton {
    background-color: #262f3c;
    border: 1px solid #3a4658;
    border-radius: 8px;
    min-width: 34px;
    min-height: 32px;
    font-size: 13pt;
}
QToolButton#stepButton:hover {
    background-color: #2f3a4a;
}
QToolButton#stepButton:pressed {
    background-color: #1f2732;
}
QTableWidget {
    background-color: #161b23;
    alternate-background-color: #1b212b;
    border: 1px solid #2a323e;
    border-radius: 10px;
    selection-background-color: #24425a;
    selection-color: #ffffff;
}
QTableWidget::item {
    padding: 0 8px;
    border: none;
}
QHeaderView::section {
    background-color: #212936;
    color: #8a94a3;
    border: none;
    border-bottom: 1px solid #2a323e;
    padding: 9px 12px;
    font-size: 9.5pt;
    font-weight: 600;
}
QTableCornerButton::section {
    background-color: #212936;
    border: none;
}
QMenuBar {
    background-color: #12161c;
    padding: 2px 6px;
}
QMenuBar::item {
    background: transparent;
    padding: 5px 10px;
    border-radius: 6px;
}
QMenuBar::item:selected, QMenuBar::item:pressed {
    background-color: #262f3c;
}
QMenu {
    background-color: #1a2029;
    border: 1px solid #3a4658;
    padding: 4px;
}
QMenu::item {
    padding: 6px 28px 6px 24px;
    border-radius: 4px;
}
QMenu::item:selected {
    background-color: #24425a;
}
QMenu::item:disabled {
    color: #5b6677;
}
QMenu::separator {
    height: 1px;
    background: #2a323e;
    margin: 4px 6px;
}
QToolBar {
    background-color: #161b23;
    border: none;
    border-top: 1px solid #2a323e;
    border-bottom: 1px solid #2a323e;
    padding: 4px 8px;
    spacing: 4px;
}
QToolBar::separator {
    width: 1px;
    background: #2a323e;
    margin: 4px 6px;
}
QToolBar QToolButton {
    background: transparent;
    border: 1px solid transparent;
    border-radius: 6px;
    padding: 5px 10px;
}
QToolBar QToolButton:hover {
    background-color: #262f3c;
    border-color: #3a4658;
}
QToolBar QToolButton:pressed, QToolBar QToolButton:checked {
    background-color: #24425a;
    border-color: #4cc2ff;
}
QToolBar QToolButton:disabled {
    color: #5b6677;
}
QLabel#toolbarLabel {
    color: #8a94a3;
    font-size: 9.5pt;
    padding: 0 4px 0 8px;
}
QSlider::groove:horizontal {
    height: 4px;
    background: #2a323e;
    border-radius: 2px;
}
QSlider::sub-page:horizontal {
    background: #2f9fe0;
    border-radius: 2px;
}
QSlider::handle:horizontal {
    width: 14px;
    margin: -5px 0;
    border-radius: 7px;
    background: #e6eaf0;
}
QTreeWidget#palette {
    background-color: #1a2029;
    border: 1px solid #2a323e;
    border-radius: 8px;
    padding: 4px;
    outline: none;
}
QTreeWidget#palette::item {
    padding: 3px 2px;
    border-radius: 4px;
}
QTreeWidget#palette::item:hover {
    background-color: #212936;
}
QTreeWidget#palette::item:selected {
    background-color: #24425a;
    color: #ffffff;
}
QCheckBox::indicator {
    width: 16px;
    height: 16px;
    border: 1px solid #46556a;
    border-radius: 4px;
    background: #12161c;
}
QCheckBox::indicator:checked {
    background: #2f9fe0;
    border-color: #4cc2ff;
}
QDialog QPushButton {
    background-color: #262f3c;
    border: 1px solid #3a4658;
    color: #e6eaf0;
    min-width: 72px;
}
QDialog QPushButton:hover {
    background-color: #2f3a4a;
}
QDialog QPushButton:default {
    border-color: #4cc2ff;
}
QSplitter::handle {
    background: transparent;
}
QScrollBar:vertical {
    background: transparent;
    width: 10px;
    margin: 2px;
}
QScrollBar::handle:vertical {
    background: #3a4658;
    border-radius: 3px;
    min-height: 28px;
}
QScrollBar::handle:vertical:hover {
    background: #4b5a70;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0;
}
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
    background: transparent;
}
QToolTip {
    background-color: #212936;
    color: #e6eaf0;
    border: 1px solid #3a4658;
    padding: 4px 6px;
}
)";

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("ElectronicsLab"));
    QApplication::setApplicationName(QStringLiteral("Electronics Lab"));
    QApplication::setStyle(QStringLiteral("Fusion"));
    app.setStyleSheet(QString::fromUtf8(kStyleSheet));

    MainWindow window;
    window.show();

    return app.exec();
}
