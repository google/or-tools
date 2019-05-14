#ifndef OR_TOOLS_FORECASTER_FORECASTER_H_
#define OR_TOOLS_FORECASTER_FORECASTER_H_
#include <vector>
#include <string>
#include <map>


#endif // OR_TOOLS_FORECASTER_FORECASTER

namespace operations_research {
namespace forecaster {

class Forecaster {
  public:
    enum ForecasterType {
      Fourier = 0,
      SuperResolution = 1,
    };

    virtual ForecasterType GetType() = 0;
    ~Forecaster();
};




} // ns: forecaster

} // ns: operations_research
