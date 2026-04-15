#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>
#include "fft.h"
#include <algorithm>

static void fftRecursive(std::vector<std::complex<double>>& x)
{
    const size_t n = x.size();
    if (n <= 1) return;
    std::vector<std::complex<double>> even(n / 2), odd(n / 2);
    for (size_t i = 0; i < n / 2; ++i) {
        even[i] = x[i * 2];
        odd[i] = x[i * 2 + 1];
    }
    fftRecursive(even);
    fftRecursive(odd);
    for (size_t k = 0; k < n / 2; ++k) {
        std::complex<double> t = std::polar(1.0, -2.0 * M_PI * k / n) * odd[k];
        x[k] = even[k] + t;
        x[k + n / 2] = even[k] - t;
    }
}

std::vector<std::complex<double>> FFT::fft(const std::vector<double>& signal)
{
    size_t n = signal.size();
    if ((n & (n - 1)) != 0) {
        n = 1;
        while (n < signal.size()) n <<= 1;
    }
    std::vector<std::complex<double>> x(n, 0.0);
    for (size_t i = 0; i < signal.size(); ++i)
        x[i] = signal[i];
    fftRecursive(x);
    return x;
}

std::vector<double> FFT::amplitudeSpectrumDb(const std::vector<double>& signal)
{
    auto complexSpectrum = fft(signal);
    size_t n = complexSpectrum.size();
    std::vector<double> ampDb(n / 2);
    for (size_t i = 0; i < n / 2; ++i) {
        double mag = std::abs(complexSpectrum[i]);
        double db = 20.0 * std::log10(mag + 1e-10);
        ampDb[i] = db;
    }
    return ampDb;
}