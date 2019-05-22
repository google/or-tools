
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
FFT1DTransform::FFT1DTransform() : 
    need_to_execute_(false) 
{

}


//TODO (dpg): 
FFT1DTransform::~FFT1DTransform() {
 //if (in_ != nullptr)
 //    fftw_free(in_);
}

void FFT1DTransform::execute() {

   need_to_execute_ = true;
}

void FFT1DTransform::clear() {
   need_to_execute_ = false;
}

const fftw_complex * FFT1DTransform::get_result() {
   return out_;
}

void FFT1DTransform::set_input(fftw_complex* in, int N) {
   in_ = in; 
}

//TODO (dpg): 
IFFT1DTransform::IFFT1DTransform() {

}

//TODO (dpg):
IFFT1DTransform::~IFFT1DTransform() {

}

void IFFT1DTransform::set_input(fftw_complex* in, int N) {
   in_ = in;
}

const fftw_complex * IFFT1DTransform::get_result() {
   return out_;
}



} // ns: forecaster

} // ns: operations_research   
