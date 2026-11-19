#pragma once

#include "Element.h"

#include <QString>

#include <memory>
#include <utility>
#include <vector>

// A circuit: the list of elements plus the simulation engine.
//
// The engine uses modified nodal analysis (MNA). analyze() turns the element
// posts into nodes and stamps the constant part of the matrix; step() advances
// the simulation by one time step, iterating (Newton-Raphson) when nonlinear
// elements are present. Elements write into the matrix through the stamp*
// functions. Node 0 is ground; voltage sources get extra rows numbered from
// nodeCount() upwards.
class Circuit
{
public:
    std::vector<std::unique_ptr<Element>> elements;

    double time = 0.0;
    double timeStep = 5.0e-6;
    QString error;                  // non-empty when the circuit cannot be simulated

    // ---- Editing ----------------------------------------------------------
    Element *add(std::unique_ptr<Element> element);     // assigns an id when it has none
    Element *find(int id) const;
    int indexOf(const Element *element) const;
    void clear();
    void markDirty() { m_dirty = true; }                // call after any change

    QString serialize() const;
    void deserialize(const QString &text);

    // ---- Simulation -------------------------------------------------------
    bool analyze();
    bool ensureAnalyzed();          // analyses only if something changed
    bool step();
    void resetState();              // time = 0, clears capacitor/inductor state
    void updateWireCurrents();      // call before drawing

    // ---- Stamping API for elements ---------------------------------------
    int nodeCount() const { return m_nodeCount; }
    int sourceRow(int voltSource) const { return m_nodeCount + voltSource; }
    void stampMatrix(int row, int column, double value);
    void stampRightSide(int row, double value);
    void stampResistor(int n1, int n2, double ohms);
    void stampConductance(int n1, int n2, double siemens);
    void stampCurrentSource(int n1, int n2, double amps);      // flows n1 -> n2
    void stampVoltageSource(int n1, int n2, int voltSource);   // v(n2) - v(n1) = value
    void setVoltageSourceValue(int voltSource, double volts);

    bool converged = true;          // cleared by elements during doStep()
    int subIteration = 0;

private:
    struct KclStep
    {
        Element *element = nullptr;
        int post = 0;
        std::vector<std::pair<Element *, int>> others;
    };

    bool luFactor();
    void luSolve(std::vector<double> &b) const;
    void applySolution(const std::vector<double> &x);
    void buildKclPlan();

    bool m_dirty = true;
    int m_nodeCount = 1;
    int m_size = 0;
    bool m_nonLinear = false;
    int m_nextId = 1;

    std::vector<double> m_matrix;
    std::vector<double> m_rightSide;
    std::vector<double> m_origMatrix;
    std::vector<double> m_origRightSide;
    std::vector<double> m_lu;
    std::vector<int> m_permutation;
    std::vector<double> m_solution;
    std::vector<KclStep> m_kclPlan;
};
