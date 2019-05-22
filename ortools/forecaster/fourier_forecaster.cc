
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

FFT1DTransform::FFT1DTransform() :
   need_to_clear_(false), in_(nullptr), out_(nullptr) 
{
   
}

//TODO (dpg): 
Forward1DTransform::Forward1DTransform()  
{

}


//TODO (dpg): 
Forward1DTransform::~Forward1DTransform() {
 //if (in_ != nullptr)
 //    fftw_free(in_);
}


void Forward1DTransform::clear() {
   need_to_clear_ = false;
}

const fftw_complex * Forward1DTransform::get_result() {
   return out_;
}

void Forward1DTransform::execute(std::unordered_map<int,double> data, int N) {
   //TODO (dpg): execute here
   //
   need_to_clear_ = true;
}

//TODO (dpg): 
Inverse1DTransform::Inverse1DTransform() {

}

//TODO (dpg):
Inverse1DTransform::~Inverse1DTransform() {

}

const fftw_complex * Inverse1DTransform::get_result() {
   return out_;
}

void Inverse1DTransform::execute(std::unordered_map<int,double> data, int N) {
   //TODO (dpg): execute here
   //
   in_ = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);
   out_ = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);
   plan_ = fftw_plan_dft_1d(N, in_, out_, FFTW_FORWARD, FFTW_ESTIMATE);
   fftw_execute(plan_);
   need_to_clear_ = true;

}



} // ns: forecaster

} // ns: operations_research   
