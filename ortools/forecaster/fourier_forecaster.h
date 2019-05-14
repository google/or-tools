#ifndef OR_TOOLS_FORECASTER_FOURIER_FORECASTER_H_
#define OR_TOOLS_FORECASTER_FOURTIER_FORECASTER_H_
#include <vector>
#include <string>
#include <map>
#include "forecaster.h"


namespace operations_research {
namespace forecaster {

class FourierForecaster : public Forecaster {
  public:
     FourierForecaster();
     ForecasterType GetType();
     ~FourierForecaster();
};




} // ns: forecaster

} // ns: operations_research

#endif // OR_TOOLS_FORECASTER_FORECASTER
