#include "FIRWorker.h"

FIRWorker::FIRWorker(QObject* parent) : QThread(parent) {}

void FIRWorker::addData(const DataPoint& point)
{
    QMutexLocker locker(&mutex);
    windowBuffer.push_back(point.value);
    if (windowBuffer.size() > windowSize)
        windowBuffer.pop_front();
}

void FIRWorker::setWindowSize(int size)
{
    QMutexLocker locker(&mutex);
    windowSize = std::max(1, size);
}

void FIRWorker::stop()
{
    running = false;
    quit();
    wait();
}

void FIRWorker::run()
{
    while (running)
    {
        DataPoint filtered;
        {
            QMutexLocker locker(&mutex);
            if (windowBuffer.empty()) {
                QThread::msleep(5);
                continue;
            }
            float sum = 0.0f;
            for (float v : windowBuffer) sum += v;
            filtered.value = sum / windowBuffer.size();
            filtered.timestamp = 0;
        }
        emit filteredDataReady(filtered);
        QThread::msleep(10);
    }
}