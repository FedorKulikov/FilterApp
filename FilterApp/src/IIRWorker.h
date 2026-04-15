#pragma once
#include <QThread>
#include <QMutex>
#include <deque>
#include "DataPoint.h"

class IIRWorker : public QThread
{
    Q_OBJECT
public:
    explicit IIRWorker(QObject* parent = nullptr);
    void addData(const DataPoint& point);
    void setAlpha(double alpha);
    void stop();

signals:
    void filteredDataReady(const DataPoint& filteredPoint);

protected:
    void run() override;

private:
    float lastValue = 0.0f;
    bool firstValue = true;
    double alpha = 0.30;
    mutable QMutex mutex;
    bool running = true;
};