#ifndef MOVING_AVERAGE_FILTER_H
#define MOVING_AVERAGE_FILTER_H

#include "filter_base.h"
#include <QQueue>

class MovingAverageFilter : public FilterBase
{
public:
    explicit MovingAverageFilter(int windowSize = 10);
    double process(double input) override;
    void reset() override;
    QString name() const override { return "Moving Average (FIR)"; }

private:
    int windowSize;
    QQueue<double> buffer;
    double sum = 0.0;
};

#endif