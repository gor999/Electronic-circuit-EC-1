#pragma once

#include <QDialog>
#include <QList>

class Element;
class QWidget;

// Property editor for one element, generated from Element::params().
// Number fields accept SI suffixes: 4.7k, 100n, 2.2u, 1meg, 10m ...
// "Apply" writes the values without closing, so a running simulation can be
// tuned while watching it.
class ElementDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ElementDialog(Element *element, QWidget *parent = nullptr);

    // Parses "4.7k", "100 nF", "1e-6" ... Returns false if the text is not a number.
    static bool parseValue(const QString &text, double &value);

signals:
    void aboutToApply();    // emitted once, before the first change (for undo)
    void applied();

private slots:
    void apply();

private:
    bool applyValues();

    Element *m_element;
    QList<QWidget *> m_editors;
    bool m_changed = false;
};
