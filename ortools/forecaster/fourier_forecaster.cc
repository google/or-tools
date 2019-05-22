
#include "ortools/forecaster/fourier_forecaster.h"

namespace operations_research {
namespace forecaster {

//TODO (dpg):
FourierForecaster::FourierForecaster() {                                                                                                        
}		

//TODO (dpg):
Forecaster::ForecasterType FourierForecaster::GetType() {
   return Forecaster::ForecasterType::Fourier; 
}

//TODO (dpg):
FourierForecaster::~FourierForecaster() {

}

//TODO (dpg): 
FFT1DTransform::FFT1DTransform() {

}

//TODO (dpg): 
FFT1DTransform::~FFT1DTransform() {
 if (in_ != nullptr)
     fftw_free(in_);
}

const fftw_complex * FFT1DTransform::get_result() {
   return out_;
}

void FFT1DTransform::set_input(fftw_complex* in) {
   in_ = in; 
}

//TODO (dpg): 
IFFT1DTransform::IFFT1DTransform() {

}

//TODO (dpg):
IFFT1DTransform::~IFFT1DTransform() {

}

} // ns: forecaster

} // ns: operations_research   
