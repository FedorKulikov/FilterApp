#ifndef FILTER_BASE_H
#define FILTER_BASE_H

#include <QVector>

class FilterBase
{
public:
    virtual ~FilterBase() = default;
    virtual double process(double input) = 0;
    virtual void reset() = 0;
    virtual QString name() const = 0;
};

#endif