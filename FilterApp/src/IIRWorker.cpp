#include "IIRWorker.h"

IIRWorker::IIRWorker(QObject* parent) : QThread(parent) {}

void IIRWorker::addData(const DataPoint& point)
{
    QMutexLocker locker(&mutex);
    if (firstValue) {
        lastValue = point.value;
        firstValue = false;
    }
    lastValue = static_cast<float>(alpha * point.value + (1.0 - alpha) * lastValue);
}

void IIRWorker::setAlpha(double newAlpha)
{
    QMutexLocker locker(&mutex);
    alpha = std::clamp(newAlpha, 0.01, 0.99);
}

void IIRWorker::stop()
{
    running = false;
    quit();
    wait();
}

void IIRWorker::run()
{
    while (running)
    {
        DataPoint filtered;
        {
            QMutexLocker locker(&mutex);
            filtered.value = lastValue;
            filtered.timestamp = 0;
        }
        emit filteredDataReady(filtered);
        QThread::msleep(10);
    }
}