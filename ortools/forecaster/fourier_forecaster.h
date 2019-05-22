#ifndef OR_TOOLS_FORECASTER_FOURIER_FORECASTER_H_
#define OR_TOOLS_FORECASTER_FOURTIER_FORECASTER_H_
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <complex.h>
#include "forecaster.h"
#include "fftw3.h"

namespace operations_research {
namespace forecaster {

class FFT1DTransform; 

class FourierForecaster : public Forecaster {
  public:
     FourierForecaster();
     ForecasterType GetType();
     ~FourierForecaster();
  protected:
     //std::unique_ptr<FFT1DTransform> m_ifft_transform;
     //std::unique_ptr<FFT1DTransform> m_fft_transform;
};

class FFT1DTransform {
  public:
    enum TransformError {
      SUCCESS=0,
      MISSING_DATA=1,
      INTERNAL_ERROR=2,
      UNSPECIFIED=4
    };
    FFT1DTransform();
    virtual void execute(std::unordered_map<int,double> data, int N) = 0;
    virtual const fftw_complex* get_result() = 0;
    virtual void clear() = 0; 
  protected:
    fftw_complex * in_;
    fftw_complex * out_; 
    fftw_plan plan_;
    bool need_to_clear_;

};

class Forward1DTransform : public FFT1DTransform {
  public:
      Forward1DTransform();
      ~Forward1DTransform();
      void execute(std::unordered_map<int,double> data, int N) override;
      const fftw_complex* get_result() override;
      void clear() override;
};

class Inverse1DTransform : public FFT1DTransform {
  public:
      Inverse1DTransform();
      ~Inverse1DTransform();
      void execute(std::unordered_map<int,double>, int N) override;
      const fftw_complex* get_result() override;
      void clear() override;
};

} // ns: forecaster


} // ns: operations_research

#endif // OR_TOOLS_FORECASTER_FOURIER_FORECASTER_H_
