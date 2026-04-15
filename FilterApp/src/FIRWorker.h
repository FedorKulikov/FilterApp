#pragma once
#include <QThread>
#include <QMutex>
#include <deque>
#include "DataPoint.h"

class FIRWorker : public QThread
{
    Q_OBJECT
public:
    explicit FIRWorker(QObject* parent = nullptr);
    void addData(const DataPoint& point);
    void setWindowSize(int size);
    void stop();

signals:
    void filteredDataReady(const DataPoint& filteredPoint);

protected:
    void run() override;

private:
    std::deque<float> windowBuffer;
    int windowSize = 14;
    mutable QMutex mutex;
    bool running = true;
};