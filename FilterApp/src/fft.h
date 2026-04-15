#ifndef FFT_H
#define FFT_H

#include <complex>
#include <vector>
#include <cmath>

class FFT
{
public:
    static std::vector<std::complex<double>> fft(const std::vector<double>& signal);
    static std::vector<double> amplitudeSpectrumDb(const std::vector<double>& signal);
};

#endif