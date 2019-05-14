
#include "ortools/forecaster/fourier_forecaster.h"

namespace operations_research {
namespace forecaster {

FourierForecaster::FourierForecaster() {                                                                                                        
}		

Forecaster::ForecasterType FourierForecaster::GetType() {
   return Forecaster::ForecasterType::Fourier; 
}

FourierForecaster::~FourierForecaster() {

}


} // ns: forecaster

} // ns: operations_research   
