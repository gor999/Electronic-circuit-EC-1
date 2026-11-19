#pragma once

#include <QColor>
#include <QPointF>
#include <QString>
#include <QVector>
#include <QWidget>

#include <functional>

struct PlotSeries
{
    QVector<QPointF> points;
    QColor color;
    Qt::PenStyle style = Qt::SolidLine;
    qreal width = 1.8;
    QString name;        // shown in the legend when not empty
    bool dots = false;   // draw points instead of a line
};

struct PlotMarker        // vertical line at x
{
    double x = 0.0;
    QColor color;
    QString label;
};

struct PlotPoint         // highlighted point (the present operating point)
{
    QPointF position;
    QColor color;
};

// Small line-plot widget (QPainter): linear or logarithmic axes, 1-2-5 ticks,
// custom tick formatters, legend, vertical markers and highlighted points.
class PlotWidget : public QWidget
{
public:
    using Formatter = std::function<QString(double)>;

    explicit PlotWidget(QWidget *parent = nullptr);

    void setAxes(const QString &xTitle, Formatter x, bool logX, const QString &yTitle, Formatter y, bool logY);
    void setContent(const QVector<PlotSeries> &series, const QVector<PlotMarker> &markers,
                    const QVector<PlotPoint> &points, const QString &message = QString());
    const QVector<PlotSeries> &series() const { return m_series; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_xTitle, m_yTitle, m_message;
    Formatter m_formatX, m_formatY;
    bool m_logX = false, m_logY = false;
    QVector<PlotSeries> m_series;
    QVector<PlotMarker> m_markers;
    QVector<PlotPoint> m_points;
};
