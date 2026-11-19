#include "Element.h"

#include "Elements.h"
#include "Format.h"

#include <QRegularExpression>

#include <cmath>

QPoint Element::snapToGrid(const QPointF &point)
{
    return QPoint(static_cast<int>(std::lround(point.x() / kGridSize)) * kGridSize,
                  static_cast<int>(std::lround(point.y() / kGridSize)) * kGridSize);
}

QPoint Element::sidePoint(const QPoint &base, double side) const
{
    const QPointF delta = p2 - p1;
    const double length = std::hypot(delta.x(), delta.y());
    if (length < 1e-9)
        return base;
    QPointF normal(-delta.y() / length, delta.x() / length);
    if (flip)
        normal = -normal;
    return snapToGrid(QPointF(base) + normal * side);
}

void Element::reset()
{
    for (int i = 0; i < 3; ++i) {
        volts[i] = 0.0;
        dotPhase[i] = 0.0;
    }
    current = 0.0;
}

QString Element::volt(double v) { return Format::si(Format::tidy(v, 1e-6), QStringLiteral("V")); }
QString Element::amp(double a) { return Format::si(Format::tidy(a, 1e-12), QStringLiteral("A")); }
QString Element::watt(double w) { return Format::si(Format::tidy(w, 1e-15), QStringLiteral("W")); }

QStringList Element::info() const
{
    QStringList lines{name()};
    const QString value = valueText();
    if (!value.isEmpty())
        lines << value;
    lines << QStringLiteral("Vd = ") + volt(voltageDiff())
          << QStringLiteral("I = ") + amp(current)
          << QStringLiteral("P = ") + watt(power());
    return lines;
}

QString Element::toLine() const
{
    QStringList parts{code(), QString::number(id),
                      QString::number(p1.x()), QString::number(p1.y()),
                      QString::number(p2.x()), QString::number(p2.y()),
                      QString::number(flip ? 1 : 0)};
    const QList<ParamInfo> list = params();
    for (const ParamInfo &param : list)
        parts << QString::number(param.value, 'g', 12);
    return parts.join(QLatin1Char(' '));
}

std::unique_ptr<Element> Element::fromLine(const QString &line)
{
    static const QRegularExpression whitespace(QStringLiteral("\\s+"));
    const QStringList parts = line.trimmed().split(whitespace, Qt::SkipEmptyParts);
    if (parts.size() < 7)
        return nullptr;

    std::unique_ptr<Element> element = create(parts[0]);
    if (!element)
        return nullptr;

    bool ok[6];
    element->id = parts[1].toInt(&ok[0]);
    element->p1 = QPoint(parts[2].toInt(&ok[1]), parts[3].toInt(&ok[2]));
    element->p2 = QPoint(parts[4].toInt(&ok[3]), parts[5].toInt(&ok[4]));
    element->flip = parts[6].toInt(&ok[5]) != 0;
    for (bool good : ok) {
        if (!good)
            return nullptr;
    }
    if (element->p1 == element->p2)
        return nullptr;

    const int paramCount = element->params().size();
    for (int i = 0; i < paramCount && 7 + i < parts.size(); ++i) {
        bool good = false;
        const double value = parts[7 + i].toDouble(&good);
        if (good && std::isfinite(value))
            element->setParam(i, value);
    }
    return element;
}

std::unique_ptr<Element> Element::create(const QString &code)
{
    auto make = [](Element *element) { return std::unique_ptr<Element>(element); };

    if (code == QLatin1String("w")) return make(new WireElm);
    if (code == QLatin1String("r")) return make(new ResistorElm);
    if (code == QLatin1String("c")) return make(new CapacitorElm);
    if (code == QLatin1String("l")) return make(new InductorElm);
    if (code == QLatin1String("s")) return make(new SwitchElm);
    if (code == QLatin1String("g")) return make(new GroundElm);
    if (code == QLatin1String("v")) return make(new VoltageElm);
    if (code == QLatin1String("vac")) {
        auto *source = new VoltageElm;
        source->wave.shape = Waveform::Sine;
        source->wave.frequency = 100.0;
        return make(source);
    }
    if (code == QLatin1String("vsq")) {
        auto *source = new VoltageElm;
        source->wave.shape = Waveform::Square;
        source->wave.frequency = 100.0;
        return make(source);
    }
    if (code == QLatin1String("rail")) return make(new RailElm);
    if (code == QLatin1String("logic")) return make(new LogicInputElm);
    if (code == QLatin1String("i")) return make(new CurrentElm);
    if (code == QLatin1String("d")) return make(new DiodeElm(false));
    if (code == QLatin1String("led")) return make(new DiodeElm(true));
    if (code == QLatin1String("npn")) return make(new BjtElm(false));
    if (code == QLatin1String("pnp")) return make(new BjtElm(true));
    if (code == QLatin1String("nmos")) return make(new MosfetElm(false));
    if (code == QLatin1String("pmos")) return make(new MosfetElm(true));
    if (code == QLatin1String("opamp")) return make(new OpAmpElm);
    if (code == QLatin1String("and")) return make(new GateElm(GateElm::And));
    if (code == QLatin1String("nand")) return make(new GateElm(GateElm::Nand));
    if (code == QLatin1String("or")) return make(new GateElm(GateElm::Or));
    if (code == QLatin1String("nor")) return make(new GateElm(GateElm::Nor));
    if (code == QLatin1String("xor")) return make(new GateElm(GateElm::Xor));
    if (code == QLatin1String("not")) return make(new GateElm(GateElm::Not));
    if (code == QLatin1String("vm")) return make(new VoltmeterElm);
    if (code == QLatin1String("am")) return make(new AmmeterElm);
    return nullptr;
}

const QList<ToolInfo> &Element::tools()
{
    static const QList<ToolInfo> list = {
        {"w", "Wire", "Passive", "W"},
        {"r", "Resistor", "Passive", "R"},
        {"c", "Capacitor", "Passive", "C"},
        {"l", "Inductor", "Passive", "L"},
        {"s", "Switch", "Passive", "S"},

        {"g", "Ground", "Sources", "G"},
        {"v", "DC source", "Sources", "V"},
        {"vac", "AC source", "Sources", "A"},
        {"vsq", "Square wave", "Sources", ""},
        {"rail", "Voltage rail", "Sources", ""},
        {"i", "Current source", "Sources", "I"},

        {"d", "Diode", "Diodes", "D"},
        {"led", "LED", "Diodes", ""},

        {"npn", "NPN transistor", "Transistors", "N"},
        {"pnp", "PNP transistor", "Transistors", "P"},
        {"nmos", "NMOS MOSFET", "Transistors", "M"},
        {"pmos", "PMOS MOSFET", "Transistors", ""},

        {"opamp", "Op-amp", "Analog", "O"},

        {"logic", "Logic input", "Logic", ""},
        {"and", "AND gate", "Logic", ""},
        {"nand", "NAND gate", "Logic", ""},
        {"or", "OR gate", "Logic", ""},
        {"nor", "NOR gate", "Logic", ""},
        {"xor", "XOR gate", "Logic", ""},
        {"not", "NOT gate (inverter)", "Logic", ""},

        {"vm", "Voltmeter", "Measurement", ""},
        {"am", "Ammeter", "Measurement", ""},
    };
    return list;
}
