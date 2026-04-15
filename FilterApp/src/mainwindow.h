#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif


#include <QMainWindow>
#include <QVector>
#include <QTimer>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <complex>
#include <vector>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "qcustomplot.h"
#include "filters/filter_base.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET socket_t;
#define INVALID_SOCKET_VAL INVALID_SOCKET
#define SOCKET_ERROR_VAL SOCKET_ERROR
#define close_socket(s) closesocket(s)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
typedef int socket_t;
#define INVALID_SOCKET_VAL -1
#define SOCKET_ERROR_VAL -1
#define close_socket(s) close(s)
#endif

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void sendSetpoint();
    void applyNetworkSettings();
    void updatePlot();
    void setMaxPoints(int points);
    void onFilterToggled();
    void onWindowSizeChanged(int size);
    void onAlphaChanged(double alpha);
    void updateSpectrum();
    void onAutoSendToggled(bool checked);
    void onAutoSendTimeout();
    void showComplexityAnalysis();

private:
    void setupUI();
    void initNetwork();
    void initFilters();
    void receiveLoop();
    void firFilterLoop();
    void iirFilterLoop();

    socket_t receiveSocket = INVALID_SOCKET_VAL;
    socket_t sendSocket = INVALID_SOCKET_VAL;

    QCustomPlot* customPlot = nullptr;
    QCustomPlot* spectrumPlot = nullptr;

    QVector<double> timeData;
    QVector<double> rawData;
    QVector<double> filtered1Data;
    QVector<double> filtered2Data;

    FilterBase* filter1 = nullptr;
    FilterBase* filter2 = nullptr;

    QLineEdit* setpointEdit = nullptr;
    QLineEdit* receiveIpEdit = nullptr;
    QLineEdit* receivePortEdit = nullptr;
    QLineEdit* sendIpEdit = nullptr;
    QLineEdit* sendPortEdit = nullptr;

    QCheckBox* enableFilter1 = nullptr;
    QCheckBox* enableFilter2 = nullptr;
    QSpinBox* windowSizeSpin = nullptr;
    QDoubleSpinBox* alphaSpin = nullptr;

    QCheckBox* autoSendCheckbox = nullptr;
    QDoubleSpinBox* autoSendPeriodSpin = nullptr;
    QComboBox* waveformCombo = nullptr;

    QPushButton* sendButton = nullptr;
    QTimer* plotTimer = nullptr;
    QTimer* autoSendTimer = nullptr;
    QTimer* spectrumTimer = nullptr;

    std::thread receiveThread;
    std::thread firThread;
    std::thread iirThread;

    std::queue<double> rawQueue;
    std::queue<double> firResultQueue;
    std::queue<double> iirResultQueue;

    std::mutex rawMutex;
    std::mutex firMutex;
    std::mutex iirMutex;

    std::atomic<bool> running{ true };

    int maxPoints = 600;
    double currentTime = 0.0;
    double autoSendTime = 0.0;

    static const int SPECTRUM_SIZE = 1024;
    std::vector<double> spectrumBufferRaw;
    std::vector<double> spectrumBufferFir;
    std::vector<double> spectrumBufferIir;
    std::mutex spectrumMutex;
};

#endif