#ifndef OR_TOOLS_FORECASTER_FOURIER_FORECASTER_H_
#define OR_TOOLS_FORECASTER_FOURTIER_FORECASTER_H_
#include <vector>
#include <string>
#include <memory>
#include <map>
#include <complex>
#include "forecaster.h"
#include "fftw3.h"

namespace operations_research {
namespace forecaster {

class FFT1DTransform;
class IFFT1DTransform;

class FourierForecaster : public Forecaster {
  public:
     FourierForecaster();
     ForecasterType GetType();
     ~FourierForecaster();
  protected:
     std::unique_ptr<IFFT1DTransform> m_fft1d_transform;
};

class AbstractTransform {
  public:
    enum TransformError {
      SUCCESS=0,
      MISSING_DATA=1,
      INTERNAL_ERROR=2,
      UNSPECIFIED=4
    };

    virtual void set_input(fftw_complex* in) = 0;
    virtual void execute(fftw_complex* in, int N) = 0;
    virtual const fftw_complex* get_result() = 0;
    virtual void clear() = 0; 


};

class FFT1DTransform : public AbstractTransform {
  public:
      FFT1DTransform();
      ~FFT1DTransform();
      void set_input(fftw_complex* in) override;
      void execute(fftw_complex* in, int N) override;
      const fftw_complex* get_result() override;
      void clear() override;
  protected:
      fftw_complex * in_;
      fftw_complex * out_;

};

class IFFT1DTransform {
  public:
      IFFT1DTransform();
      ~IFFT1DTransform();

};

} // ns: forecaster


} // ns: operations_research

#endif // OR_TOOLS_FORECASTER_FOURIER_FORECASTER_H_
