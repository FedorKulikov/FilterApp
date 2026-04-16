#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QMessageBox>
#include <QThread>
#include <cmath>
#include "filters/moving_average_filter.h"
#include "filters/exponential_filter.h"
#include "fft.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUI();
    initNetwork();
    initFilters();

    plotTimer = new QTimer(this);
    connect(plotTimer, &QTimer::timeout, this, &MainWindow::updatePlot);
    plotTimer->start(50);

    spectrumTimer = new QTimer(this);
    connect(spectrumTimer, &QTimer::timeout, this, &MainWindow::updateSpectrum);
    spectrumTimer->start(100);

    autoSendTimer = new QTimer(this);
    connect(autoSendTimer, &QTimer::timeout, this, &MainWindow::onAutoSendTimeout);

    running = true;
    receiveThread = std::thread(&MainWindow::receiveLoop, this);
    firThread = std::thread(&MainWindow::firFilterLoop, this);
    iirThread = std::thread(&MainWindow::iirFilterLoop, this);
}

MainWindow::~MainWindow()
{
    running = false;
    if (receiveThread.joinable()) receiveThread.join();
    if (firThread.joinable()) firThread.join();
    if (iirThread.joinable()) iirThread.join();

    if (receiveSocket != INVALID_SOCKET_VAL) ::close_socket(receiveSocket);
    if (sendSocket != INVALID_SOCKET_VAL) ::close_socket(sendSocket);
}

void MainWindow::setupUI()
{
    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    QHBoxLayout* mainLayout = new QHBoxLayout(central);

    QVBoxLayout* leftLayout = new QVBoxLayout();
    QVBoxLayout* rightLayout = new QVBoxLayout();

    customPlot = new QCustomPlot();
    customPlot->addGraph();
    customPlot->addGraph();
    customPlot->addGraph();
    customPlot->graph(0)->setPen(QPen(Qt::blue));
    customPlot->graph(1)->setPen(QPen(Qt::red));
    customPlot->graph(2)->setPen(QPen(Qt::green));
    customPlot->xAxis->setLabel("Time");
    customPlot->yAxis->setLabel("Value");
    leftLayout->addWidget(customPlot);

    spectrumPlot = new QCustomPlot();
    spectrumPlot->addGraph();
    spectrumPlot->graph(0)->setPen(QPen(Qt::blue));
    spectrumPlot->xAxis->setLabel("Frequency");
    spectrumPlot->yAxis->setLabel("Magnitude");
    leftLayout->addWidget(spectrumPlot);

    QGroupBox* networkGroup = new QGroupBox("Network Settings");
    QFormLayout* networkLayout = new QFormLayout(networkGroup);
    receiveIpEdit = new QLineEdit("127.0.0.1");
    receivePortEdit = new QLineEdit("12345");
    sendIpEdit = new QLineEdit("127.0.0.1");
    sendPortEdit = new QLineEdit("12346");
    networkLayout->addRow("Receive IP:", receiveIpEdit);
    networkLayout->addRow("Receive Port:", receivePortEdit);
    networkLayout->addRow("Send IP:", sendIpEdit);
    networkLayout->addRow("Send Port:", sendPortEdit);
    QPushButton* applyButton = new QPushButton("Apply");
    connect(applyButton, &QPushButton::clicked, this, &MainWindow::applyNetworkSettings);
    networkLayout->addRow(applyButton);
    rightLayout->addWidget(networkGroup);

    QGroupBox* filterGroup = new QGroupBox("Filters");
    QFormLayout* filterLayout = new QFormLayout(filterGroup);
    enableFilter1 = new QCheckBox("Moving Average");
    enableFilter2 = new QCheckBox("Exponential");
    windowSizeSpin = new QSpinBox();
    windowSizeSpin->setRange(1, 100);
    windowSizeSpin->setValue(5);
    alphaSpin = new QDoubleSpinBox();
    alphaSpin->setRange(0.0, 1.0);
    alphaSpin->setSingleStep(0.05);
    alphaSpin->setValue(0.2);
    filterLayout->addRow(enableFilter1);
    filterLayout->addRow("Window Size:", windowSizeSpin);
    filterLayout->addRow(enableFilter2);
    filterLayout->addRow("Alpha:", alphaSpin);
    QPushButton* complexityButton = new QPushButton("Complexity Analysis");
    connect(complexityButton, &QPushButton::clicked, this, &MainWindow::showComplexityAnalysis);
    filterLayout->addRow(complexityButton);
    rightLayout->addWidget(filterGroup);

    QGroupBox* controlGroup = new QGroupBox("Control");
    QFormLayout* controlLayout = new QFormLayout(controlGroup);
    setpointEdit = new QLineEdit("0");
    sendButton = new QPushButton("Send Setpoint");
    connect(sendButton, &QPushButton::clicked, this, &MainWindow::sendSetpoint);
    controlLayout->addRow("Setpoint:", setpointEdit);
    controlLayout->addRow(sendButton);
    autoSendCheckbox = new QCheckBox("Auto Send");
    autoSendPeriodSpin = new QDoubleSpinBox();
    autoSendPeriodSpin->setRange(0.1, 10.0);
    autoSendPeriodSpin->setSingleStep(0.1);
    autoSendPeriodSpin->setValue(1.0);
    waveformCombo = new QComboBox();
    waveformCombo->addItems({"Sine", "Square", "Triangle"});
    controlLayout->addRow(autoSendCheckbox);
    controlLayout->addRow("Period (s):", autoSendPeriodSpin);
    controlLayout->addRow("Waveform:", waveformCombo);
    rightLayout->addWidget(controlGroup);

    QGroupBox* displayGroup = new QGroupBox("Display");
    QFormLayout* displayLayout = new QFormLayout(displayGroup);
    QSpinBox* maxPointsSpin = new QSpinBox();
    maxPointsSpin->setRange(100, 2000);
    maxPointsSpin->setValue(600);
    connect(maxPointsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::setMaxPoints);
    displayLayout->addRow("Max Points:", maxPointsSpin);
    rightLayout->addWidget(displayGroup);

    mainLayout->addLayout(leftLayout, 2);
    mainLayout->addLayout(rightLayout, 1);
}

void MainWindow::initNetwork()
{
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

void MainWindow::initFilters()
{
    filter1 = new MovingAverageFilter(5);
    filter2 = new ExponentialFilter(0.2);
}

void MainWindow::receiveLoop()
{
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(receivePortEdit->text().toUShort());
    inet_pton(AF_INET, receiveIpEdit->text().toStdString().c_str(), &addr.sin_addr);

    receiveSocket = socket(AF_INET, SOCK_DGRAM, 0);
    bind(receiveSocket, (sockaddr*)&addr, sizeof(addr));

    while (running) {
        char buffer[8];
        sockaddr_in from;
        socklen_t fromLen = sizeof(from);
        int bytes = recvfrom(receiveSocket, buffer, 8, 0, (sockaddr*)&from, &fromLen);
        if (bytes == 8) {
            double value;
            memcpy(&value, buffer, 8);
            std::lock_guard<std::mutex> lock(rawMutex);
            rawQueue.push(value);
        }
    }
}

void MainWindow::firFilterLoop()
{
    while (running) {
        double value = 0;
        bool hasData = false;
        {
            std::lock_guard<std::mutex> lock(rawMutex);
            if (!rawQueue.empty()) {
                value = rawQueue.front();
                rawQueue.pop();
                hasData = true;
            }
        }
        if (hasData && enableFilter1->isChecked()) {
            double filtered = filter1->process(value);
            std::lock_guard<std::mutex> lock(firMutex);
            firResultQueue.push(filtered);
        } else if (hasData) {
            std::lock_guard<std::mutex> lock(firMutex);
            firResultQueue.push(value);
        }
        QThread::msleep(1);
    }
}

void MainWindow::iirFilterLoop()
{
    while (running) {
        double value = 0;
        bool hasData = false;
        {
            std::lock_guard<std::mutex> lock(firMutex);
            if (!firResultQueue.empty()) {
                value = firResultQueue.front();
                firResultQueue.pop();
                hasData = true;
            }
        }
        if (hasData && enableFilter2->isChecked()) {
            double filtered = filter2->process(value);
            std::lock_guard<std::mutex> lock(iirMutex);
            iirResultQueue.push(filtered);
        } else if (hasData) {
            std::lock_guard<std::mutex> lock(iirMutex);
            iirResultQueue.push(value);
        }
        QThread::msleep(1);
    }
}

void MainWindow::updatePlot()
{
    double rawVal = 0, filteredVal = 0;
    bool hasRaw = false, hasFiltered = false;

    {
        std::lock_guard<std::mutex> lock(iirMutex);
        if (!iirResultQueue.empty()) {
            filteredVal = iirResultQueue.front();
            iirResultQueue.pop();
            hasFiltered = true;
        }
    }
    {
        std::lock_guard<std::mutex> lock(rawMutex);
        if (!rawQueue.empty()) {
            rawVal = rawQueue.front();
            rawQueue.pop();
            hasRaw = true;
        }
    }

    if (hasRaw) {
        rawData.append(rawVal);
        filtered1Data.append(rawVal);
        if (rawData.size() > maxPoints) rawData.removeFirst();
        if (filtered1Data.size() > maxPoints) filtered1Data.removeFirst();
    }
    if (hasFiltered) {
        filtered2Data.append(filteredVal);
        if (filtered2Data.size() > maxPoints) filtered2Data.removeFirst();
    }

    if (hasRaw || hasFiltered) {
        QVector<double> x(rawData.size());
        for (int i = 0; i < rawData.size(); ++i) x[i] = i;
        customPlot->graph(0)->setData(x, rawData);
        customPlot->graph(1)->setData(x, filtered1Data);
        customPlot->graph(2)->setData(x, filtered2Data);
        customPlot->xAxis->rescale();
        customPlot->yAxis->rescale();
        customPlot->replot();
    }
}

void MainWindow::updateSpectrum()
{
    std::vector<double> rawCopy, firCopy, iirCopy;
    {
        std::lock_guard<std::mutex> lock(rawMutex);
        rawCopy.assign(spectrumBufferRaw.begin(), spectrumBufferRaw.end());
    }
    {
        std::lock_guard<std::mutex> lock(firMutex);
        firCopy.assign(spectrumBufferFir.begin(), spectrumBufferFir.end());
    }
    {
        std::lock_guard<std::mutex> lock(iirMutex);
        iirCopy.assign(spectrumBufferIir.begin(), spectrumBufferIir.end());
    }

    if (rawCopy.size() > 0) {
        std::vector<std::complex<double>> fftRaw = fft(rawCopy);
        QVector<double> freq(fftRaw.size()), mag(fftRaw.size());
        for (size_t i = 0; i < fftRaw.size(); ++i) {
            freq[i] = i;
            mag[i] = std::abs(fftRaw[i]);
        }
        spectrumPlot->graph(0)->setData(freq, mag);
        spectrumPlot->replot();
    }
}

void MainWindow::setMaxPoints(int points)
{
    maxPoints = points;
    while (rawData.size() > maxPoints) rawData.removeFirst();
    while (filtered1Data.size() > maxPoints) filtered1Data.removeFirst();
    while (filtered2Data.size() > maxPoints) filtered2Data.removeFirst();
}

void MainWindow::onFilterToggled()
{
    filter1->reset();
    filter2->reset();
}

void MainWindow::onWindowSizeChanged(int size)
{
    delete filter1;
    filter1 = new MovingAverageFilter(size);
}

void MainWindow::onAlphaChanged(double alpha)
{
    delete filter2;
    filter2 = new ExponentialFilter(alpha);
}

void MainWindow::sendSetpoint()
{
    double value = setpointEdit->text().toDouble();
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(sendPortEdit->text().toUShort());
    inet_pton(AF_INET, sendIpEdit->text().toStdString().c_str(), &addr.sin_addr);

    if (sendSocket == INVALID_SOCKET_VAL) {
        sendSocket = socket(AF_INET, SOCK_DGRAM, 0);
    }

    char buffer[8];
    memcpy(buffer, &value, 8);
    int result = sendto(sendSocket, buffer, 8, 0, (sockaddr*)&addr, sizeof(addr));
    if (result == -1) {
#ifdef _WIN32
        int err = WSAGetLastError();
#else
        int err = errno;
#endif
        QMessageBox::warning(this, "Error", QString("Send failed with error: %1").arg(err));
    }
}

void MainWindow::applyNetworkSettings()
{
    running = false;
    if (receiveThread.joinable()) receiveThread.join();
    if (firThread.joinable()) firThread.join();
    if (iirThread.joinable()) iirThread.join();

    if (receiveSocket != INVALID_SOCKET_VAL) ::close_socket(receiveSocket);
    if (sendSocket != INVALID_SOCKET_VAL) ::close_socket(sendSocket);

    receiveSocket = INVALID_SOCKET_VAL;
    sendSocket = INVALID_SOCKET_VAL;

    running = true;
    receiveThread = std::thread(&MainWindow::receiveLoop, this);
    firThread = std::thread(&MainWindow::firFilterLoop, this);
    iirThread = std::thread(&MainWindow::iirFilterLoop, this);
}

void MainWindow::onAutoSendToggled(bool checked)
{
    if (checked) {
        autoSendTimer->start(static_cast<int>(autoSendPeriodSpin->value() * 1000));
    } else {
        autoSendTimer->stop();
    }
}

void MainWindow::onAutoSendTimeout()
{
    double t = autoSendTime;
    double value = 0;
    QString waveform = waveformCombo->currentText();
    if (waveform == "Sine") {
        value = sin(t * 2 * M_PI);
    } else if (waveform == "Square") {
        value = (sin(t * 2 * M_PI) >= 0) ? 1.0 : -1.0;
    } else if (waveform == "Triangle") {
        value = 2 * fabs(2 * (t - floor(t + 0.5))) - 1;
    }
    autoSendTime += autoSendPeriodSpin->value();
    setpointEdit->setText(QString::number(value));
    sendSetpoint();
}

void MainWindow::showComplexityAnalysis()
{
    QString msg = "Moving Average: O(n)\nExponential: O(1)";
    QMessageBox::information(this, "Complexity Analysis", msg);
}

void MainWindow::onAutoSendToggled(bool checked)
{
    if (checked) {
        autoSendTimer->start(static_cast<int>(autoSendPeriodSpin->value() * 1000));
    } else {
        autoSendTimer->stop();
    }
}
