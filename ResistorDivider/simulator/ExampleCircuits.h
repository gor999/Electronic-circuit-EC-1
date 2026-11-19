#pragma once

#include <QList>
#include <QString>

// Built-in sample circuits (the "Circuits" menu), stored in the same text
// format that File > Save writes:
//
//   $ <time step>
//   <code> <id> <x1> <y1> <x2> <y2> <flip> <parameters...>     one line per element
//   o <element index>                                          one line per scope
struct ExampleCircuit
{
    QString title;
    QString text;
};

const QList<ExampleCircuit> &exampleCircuits();
