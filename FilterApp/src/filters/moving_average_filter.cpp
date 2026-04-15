#include "moving_average_filter.h"

MovingAverageFilter::MovingAverageFilter(int windowSize)
    : windowSize(windowSize)
{
}

double MovingAverageFilter::process(double input)
{
    buffer.enqueue(input);
    sum += input;

    if (buffer.size() > windowSize) {
        sum -= buffer.dequeue();
    }

    return sum / buffer.size();
}

void MovingAverageFilter::reset()
{
    buffer.clear();
    sum = 0.0;
}