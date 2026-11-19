#pragma once

#include <QString>

#include <cmath>

// Number formatting shared by the results table and the circuit drawing.
namespace Format {

// Plain number with up to `significant` significant digits, no trailing zeros.
inline QString number(double value, int significant = 6)
{
    if (value == 0.0)
        return QStringLiteral("0");
    return QString::number(value, 'g', significant);
}

// Snaps values that are zero apart from floating-point noise (e.g. a gain of
// -3.9e-15 dB deep in the passband) to exactly 0 for display.
inline double tidy(double value, double threshold = 5.0e-5)
{
    return std::fabs(value) < threshold ? 0.0 : value;
}

// Engineering notation with an SI prefix, e.g. 0.02 A -> "20 mA".
inline QString si(double value, const QString &unit, int significant = 4)
{
    if (value == 0.0 || !std::isfinite(value))
        return number(value, significant) + QLatin1Char(' ') + unit;

    static const char *const prefixes[] = {"p", "n", "µ", "m", "", "k", "M", "G"};
    const int minExp = -12;
    const int maxExp = 9;

    int exp3 = static_cast<int>(std::floor(std::log10(std::fabs(value)) / 3.0)) * 3;
    exp3 = qBound(minExp, exp3, maxExp);
    double scaled = value / std::pow(10.0, exp3);

    // Rounding can push e.g. 999.99 up to 1000; move to the next prefix then.
    if (std::fabs(QString::number(scaled, 'g', significant).toDouble()) >= 1000.0
        && exp3 < maxExp) {
        exp3 += 3;
        scaled = value / std::pow(10.0, exp3);
    }

    const QString prefix = QString::fromUtf8(prefixes[(exp3 - minExp) / 3]);
    return QString::number(scaled, 'g', significant) + QLatin1Char(' ') + prefix + unit;
}

} // namespace Format
