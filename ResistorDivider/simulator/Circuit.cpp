#include "Circuit.h"

#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>

namespace {

// Tiny conductance from every node to ground. It keeps the matrix solvable
// while parts of the circuit are still unconnected during editing.
const double kNodeLeakage = 1.0e-12;
const int kMaxIterations = 400;

} // namespace

// ---------------------------------------------------------------------------
// Editing
// ---------------------------------------------------------------------------

Element *Circuit::add(std::unique_ptr<Element> element)
{
    if (element->id <= 0 || find(element->id))
        element->id = m_nextId;
    m_nextId = std::max(m_nextId, element->id + 1);
    elements.push_back(std::move(element));
    m_dirty = true;
    return elements.back().get();
}

Element *Circuit::find(int id) const
{
    for (const auto &element : elements) {
        if (element->id == id)
            return element.get();
    }
    return nullptr;
}

int Circuit::indexOf(const Element *element) const
{
    for (std::size_t i = 0; i < elements.size(); ++i) {
        if (elements[i].get() == element)
            return static_cast<int>(i);
    }
    return -1;
}

void Circuit::clear()
{
    elements.clear();
    m_kclPlan.clear();
    m_dirty = true;
}

QString Circuit::serialize() const
{
    QString text = QStringLiteral("$ %1\n").arg(timeStep, 0, 'g', 12);
    for (const auto &element : elements)
        text += element->toLine() + QLatin1Char('\n');
    return text;
}

void Circuit::deserialize(const QString &text)
{
    clear();
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QLatin1Char('$'))) {
            bool ok = false;
            const double step = trimmed.mid(1).trimmed().toDouble(&ok);
            if (ok && step > 0.0 && std::isfinite(step))
                timeStep = step;
            continue;
        }
        if (std::unique_ptr<Element> element = Element::fromLine(trimmed))
            add(std::move(element));
    }
}

// ---------------------------------------------------------------------------
// Stamping
// ---------------------------------------------------------------------------

void Circuit::stampMatrix(int row, int column, double value)
{
    if (row > 0 && column > 0)
        m_matrix[static_cast<std::size_t>(row - 1) * m_size + (column - 1)] += value;
}

void Circuit::stampRightSide(int row, double value)
{
    if (row > 0)
        m_rightSide[row - 1] += value;
}

void Circuit::stampConductance(int n1, int n2, double siemens)
{
    stampMatrix(n1, n1, siemens);
    stampMatrix(n2, n2, siemens);
    stampMatrix(n1, n2, -siemens);
    stampMatrix(n2, n1, -siemens);
}

void Circuit::stampResistor(int n1, int n2, double ohms)
{
    stampConductance(n1, n2, 1.0 / ohms);
}

void Circuit::stampCurrentSource(int n1, int n2, double amps)
{
    stampRightSide(n1, -amps);
    stampRightSide(n2, amps);
}

void Circuit::stampVoltageSource(int n1, int n2, int voltSource)
{
    const int row = sourceRow(voltSource);
    stampMatrix(row, n1, -1.0);
    stampMatrix(row, n2, 1.0);
    stampMatrix(n1, row, 1.0);
    stampMatrix(n2, row, -1.0);
}

void Circuit::setVoltageSourceValue(int voltSource, double volts)
{
    stampRightSide(sourceRow(voltSource), volts);
}

// ---------------------------------------------------------------------------
// Analysis
// ---------------------------------------------------------------------------

bool Circuit::analyze()
{
    m_dirty = false;
    error.clear();
    m_kclPlan.clear();
    m_size = 0;
    m_nodeCount = 1;
    if (elements.empty())
        return true;

    // 1. Every distinct post position is a point; wires merge their two points.
    std::map<std::pair<int, int>, int> pointIndex;
    std::vector<int> parent;
    auto indexOfPoint = [&](const QPoint &p) {
        const auto key = std::make_pair(p.x(), p.y());
        auto it = pointIndex.find(key);
        if (it != pointIndex.end())
            return it->second;
        const int index = static_cast<int>(parent.size());
        parent.push_back(index);
        pointIndex.emplace(key, index);
        return index;
    };
    auto root = [&](int i) {
        while (parent[i] != i) {
            parent[i] = parent[parent[i]];
            i = parent[i];
        }
        return i;
    };
    auto unite = [&](int a, int b) { parent[root(a)] = root(b); };

    for (const auto &element : elements) {
        for (int i = 0; i < element->postCount(); ++i)
            indexOfPoint(element->post(i));
        if (element->isWire())
            unite(indexOfPoint(element->post(0)), indexOfPoint(element->post(1)));
    }

    // 2. Pick the ground node. Without a ground symbol (and nothing else that
    //    refers to ground) the negative side of the first source is used.
    int groundPoint = -1;
    bool groundReferenced = false;
    for (const auto &element : elements) {
        if (element->isGround()) {
            const int point = indexOfPoint(element->post(0));
            if (groundPoint < 0)
                groundPoint = point;
            else
                unite(point, groundPoint);
        }
        groundReferenced = groundReferenced || element->usesGroundNode();
    }
    if (groundPoint < 0 && !groundReferenced) {
        const Element *reference = elements.front().get();
        for (const auto &element : elements) {
            if (element->voltageSourceCount() > 0) {
                reference = element.get();
                break;
            }
        }
        groundPoint = indexOfPoint(reference->post(0));
    }

    // 3. Number the nodes.
    std::vector<int> nodeOfRoot(parent.size(), -1);
    if (groundPoint >= 0)
        nodeOfRoot[root(groundPoint)] = 0;
    for (const auto &element : elements) {
        for (int i = 0; i < element->postCount(); ++i) {
            const int r = root(indexOfPoint(element->post(i)));
            if (nodeOfRoot[r] < 0)
                nodeOfRoot[r] = m_nodeCount++;
            element->nodes[i] = nodeOfRoot[r];
        }
    }

    // 4. Number the voltage sources and build the constant part of the system.
    int sourceCount = 0;
    m_nonLinear = false;
    for (const auto &element : elements) {
        element->voltSource = sourceCount;
        sourceCount += element->voltageSourceCount();
        m_nonLinear = m_nonLinear || element->nonLinear();
    }

    m_size = m_nodeCount - 1 + sourceCount;
    m_matrix.assign(static_cast<std::size_t>(m_size) * m_size, 0.0);
    m_rightSide.assign(m_size, 0.0);
    for (const auto &element : elements)
        element->stamp(*this);
    for (int node = 1; node < m_nodeCount; ++node)
        stampMatrix(node, node, kNodeLeakage);

    m_origMatrix = m_matrix;
    m_origRightSide = m_rightSide;

    // A linear circuit has a constant matrix: factor it once.
    if (!m_nonLinear && m_size > 0) {
        m_lu = m_matrix;
        if (!luFactor()) {
            error = QStringLiteral("Cannot solve this circuit (voltage sources or closed "
                                   "switches connected in a loop?)");
        }
    }

    buildKclPlan();
    return error.isEmpty();
}

// Works out an order in which the currents of wires and ground symbols can be
// recovered: at a junction where everything else is known, the remaining
// element carries the balance.
void Circuit::buildKclPlan()
{
    std::map<std::pair<int, int>, std::vector<std::pair<Element *, int>>> junctions;
    std::vector<Element *> pending;
    for (const auto &element : elements) {
        for (int i = 0; i < element->postCount(); ++i) {
            const QPoint p = element->post(i);
            junctions[{p.x(), p.y()}].emplace_back(element.get(), i);
        }
        if (element->currentFromKcl())
            pending.push_back(element.get());
    }

    auto isPending = [&](const Element *element) {
        return std::find(pending.begin(), pending.end(), element) != pending.end();
    };

    while (!pending.empty()) {
        bool progressed = false;
        for (std::size_t n = 0; n < pending.size() && !progressed; ++n) {
            Element *element = pending[n];
            for (int i = 0; i < element->postCount() && !progressed; ++i) {
                const QPoint p = element->post(i);
                KclStep step;
                step.element = element;
                step.post = i;
                bool solvable = true;
                for (const auto &attached : junctions[{p.x(), p.y()}]) {
                    if (attached.first == element)
                        continue;
                    if (isPending(attached.first)) {
                        solvable = false;
                        break;
                    }
                    step.others.push_back(attached);
                }
                if (solvable) {
                    m_kclPlan.push_back(std::move(step));
                    pending.erase(pending.begin() + static_cast<std::ptrdiff_t>(n));
                    progressed = true;
                }
            }
        }
        if (!progressed) {
            // Wires forming a loop: the split of current between them is not
            // defined, so one of them is shown without current.
            auto it = std::find_if(pending.begin(), pending.end(),
                                   [](const Element *e) { return e->isWire(); });
            if (it == pending.end())
                it = pending.begin();
            KclStep step;
            step.element = *it;
            m_kclPlan.push_back(std::move(step));
            pending.erase(it);
        }
    }
}

void Circuit::updateWireCurrents()
{
    for (const KclStep &step : m_kclPlan) {
        double sum = 0.0;
        for (const auto &other : step.others)
            sum += other.first->postCurrent(other.second);
        step.element->setPostCurrent(step.post, -sum);
    }
}

// ---------------------------------------------------------------------------
// Time stepping
// ---------------------------------------------------------------------------

bool Circuit::ensureAnalyzed()
{
    if (m_dirty)
        analyze();
    return error.isEmpty();
}

void Circuit::resetState()
{
    time = 0.0;
    for (const auto &element : elements)
        element->reset();
    m_dirty = true;
}

bool Circuit::step()
{
    if (!ensureAnalyzed())
        return false;
    if (m_size == 0) {
        time += timeStep;
        return true;
    }

    for (const auto &element : elements)
        element->startIteration(*this);

    bool solved = false;
    const int limit = m_nonLinear ? kMaxIterations : 1;
    for (subIteration = 0; subIteration < limit; ++subIteration) {
        converged = true;
        m_rightSide = m_origRightSide;
        if (m_nonLinear)
            m_matrix = m_origMatrix;
        for (const auto &element : elements)
            element->doStep(*this);

        if (m_nonLinear) {
            // Converged when no element had to change its operating point.
            if (converged && subIteration > 0) {
                solved = true;
                break;
            }
            m_lu = m_matrix;
            if (!luFactor()) {
                error = QStringLiteral("Cannot solve this circuit (singular matrix)");
                return false;
            }
        }

        m_solution = m_rightSide;
        luSolve(m_solution);
        for (double value : m_solution) {
            if (!std::isfinite(value)) {
                error = QStringLiteral("Simulation diverged (values became infinite)");
                return false;
            }
        }
        applySolution(m_solution);

        if (!m_nonLinear) {
            solved = true;
            break;
        }
    }

    if (!solved) {
        error = QStringLiteral("Convergence failed. Press Reset or change the circuit.");
        return false;
    }

    for (const auto &element : elements)
        element->stepFinished(*this);
    time += timeStep;
    return true;
}

void Circuit::applySolution(const std::vector<double> &x)
{
    for (const auto &element : elements) {
        for (int i = 0; i < element->postCount(); ++i) {
            const int node = element->nodes[i];
            element->volts[i] = node > 0 ? x[node - 1] : 0.0;
        }
        for (int k = 0; k < element->voltageSourceCount(); ++k)
            element->setSourceCurrent(k, x[m_nodeCount - 1 + element->voltSource + k]);
    }
}

// ---------------------------------------------------------------------------
// Dense LU decomposition with partial pivoting
// ---------------------------------------------------------------------------

bool Circuit::luFactor()
{
    const int n = m_size;
    m_permutation.resize(n);
    for (int j = 0; j < n; ++j) {
        int pivot = j;
        double largest = std::fabs(m_lu[static_cast<std::size_t>(j) * n + j]);
        for (int i = j + 1; i < n; ++i) {
            const double candidate = std::fabs(m_lu[static_cast<std::size_t>(i) * n + j]);
            if (candidate > largest) {
                largest = candidate;
                pivot = i;
            }
        }
        if (!(largest > 1.0e-20))
            return false;

        m_permutation[j] = pivot;
        if (pivot != j) {
            std::swap_ranges(m_lu.begin() + static_cast<std::ptrdiff_t>(j) * n,
                             m_lu.begin() + static_cast<std::ptrdiff_t>(j + 1) * n,
                             m_lu.begin() + static_cast<std::ptrdiff_t>(pivot) * n);
        }

        const double diagonal = m_lu[static_cast<std::size_t>(j) * n + j];
        for (int i = j + 1; i < n; ++i) {
            double *row = &m_lu[static_cast<std::size_t>(i) * n];
            const double factor = row[j] / diagonal;
            row[j] = factor;
            if (factor == 0.0)
                continue;
            const double *pivotRow = &m_lu[static_cast<std::size_t>(j) * n];
            for (int k = j + 1; k < n; ++k)
                row[k] -= factor * pivotRow[k];
        }
    }
    return true;
}

void Circuit::luSolve(std::vector<double> &b) const
{
    const int n = m_size;
    for (int j = 0; j < n; ++j)
        std::swap(b[j], b[m_permutation[j]]);
    for (int i = 0; i < n; ++i) {
        const double *row = &m_lu[static_cast<std::size_t>(i) * n];
        double sum = b[i];
        for (int k = 0; k < i; ++k)
            sum -= row[k] * b[k];
        b[i] = sum;
    }
    for (int i = n - 1; i >= 0; --i) {
        const double *row = &m_lu[static_cast<std::size_t>(i) * n];
        double sum = b[i];
        for (int k = i + 1; k < n; ++k)
            sum -= row[k] * b[k];
        b[i] = sum / row[i];
    }
}
