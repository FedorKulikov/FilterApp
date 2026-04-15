#include "exponential_filter.h"

ExponentialFilter::ExponentialFilter(double alpha)
    : alpha(alpha)
{
}

double ExponentialFilter::process(double input)
{
    if (first) {
        previous = input;
        first = false;
        return input;
    }

    previous = alpha * input + (1.0 - alpha) * previous;
    return previous;
}

void ExponentialFilter::reset()
{
    previous = 0.0;
    first = true;
}