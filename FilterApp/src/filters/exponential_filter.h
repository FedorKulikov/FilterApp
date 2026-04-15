#ifndef EXPONENTIAL_FILTER_H
#define EXPONENTIAL_FILTER_H

#include "filter_base.h"

class ExponentialFilter : public FilterBase
{
public:
    explicit ExponentialFilter(double alpha = 0.2);
    double process(double input) override;
    void reset() override;
    QString name() const override { return "Exponential Smoothing (IIR)"; }

private:
    double alpha;
    double previous = 0.0;
    bool first = true;
};

#endif