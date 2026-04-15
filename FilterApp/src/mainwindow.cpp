#include "mainwindow.h"
#include <QMessageBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QVBoxLayout>
#include <QComboBox>
#include <QDebug>
#include <algorithm>
#include <chrono>
#include <cmath>

#include "filters/moving_average_filter.h"
#include "filters/exponential_filter.h"
#include "fft.h"

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setupUI();
    initNetwork();
    initFilters();

    plotTimer = new QTimer(this);
    connect(plotTimer, &QTimer::timeout, this, &MainWindow::updatePlot);
    plotTimer->start(30);

    autoSendTimer = new QTimer(this);
    connect(autoSendTimer, &QTimer::timeout, this, &MainWindow::onAutoSendTimeout);
    connect(autoSendCheckbox, &QCheckBox::toggled, this, &MainWindow::onAutoSendToggled);

    spectrumTimer = new QTimer(this);
    connect(spectrumTimer, &QTimer::timeout, this, &MainWindow::updateSpectrum);
    spectrumTimer->start(1000);

    receiveThread = std::thread(&MainWindow::receiveLoop, this);
    firThread = std::thread(&MainWindow::firFilterLoop, this);
    iirThread = std::thread(&MainWindow::iirFilterLoop, this);

    spectrumBufferRaw.reserve(SPECTRUM_SIZE);
    spectrumBufferFir.reserve(SPECTRUM_SIZE);
    spectrumBufferIir.reserve(SPECTRUM_SIZE);
}

MainWindow::~MainWindow()
{
    running = false;
    if (receiveThread.joinable()) receiveThread.join();
    if (firThread.joinable()) firThread.join();
    if (iirThread.joinable()) iirThread.join();
    if (receiveSocket != INVALID_SOCKET_VAL) close_socket(receiveSocket);
    if (sendSocket != INVALID_SOCKET_VAL) close_socket(sendSocket);
#ifdef _WIN32
    WSACleanup();
#endif
}

void MainWindow::setupUI()
{
    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout* mainLayout = new QVBoxLayout(central);

    QGroupBox* sendGroup = new QGroupBox("Отправка целевого значения");
    QHBoxLayout* sendLayout = new QHBoxLayout();
    setpointEdit = new QLineEdit("1.0");
    sendButton = new QPushButton("Отправить");
    sendLayout->addWidget(new QLabel("Целевое значение:"));
    sendLayout->addWidget(setpointEdit);
    sendLayout->addWidget(sendButton);
    sendGroup->setLayout(sendLayout);
    mainLayout->addWidget(sendGroup);

    QGroupBox* netGroup = new QGroupBox("Сетевые настройки");
    QGridLayout* netLayout = new QGridLayout();
    receiveIpEdit = new QLineEdit("127.0.0.1");
    receivePortEdit = new QLineEdit("50006");
    sendIpEdit = new QLineEdit("127.0.0.1");
    sendPortEdit = new QLineEdit("50005");

    netLayout->addWidget(new QLabel("Приём от модели (IP):"), 0, 0);
    netLayout->addWidget(receiveIpEdit, 0, 1);
    netLayout->addWidget(new QLabel("Порт приёма:"), 0, 2);
    netLayout->addWidget(receivePortEdit, 0, 3);

    netLayout->addWidget(new QLabel("Отправка в модель (IP):"), 1, 0);
    netLayout->addWidget(sendIpEdit, 1, 1);
    netLayout->addWidget(new QLabel("Порт отправки:"), 1, 2);
    netLayout->addWidget(sendPortEdit, 1, 3);

    QPushButton* applyNetButton = new QPushButton("Применить настройки");
    netLayout->addWidget(applyNetButton, 2, 0, 1, 4);
    netGroup->setLayout(netLayout);
    mainLayout->addWidget(netGroup);

    QGroupBox* filterGroup = new QGroupBox("Фильтры");
    QGridLayout* filterLayout = new QGridLayout();
    enableFilter1 = new QCheckBox("Скользящее среднее (FIR)");
    enableFilter2 = new QCheckBox("Экспоненциальное сглаживание (IIR)");
    windowSizeSpin = new QSpinBox();
    alphaSpin = new QDoubleSpinBox();

    windowSizeSpin->setRange(5, 200);
    windowSizeSpin->setValue(20);
    windowSizeSpin->setSuffix(" точек");

    alphaSpin->setRange(0.01, 0.99);
    alphaSpin->setValue(0.3);
    alphaSpin->setSingleStep(0.05);
    alphaSpin->setDecimals(2);

    filterLayout->addWidget(enableFilter1, 0, 0);
    filterLayout->addWidget(new QLabel("Размер окна:"), 0, 1);
    filterLayout->addWidget(windowSizeSpin, 0, 2);
    filterLayout->addWidget(enableFilter2, 1, 0);
    filterLayout->addWidget(new QLabel("Коэффициент Alpha:"), 1, 1);
    filterLayout->addWidget(alphaSpin, 1, 2);
    filterGroup->setLayout(filterLayout);
    mainLayout->addWidget(filterGroup);

    QHBoxLayout* depthLayout = new QHBoxLayout();
    depthLayout->addWidget(new QLabel("Глубина графика (точек):"));
    QSpinBox* depthSpin = new QSpinBox();
    depthSpin->setRange(50, 1000);
    depthSpin->setValue(600);
    depthSpin->setSingleStep(50);
    connect(depthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::setMaxPoints);
    depthLayout->addWidget(depthSpin);
    mainLayout->addLayout(depthLayout);

    QGroupBox* autoGroup = new QGroupBox("Автоматическая отправка");
    QHBoxLayout* autoLayout = new QHBoxLayout();
    autoSendCheckbox = new QCheckBox("Включить циклическую отправку");
    autoSendPeriodSpin = new QDoubleSpinBox();
    autoSendPeriodSpin->setRange(0.1, 10.0);
    autoSendPeriodSpin->setValue(1.0);
    autoSendPeriodSpin->setSuffix(" с");
    waveformCombo = new QComboBox();
    waveformCombo->addItems({ "Синус", "Меандр", "Пила", "Треугольник", "Ступенька (0→5→-5)" });
    autoLayout->addWidget(autoSendCheckbox);
    autoLayout->addWidget(new QLabel("Период:"));
    autoLayout->addWidget(autoSendPeriodSpin);
    autoLayout->addWidget(new QLabel("Форма сигнала:"));
    autoLayout->addWidget(waveformCombo);
    autoGroup->setLayout(autoLayout);
    mainLayout->addWidget(autoGroup);

    QGroupBox* spectrumGroup = new QGroupBox("Спектр (dB)");
    QVBoxLayout* spectrumLayout = new QVBoxLayout();
    spectrumPlot = new QCustomPlot(this);
    spectrumPlot->addGraph();
    spectrumPlot->addGraph();
    spectrumPlot->addGraph();
    spectrumPlot->graph(0)->setPen(QPen(Qt::blue, 2));
    spectrumPlot->graph(1)->setPen(QPen(Qt::red, 2));
    spectrumPlot->graph(2)->setPen(QPen(Qt::green, 2));
    spectrumPlot->graph(0)->setName("Исходный спектр");
    spectrumPlot->graph(1)->setName("FIR спектр");
    spectrumPlot->graph(2)->setName("IIR спектр");
    spectrumPlot->xAxis->setLabel("Частота (Гц)");
    spectrumPlot->yAxis->setLabel("Амплитуда (dB)");
    spectrumPlot->legend->setVisible(true);
    spectrumLayout->addWidget(spectrumPlot);
    spectrumGroup->setLayout(spectrumLayout);
    mainLayout->addWidget(spectrumGroup);

    QPushButton* complexityButton = new QPushButton("Анализ сложности фильтров");
    connect(complexityButton, &QPushButton::clicked, this, &MainWindow::showComplexityAnalysis);
    mainLayout->addWidget(complexityButton);

    customPlot = new QCustomPlot(this);
    customPlot->addGraph();
    customPlot->addGraph();
    customPlot->addGraph();

    customPlot->graph(0)->setPen(QPen(Qt::blue, 2));
    customPlot->graph(1)->setPen(QPen(Qt::red, 2));
    customPlot->graph(2)->setPen(QPen(Qt::green, 2));

    customPlot->graph(0)->setName("Исходный сигнал");
    customPlot->graph(1)->setName("Скользящее среднее");
    customPlot->graph(2)->setName("Экспоненциальный");

    customPlot->xAxis->setLabel("Время (секунды)");
    customPlot->yAxis->setLabel("Значение");
    customPlot->legend->setVisible(true);
    customPlot->legend->setBrush(QBrush(QColor(255, 255, 255, 200)));

    mainLayout->addWidget(customPlot, 1);

    connect(sendButton, &QPushButton::clicked, this, &MainWindow::sendSetpoint);
    connect(applyNetButton, &QPushButton::clicked, this, &MainWindow::applyNetworkSettings);
    connect(enableFilter1, &QCheckBox::toggled, this, &MainWindow::onFilterToggled);
    connect(enableFilter2, &QCheckBox::toggled, this, &MainWindow::onFilterToggled);
    connect(windowSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onWindowSizeChanged);
    connect(alphaSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onAlphaChanged);
}

void MainWindow::initNetwork()
{
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    receiveSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(50006);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(receiveSocket, (sockaddr*)&addr, sizeof(addr));

    sendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

void MainWindow::initFilters()
{
    if (filter1) delete filter1;
    if (filter2) delete filter2;
    filter1 = new MovingAverageFilter(windowSizeSpin->value());
    filter2 = new ExponentialFilter(alphaSpin->value());
}

void MainWindow::onFilterToggled()
{
    if (filter1) filter1->reset();
    if (filter2) filter2->reset();
}

void MainWindow::onWindowSizeChanged(int size)
{
    if (filter1) {
        delete filter1;
        filter1 = new MovingAverageFilter(size);
    }
}

void MainWindow::onAlphaChanged(double alpha)
{
    if (filter2) {
        delete filter2;
        filter2 = new ExponentialFilter(alpha);
    }
}

void MainWindow::receiveLoop()
{
    while (running)
    {
        char buffer[8];
        sockaddr_in from{};
        int fromLen = sizeof(from);

        int bytes = recvfrom(receiveSocket, buffer, 8, 0, (sockaddr*)&from, &fromLen);
        if (bytes == 8)
        {
            uint32_t ts;
            float value;
            memcpy(&ts, buffer, 4);
            memcpy(&value, buffer + 4, 4);

            {
                std::lock_guard<std::mutex> lock(rawMutex);
                rawQueue.push((double)value);
            }
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void MainWindow::firFilterLoop()
{
    while (running)
    {
        double input = 0.0;
        bool hasData = false;

        {
            std::lock_guard<std::mutex> lock(rawMutex);
            if (!rawQueue.empty())
            {
                input = rawQueue.front();
                rawQueue.pop();
                hasData = true;
            }
        }

        if (hasData)
        {
            double output = input;
            if (enableFilter1->isChecked() && filter1)
                output = filter1->process(input);

            {
                std::lock_guard<std::mutex> lock(firMutex);
                firResultQueue.push(output);
            }
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void MainWindow::iirFilterLoop()
{
    while (running)
    {
        double input = 0.0;
        bool hasData = false;

        {
            std::lock_guard<std::mutex> lock(rawMutex);
            if (!rawQueue.empty())
            {
                input = rawQueue.front();
                rawQueue.pop();
                hasData = true;
            }
        }

        if (hasData)
        {
            double output = input;
            if (enableFilter2->isChecked() && filter2)
                output = filter2->process(input);

            {
                std::lock_guard<std::mutex> lock(iirMutex);
                iirResultQueue.push(output);
            }
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void MainWindow::updatePlot()
{
    {
        std::lock_guard<std::mutex> lockRaw(rawMutex);
        std::lock_guard<std::mutex> lockFir(firMutex);
        std::lock_guard<std::mutex> lockIir(iirMutex);

        int newPoints = 0;
        if (!rawQueue.empty())
        {
            size_t rawSize = rawQueue.size();
            size_t firSize = firResultQueue.size();
            size_t iirSize = iirResultQueue.size();
            newPoints = static_cast<int>(std::min({ rawSize, firSize, iirSize }));
        }

        for (int i = 0; i < newPoints; ++i)
        {
            double rawVal = rawQueue.front(); rawQueue.pop();
            double firVal = firResultQueue.front(); firResultQueue.pop();
            double iirVal = iirResultQueue.front(); iirResultQueue.pop();

            rawData.append(rawVal);
            filtered1Data.append(firVal);
            filtered2Data.append(iirVal);
            timeData.append(currentTime);
            currentTime += 0.05;
        }
    }

    if (rawData.size() > maxPoints)
    {
        int excess = rawData.size() - maxPoints;
        rawData.remove(0, excess);
        filtered1Data.remove(0, excess);
        filtered2Data.remove(0, excess);
        timeData.remove(0, excess);
    }

    if (rawData.isEmpty())
        return;

    customPlot->graph(0)->setData(timeData, rawData);
    customPlot->graph(1)->setData(timeData, filtered1Data);
    customPlot->graph(2)->setData(timeData, filtered2Data);

    customPlot->graph(0)->setVisible(true);
    customPlot->graph(1)->setVisible(enableFilter1->isChecked());
    customPlot->graph(2)->setVisible(enableFilter2->isChecked());

    double maxTime = timeData.last();
    customPlot->xAxis->setRange(maxTime - 15, maxTime);

    double minVal = *std::min_element(rawData.constBegin(), rawData.constEnd());
    double maxVal = *std::max_element(rawData.constBegin(), rawData.constEnd());
    customPlot->yAxis->setRange(minVal - 2.0, maxVal + 2.0);

    customPlot->replot();
}

void MainWindow::setMaxPoints(int points)
{
    maxPoints = points;
}

void MainWindow::sendSetpoint()
{
    bool ok;
    float setpoint = setpointEdit->text().toFloat(&ok);
    if (!ok) {
        statusBar()->showMessage("Неверное целевое значение", 2000);
        return;
    }

    int port = sendPortEdit->text().toInt();
    QString ipStr = sendIpEdit->text();

    if (sendSocket == INVALID_SOCKET_VAL) {
        sendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sendSocket == INVALID_SOCKET_VAL) {
            statusBar()->showMessage("Не удалось создать сокет отправки", 2000);
            return;
        }
    }

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    inet_pton(AF_INET, ipStr.toStdString().c_str(), &dest.sin_addr);

    int bytesSent = sendto(sendSocket, (char*)&setpoint, sizeof(float), 0, (sockaddr*)&dest, sizeof(dest));

    if (bytesSent == sizeof(float)) {
        statusBar()->showMessage(QString("Отправлено целевое значение: %1").arg(setpoint), 2000);
    }
    else {
        int err = WSAGetLastError();
        statusBar()->showMessage(QString("Ошибка отправки, код: %1").arg(err), 2000);
    }
}

void MainWindow::applyNetworkSettings()
{
    int port = receivePortEdit->text().toInt();
    if (receiveSocket != INVALID_SOCKET_VAL)
        close_socket(receiveSocket);

    receiveSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(receiveSocket, (sockaddr*)&addr, sizeof(addr));

    if (sendSocket != INVALID_SOCKET_VAL) {
        close_socket(sendSocket);
        sendSocket = INVALID_SOCKET_VAL;
    }
    sendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    statusBar()->showMessage("Сетевые настройки применены", 2000);
}

void MainWindow::onAutoSendToggled(bool checked)
{
    if (checked) {
        autoSendTimer->start(int(autoSendPeriodSpin->value() * 1000));
        autoSendTime = 0.0;
    }
    else {
        autoSendTimer->stop();
    }
}

void MainWindow::onAutoSendTimeout()
{
    double setpoint = 0.0;
    double t = autoSendTime;
    int waveform = waveformCombo->currentIndex();
    const double period = autoSendPeriodSpin->value();
    double phase = fmod(t, period) / period;

    switch (waveform) {
    case 0:
        setpoint = 5.0 * sin(2 * M_PI * phase);
        break;
    case 1:
        setpoint = (phase < 0.5) ? 5.0 : -5.0;
        break;
    case 2:
        setpoint = 10.0 * (phase - 0.5);
        break;
    case 3:
        setpoint = 10.0 * (1.0 - 2.0 * fabs(phase - 0.5));
        break;
    case 4:
    {
        int step = int(t / period) % 3;
        if (step == 0) setpoint = 0.0;
        else if (step == 1) setpoint = 5.0;
        else setpoint = -5.0;
    }
    break;
    }
    autoSendTime += 0.05;
    setpointEdit->setText(QString::number(setpoint));
    sendSetpoint();
}

void MainWindow::updateSpectrum()
{
    std::lock_guard<std::mutex> lockRaw(rawMutex);
    std::lock_guard<std::mutex> lockFir(firMutex);
    std::lock_guard<std::mutex> lockIir(iirMutex);

    auto fillBuffer = [](std::queue<double>& q, std::vector<double>& buf, int size) {
        buf.clear();
        std::queue<double> temp = q;
        while (!temp.empty() && buf.size() < size) {
            buf.push_back(temp.front());
            temp.pop();
        }
        if (buf.size() < size) {
            buf.resize(size, 0.0);
        }
        };
    fillBuffer(rawQueue, spectrumBufferRaw, SPECTRUM_SIZE);
    fillBuffer(firResultQueue, spectrumBufferFir, SPECTRUM_SIZE);
    fillBuffer(iirResultQueue, spectrumBufferIir, SPECTRUM_SIZE);

    auto spectrumRaw = FFT::amplitudeSpectrumDb(spectrumBufferRaw);
    auto spectrumFir = FFT::amplitudeSpectrumDb(spectrumBufferFir);
    auto spectrumIir = FFT::amplitudeSpectrumDb(spectrumBufferIir);

    double fs = 20.0;
    int n = (int)spectrumRaw.size();
    QVector<double> freq(n);
    for (int i = 0; i < n; ++i)
        freq[i] = (double)i / n * (fs / 2.0);

    QVector<double> rawVec(spectrumRaw.size());
    QVector<double> firVec(spectrumFir.size());
    QVector<double> iirVec(spectrumIir.size());
    for (size_t i = 0; i < spectrumRaw.size(); ++i) rawVec[i] = spectrumRaw[i];
    for (size_t i = 0; i < spectrumFir.size(); ++i) firVec[i] = spectrumFir[i];
    for (size_t i = 0; i < spectrumIir.size(); ++i) iirVec[i] = spectrumIir[i];

    spectrumPlot->graph(0)->setData(freq, rawVec);
    spectrumPlot->graph(1)->setData(freq, firVec);
    spectrumPlot->graph(2)->setData(freq, iirVec);
    spectrumPlot->rescaleAxes();
    spectrumPlot->replot();
}

void MainWindow::showComplexityAnalysis()
{
    QString text =
        "=== Анализ сложности фильтров ===\n\n"
        "1. Скользящее среднее (FIR)\n"
        "   - Временная сложность: O(N) на отсчёт, где N = размер окна\n"
        "   - Память: O(N) для буфера\n"
        "   - Реализация использует очередь, сумма обновляется инкрементально -> амортизированно O(1)\n\n"
        "2. Экспоненциальное сглаживание (IIR)\n"
        "   - Временная сложность: O(1) на отсчёт\n"
        "   - Память: O(1) (только предыдущее выходное значение)\n"
        "   - Простой фильтр нижних частот первого порядка\n\n"
        "3. БПФ для спектра\n"
        "   - Временная сложность: O(M log M), M = 1024 (фиксировано)\n"
        "   - Память: O(M)\n\n"
        "Примечание: все фильтры работают в отдельных потоках, поэтому GUI остаётся отзывчивым.";
    QMessageBox::information(this, "Анализ сложности", text);
}