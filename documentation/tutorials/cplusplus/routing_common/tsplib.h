// Copyright 2011-2014 Google
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//
//  Definitions for the TSPLIB format.

#ifndef OR_TOOLS_TUTORIALS_CPLUSPLUS_TSPLIB_H
#define OR_TOOLS_TUTORIALS_CPLUSPLUS_TSPLIB_H

#include <cmath>
#include <limits>

//#include "base/commandlineflags.h"
#include "base/integral_types.h"

#include "routing_common.h"

//DEFINE_bool(tsp_start_counting_at_1, true, "TSPLIB convention: first node is 1 (not 0).");

//  You can find the technical description of the TSPLIB in
//  http://comopt.ifi.uni-heidelberg.de/software/TSPLIB95/DOC.PS

// EOF is tested separately because it is sometimes redefined

namespace operations_research {

typedef int64 (*TWOD_distance_function)(const Point, const Point);
typedef int64 (*THREED_distance_function)(const Point, const Point);
  
const int kTSPLIBDelimiter = -1;
const char * kTSPLIBEndFileDelimiter = "EOF";

#define TSPLIB_STATES         \
        ENUM_OR_STRING( NAME ),    \
        ENUM_OR_STRING( TYPE ),    \
        ENUM_OR_STRING( COMMENT ),  \
        ENUM_OR_STRING( DIMENSION ),  \
        ENUM_OR_STRING( CAPACITY ),  \
        ENUM_OR_STRING( EDGE_WEIGHT_TYPE ),  \
        ENUM_OR_STRING( EDGE_WEIGHT_FORMAT ),  \
        ENUM_OR_STRING( EDGE_DATA_FORMAT ),  \
        ENUM_OR_STRING( NODE_COORD_TYPE ),  \
        ENUM_OR_STRING( DISPLAY_DATA_TYPE ),  \
        ENUM_OR_STRING( NODE_COORD_SECTION ),  \
        ENUM_OR_STRING( DEPOT_SECTION ),  \
        ENUM_OR_STRING( DEMAND_SECTION ),  \
        ENUM_OR_STRING( EDGE_DATA_SECTION ),  \
        ENUM_OR_STRING( FIXED_EDGE_SECTION ),  \
        ENUM_OR_STRING( DISPLAY_DATA_SECTION ),  \
        ENUM_OR_STRING( TOUR_SECTION ),  \
        ENUM_OR_STRING( EDGE_WEIGHT_SECTION ),  \
        ENUM_OR_STRING( TSPLIB_STATES_COUNT ),  \
        ENUM_OR_STRING( TSPLIB_STATES_UNDEFINED )

//  TYPE
#define TSPLIB_PROBLEM_TYPES  \
  ENUM_OR_STRING( TSP ), \
  ENUM_OR_STRING( ATSP ), \
  ENUM_OR_STRING( CVRP ), \
  ENUM_OR_STRING( CCPP ), \
  ENUM_OR_STRING( TOUR ), \
  ENUM_OR_STRING( TSPLIB_PROBLEM_TYPES_COUNT ), \
  ENUM_OR_STRING( TSPLIB_PROBLEM_TYPES_UNDEFINED )

//  EDGE_WEIGHT_TYPE
#define TSPLIB_EDGE_WEIGHT_TYPES \
  ENUM_OR_STRING( ATT ), \
  ENUM_OR_STRING( CEIL_2D ), \
  ENUM_OR_STRING( CEIL_3D ), \
  ENUM_OR_STRING( EUC_2D ), \
  ENUM_OR_STRING( EUC_3D ), \
  ENUM_OR_STRING( EXPLICIT ), \
  ENUM_OR_STRING( GEO ), \
  ENUM_OR_STRING( GEOM ), \
  ENUM_OR_STRING( GEO_MEEUS ), \
  ENUM_OR_STRING( GEOM_MEEUS ), \
  ENUM_OR_STRING( MAN_2D ), \
  ENUM_OR_STRING( MAN_3D ), \
  ENUM_OR_STRING( MAX_2D ), \
  ENUM_OR_STRING( MAX_3D ), \
  ENUM_OR_STRING( TSPLIB_EDGE_WEIGHT_TYPES_COUNT ), \
  ENUM_OR_STRING( TSPLIB_EDGE_WEIGHT_TYPES_UNDEFINED )

//  EDGE_WEIGHT_FORMAT
#define TSPLIB_EDGE_WEIGHT_FORMAT_TYPES \
  ENUM_OR_STRING( FUNCTION ), \
  ENUM_OR_STRING( FULL_MATRIX ), \
  ENUM_OR_STRING( UPPER_ROW ), \
  ENUM_OR_STRING( LOWER_ROW ), \
  ENUM_OR_STRING( UPPER_DIAG_ROW ), \
  ENUM_OR_STRING( LOWER_DIAG_ROW ), \
  ENUM_OR_STRING( UPPER_COL ), \
  ENUM_OR_STRING( LOWER_COL ), \
  ENUM_OR_STRING( UPPER_DIAG_COL ), \
  ENUM_OR_STRING( LOWER_DIAG_COL ), \
  ENUM_OR_STRING( TSPLIB_EDGE_WEIGHT_FORMAT_TYPES_COUNT ), \
  ENUM_OR_STRING( TSPLIB_EDGE_WEIGHT_FORMAT_TYPES_UNDEFINED )

//  EDGE_DATA_FORMAT
#define TSPLIB_EDGE_DATA_FORMAT_TYPES \
  ENUM_OR_STRING( EDGE_LIST ), \
  ENUM_OR_STRING( ADJ_LIST ), \
  ENUM_OR_STRING( TSPLIB_EDGE_DATA_FORMAT_TYPES_COUNT ), \
  ENUM_OR_STRING( TSPLIB_EDGE_DATA_FORMAT_TYPES_UNDEFINED )

//  NODE_COORD_TYPE
#define TSPLIB_NODE_COORD_TYPE_TYPES \
  ENUM_OR_STRING( TWOD_COORDS ), \
  ENUM_OR_STRING( THREED_COORDS ), \
  ENUM_OR_STRING( NO_COORDS ), \
  ENUM_OR_STRING( TSPLIB_NODE_COORD_TYPE_TYPES_COUNT ), \
  ENUM_OR_STRING( TSPLIB_NODE_COORD_TYPE_TYPES_UNDEFINED )

//  DISPLAY_DATA_TYPE
#define TSPLIB_DISPLAY_DATA_TYPE_TYPES \
  ENUM_OR_STRING( COORD_DISPLAY ), \
  ENUM_OR_STRING( TWOD_DISPLAY ), \
  ENUM_OR_STRING( NO_DISPLAY ), \
  ENUM_OR_STRING( TSPLIB_DISPLAY_DATA_TYPE_TYPES_COUNT ), \
  ENUM_OR_STRING( TSPLIB_DISPLAY_DATA_TYPE_TYPES_UNDEFINED )


// Enum

#undef ENUM_OR_STRING
#define ENUM_OR_STRING( x ) x

enum TSPLIB_STATES_enum
{
    TSPLIB_STATES
};

enum TSPLIB_PROBLEM_TYPES_enum
{
    TSPLIB_PROBLEM_TYPES
};

enum TSPLIB_EDGE_WEIGHT_TYPES_enum
{
    TSPLIB_EDGE_WEIGHT_TYPES
};

enum TSPLIB_EDGE_WEIGHT_FORMAT_TYPES_enum
{
    TSPLIB_EDGE_WEIGHT_FORMAT_TYPES
};

enum TSPLIB_EDGE_DATA_FORMAT_TYPES_enum
{
    TSPLIB_EDGE_DATA_FORMAT_TYPES
};

enum TSPLIB_NODE_COORD_TYPE_TYPES_enum
{
    TSPLIB_NODE_COORD_TYPE_TYPES
};

enum TSPLIB_DISPLAY_DATA_TYPE_TYPES_enum
{
    TSPLIB_DISPLAY_DATA_TYPE_TYPES
};


// Strings

#undef ENUM_OR_STRING
#define ENUM_OR_STRING( x ) #x

const char * TSPLIB_STATES_KEYWORDS[] =
{
    TSPLIB_STATES
};

const char * TSPLIB_PROBLEM_TYPES_KEYWORDS[] =
{
    TSPLIB_PROBLEM_TYPES
};


const char * TSPLIB_EDGE_WEIGHT_TYPES_KEYWORDS[] =
{
    TSPLIB_EDGE_WEIGHT_TYPES
};


const char * TSPLIB_EDGE_WEIGHT_FORMAT_TYPES_KEYWORDS[] =
{
    TSPLIB_EDGE_WEIGHT_FORMAT_TYPES
};

const char * TSPLIB_EDGE_DATA_FORMAT_TYPES_KEYWORDS[] =
{
    TSPLIB_EDGE_DATA_FORMAT_TYPES
};

const char * TSPLIB_NODE_COORD_TYPE_TYPES_KEYWORDS[] =
{
    TSPLIB_NODE_COORD_TYPE_TYPES
};

const char * TSPLIB_DISPLAY_DATA_TYPE_TYPES_KEYWORDS[] =
{
    TSPLIB_DISPLAY_DATA_TYPE_TYPES
};


struct TSPLIBDistanceFunctions {

  typedef int64 (*TWOD_distance_function)(const Point, const Point);
  typedef int64 (*THREED_distance_function)(const Point, const Point, const Point);
  
 static constexpr double PI = 3.141594;
  //  Earth radius in km
 static constexpr double RRR = 6378.388;

  TSPLIBDistanceFunctions(const TSPLIB_NODE_COORD_TYPE_TYPES_enum dim  , const TSPLIB_EDGE_WEIGHT_TYPES_enum type) {
    switch (dim) {
      case TWOD_COORDS: {
        switch (type) {
          case EUC_2D: {
            TWOD_dist_fun_ = &TWOD_euc_2d_distance;
            break;
          }

          case GEO:
          case GEOM: {
            TWOD_dist_fun_ = &TSPLIBDistanceFunctions::TWOD_geo_distance;
            break;
          }
          case ATT: {
            TWOD_dist_fun_ = &TSPLIBDistanceFunctions::TWOD_att_distance;
            break;
          }
        }
break;
      }  //  case TWOD_COORDS:

      case THREED_COORDS: {
        break;
      }

      case NO_COORDS: {
        break;
      }

      case TSPLIB_NODE_COORD_TYPE_TYPES_UNDEFINED: {
        break;
      }
    }
  }


  int64 TWOD_distance(const Point x, const Point y) {
    return TWOD_dist_fun_(x,y);
  }






  //  Rounds to the nearest int
static int64 nint(double d)
{
   return std::floor(d+0.5);
}

//  Convert longitude and latitude given in DDD.MM with DDD = degrees and MM = minutes
//  into longitude and latitude given in radians.
static double convert_to_geo(double x) {
  int64 deg =  nint(x);

  return PI * (deg + 5.0 * (x - deg) / 3.0 ) / 180.0;
}


//  2D functions

static int64 TWOD_euc_2d_distance(const Point a, const Point b) {
    double xd = a.x - b.x;
    double yd = a.y - b.y;

    return nint(std::sqrt(xd*xd + yd*yd));;
  
}

static int64 THREED_euc_3d_distance(const Point a, const Point b) {
    double xd = a.x - b.x;
    double yd = a.y - b.y;
    double zd = a.z - b.z;

    return nint(std::sqrt(xd*xd + yd*yd + zd*zd));;

}

static int64 TWOD_max_2d_distance(const Point a, const Point b) {
    double xd = std::abs(a.x - b.x);
    double yd = std::abs(a.y - b.y);

    return std::max(nint(xd), nint(yd));
}

static int64 THREED_max_3d_distance(const Point a, const Point b) {
    double xd = std::abs(a.x - b.x);
    double yd = std::abs(a.y - b.y);
    double zd = std::abs(a.z - b.z);

    return std::max(std::max(nint(xd), nint(yd)), nint(zd));
}

static int64 TWOD_man_2d_distance(const Point a, const Point b) {
    double xd = std::abs(a.x - b.x);
    double yd = std::abs(a.y - b.y);

    return nint(xd + yd);
}


static int64 THREED_man_3d_distance(const Point a, const Point b) {
    double xd = std::abs(a.x - b.x);
    double yd = std::abs(a.y - b.y);
    double zd = std::abs(a.z - b.z);

    return nint(xd + yd +zd);
}

static int64 TWOD_ceil_2d_distance(const Point a, const Point b) {
    double xd = std::abs(a.x - b.x);
    double yd = std::abs(a.y - b.y);

    return std::ceil(xd + yd);
}

static int64 THREED_ceil_3d_distance(const Point a, const Point b) {
    double xd = std::abs(a.x - b.x);
    double yd = std::abs(a.y - b.y);
    double zd = std::abs(a.z - b.z);

    return std::ceil(xd + yd +zd);
}

static  int64 TWOD_geo_distance(const Point a, const Point b) {
  Point a_geo(convert_to_geo(a.x), convert_to_geo(a.y) );
  Point b_geo(convert_to_geo(b.x), convert_to_geo(b.y) );

  double q1 = std::cos(a_geo.y - b_geo.y);
  double q2 = std::cos(a_geo.x - b_geo.x);
  double q3 = std::cos(a_geo.x + b_geo.x);
    return (int64) RRR * std::acos( 0.5 * ((1.0 + q1)*q2 - (1.0 - q1) * q3)) + 1.0;
  }

static int64 TWOD_att_distance(const Point a, const Point b) {
  double xd = a.x - b.x;
  double yd = a.y - b.y;

  double rij = std::sqrt( (xd*xd + yd*yd) / 10.0);
  int64 tij = nint(rij);

  return (tij < rij)? tij + 1: tij;
}
TWOD_distance_function TWOD_dist_fun_;

  //  3D functions
THREED_distance_function THREED_dist_fun_;

};

void PrintFatalLog(const char * msg,const  std::string & wrong_keyword, int line_number) {
   LOG(FATAL) << "TSPLIB: " << msg << ": \"" << wrong_keyword << "\" on line " << line_number;
}


//  Find the enum corresponding to a string
//  This only works is the strings and enums are ordered in the same way
//  and an "undefined enum" is placed at the end of the enum (hence the "index + 1").
template <typename E>
E FindEnumKeyword(const char** list,const std::string word,const  E end_index) {
  int index = 0;
  for (; index < end_index; ++index) {
    if (!strcmp(word.c_str(),list[index])) {
      return (E) index;
    }
  }
  return (E) (index + 1);
}

//  Find the enum corresponding to a string
//  This only works is the strings and enums are ordered in the same way
//  and an "undefined enum" is placed at the end of the enum (hence the "index + 1")
//  and a "count enum" gives the number of elements in the enum (XXX_UNDEFINED = XXX_COUNT + 1).
//  Print a LOG(FATAL) if no enum if found.
template <typename E>
E FindOrDieEnumKeyword(const char** list, const std::string word, const E end_index, const char * err_msg, int line_number) {
  E enum_element = FindEnumKeyword<E>(list, word, end_index);
  if (enum_element == end_index + 1) {
    PrintFatalLog(err_msg, word, line_number);
  }
  return enum_element;
}


}  //  namespace operations_research

#endif