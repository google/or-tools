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
//  TSPLIBReader.
//
//  Only valid for:
//  - TSP
//  - ATSP
//  - CVRP
//  - CCPP (this is an extension)

#ifndef OR_TOOLS_TUTORIALS_CPLUSPLUS_TSPLIB_READER_H
#define OR_TOOLS_TUTORIALS_CPLUSPLUS_TSPLIB_READER_H

#include <cmath>
#include <limits>
#include <vector>

#include "base/integral_types.h"
#include "base/filelinereader.h"
#include "base/split.h"
#include "base/strtoint.h"

#include "routing_common/routing_common.h"
#include "routing_common/routing_data.h"
#include "routing_common/tsplib.h"

namespace operations_research {
  class TSPLIBReader : public RoutingData {
  public:
    typedef std::vector<RoutingModel::NodeIndex>::iterator solution_iterator;
    typedef std::vector<RoutingModel::NodeIndex>::const_iterator const_solution_iterator;
    explicit TSPLIBReader(const std::string & filename) :
      RoutingData(0),
      line_number_(0),
      visualizable_(false),
      two_dimension_(false),
      symmetric_(false),
      need_to_compute_distances_(false),
      tsplib_state_unknown_(true),
      tsplib_state_(TSPLIB_STATES_UNDEFINED),
      name_(""),
      type_(TSPLIB_PROBLEM_TYPES_UNDEFINED),
      comment_(""),
      capacity_(-1),
      edge_weight_type_(TSPLIB_EDGE_WEIGHT_TYPES_UNDEFINED),
      edge_weight_format_type_(TSPLIB_EDGE_WEIGHT_FORMAT_TYPES_UNDEFINED),
      edge_data_format_type_(TSPLIB_EDGE_DATA_FORMAT_TYPES_UNDEFINED),
      node_coord_type_(TWOD_COORDS), //  If no coord type is given, we assume 2D.
      display_data_type_(TSPLIB_DISPLAY_DATA_TYPE_TYPES_UNDEFINED)

    {
      LoadInstance(filename);
      if (depots_.size() == 0) {
        depots_.push_back(RoutingModel::kFirstNode);
      }
      SetRoutingDataInstanciated();
    }
    TSPLIB_PROBLEM_TYPES_enum TSPLIBType () const {
      return type_;
    }
    RoutingModel::NodeIndex Depot() const {
      return depots_[0];
    }

std::vector<RoutingModel::NodeIndex> Depots() const {
  return depots_;
}

int32 Capacity() const {
  return capacity_;
}

int64 Demand(RoutingModel::NodeIndex i) const {
  return demands_[i.value()];
}
    bool HasDimensionTwo() const {
      return two_dimension_;
    }
    TSPLIB_NODE_COORD_TYPE_TYPES_enum NodeCoordinateType() const {
      return node_coord_type_;
    }

    TSPLIB_DISPLAY_DATA_TYPE_TYPES_enum DisplayDataType() const {
      return display_data_type_;
    }

    TSPLIB_EDGE_WEIGHT_TYPES_enum EdgeWeightType() const {
      return edge_weight_type_;
    }

    TSPLIB_EDGE_WEIGHT_FORMAT_TYPES_enum EdgeWeightTypeFormat() const {
      return edge_weight_format_type_;
    }

    solution_iterator solution_begin() {
      return tsp_sol_.begin();
    }
    const_solution_iterator solution_begin() const {
      return tsp_sol_.begin();
    }
    solution_iterator solution_end() {
      return tsp_sol_.end();
    }
    const_solution_iterator solution_end() const {
      return tsp_sol_.end();
    }
  protected:
    void LoadInstance(const std::string& filename);
  //  Helper function
  int64& SetMatrix(int i, int j) {
    return distances_.Cost(RoutingModel::NodeIndex(i), RoutingModel::NodeIndex(j));
  }
    
  private:
    void ProcessNewLine(char* const line);

  std::vector<RoutingModel::NodeIndex> depots_;
    int line_number_;
  bool visualizable_;
  bool two_dimension_;
  bool symmetric_;
  bool need_to_compute_distances_;

  TSPLIB_STATES_enum tsplib_state_;
  bool tsplib_state_unknown_;

  TSPLIB_PROBLEM_TYPES_enum type_;
  std::string name_;
  std::string comment_;
  int32 capacity_;
  TSPLIB_EDGE_WEIGHT_TYPES_enum edge_weight_type_;
  TSPLIB_EDGE_WEIGHT_FORMAT_TYPES_enum edge_weight_format_type_;
  TSPLIB_EDGE_DATA_FORMAT_TYPES_enum edge_data_format_type_;
  TSPLIB_NODE_COORD_TYPE_TYPES_enum node_coord_type_;
  TSPLIB_DISPLAY_DATA_TYPE_TYPES_enum display_data_type_;

  TWOD_distance_function TWOD_dist_fun_;
  THREED_distance_function THREED_dist_fun_;

  std::vector<RoutingModel::NodeIndex> tsp_sol_;
  std::vector<int64> demands_;
  };

  void TSPLIBReader::LoadInstance(const std::string& filename)
  {

    FileLineReader reader(filename.c_str());
    reader.set_line_callback(NewPermanentCallback(
      this,
      &TSPLIBReader::ProcessNewLine));
    reader.Reload();
    if (!reader.loaded_successfully()) {
      LOG(FATAL) << "Could not open TSPLIB instance file: " << filename;
    }
  }


  void TSPLIBReader::ProcessNewLine(char*const line) {
     ++line_number_;
  VLOG(2) << "Line " << line_number_ << ": " << line;
  //  Must always be -1 outside a section
  static int32 nodes_nbr = -1;
  static bool read_matrix_done = false;

  static const char kWordDelimiters[] = " :";
  std::vector<std::string> words;
  words = strings::Split(line, kWordDelimiters, strings::SkipEmpty());

  //  Empty lines
  if (words.size() == 0) {
    return;
  }

  //  FIND TSPLIB KEYWORD
  if (tsplib_state_unknown_) {
    bool keyword_found = false;
    //read_matrix_done = false;

    tsplib_state_ = FindEnumKeyword(TSPLIB_STATES_KEYWORDS, words[0], TSPLIB_STATES_COUNT);
    if (tsplib_state_ != TSPLIB_STATES_UNDEFINED) {
      keyword_found = true;
    }

    //  separate test because "EOF" is sometimes redefined
    if (words[0] == kTSPLIBEndFileDelimiter) {
      return;
    }

    if (!keyword_found) {
      PrintFatalLog("Unknown keyword", words[0], line_number_);
    }

    tsplib_state_unknown_ = false;
  }

  //  SWITCH FOLLOWING TSPLIB KEYWORD
  switch (tsplib_state_) {
    case NAME: {
      name_ = words[1];
      tsplib_state_unknown_ = true;
      break;
    }
    case TYPE: {
      type_ = FindOrDieEnumKeyword(TSPLIB_PROBLEM_TYPES_KEYWORDS, words[1], TSPLIB_PROBLEM_TYPES_COUNT, "Unknown problem type", line_number_);
      tsplib_state_unknown_ = true;
      break;
    }
    case COMMENT: {
      if (words.size() > 1) {
        for (int index = 1; index < words.size(); ++index) {
          comment_ = StrCat(comment_, words[index] + " ");
        }
      }
      tsplib_state_unknown_ = true;
      break;
    }
    case DIMENSION: {
      int32 size = atoi32(words[1]);
      CreateRoutingData(size);
      tsplib_state_unknown_ = true;
      break;
    }
    case CAPACITY: {
      capacity_ = atoi32(words[1]);
      tsplib_state_unknown_ = true;
      break;
    }
    case DEPOT_SECTION: {
      if (nodes_nbr == -1) {
        // titel
        ++nodes_nbr;
        break;
      }
      if (atoi32(words[0]) == -1) {
        nodes_nbr = -1;
        tsplib_state_unknown_ = true;
        break;
      }
      depots_.push_back(RoutingModel::NodeIndex(atoi32(words[1]) - 1));
      ++nodes_nbr;
      break;
    }
    case DEMAND_SECTION: {
      if (nodes_nbr == -1) {
        // titel
        demands_.resize(Size());
        ++nodes_nbr;
        break;
      }
      
      if (nodes_nbr == Size() - 1) {
        tsplib_state_unknown_ = true;
        nodes_nbr = -1;
        break;
      }
      CHECK_EQ(words.size(), 2) << "Demand section should only contain node_id and demand on line " << line_number_;
      CHECK_LE(atoi32(words[0]), Size()) << "Node with node_id bigger than size of instance on line " << line_number_;
      demands_[atoi32(words[0]) - 1] = atoi32(words[1]);
      ++nodes_nbr;
      break;
    }
     case TOUR_SECTION: {
      if (nodes_nbr == -1) {
        // titel
        tsp_sol_.resize(Size());
        ++nodes_nbr;
        break;
      }
      if (nodes_nbr == Size()) {
        CHECK_EQ(atoi32(words[0]), -1) << "Tour is supposed to end with -1.";
        tsplib_state_unknown_ = true;
        break;
      }
      RoutingModel::NodeIndex node(atoi32(words[0]) - 1);
      tsp_sol_[nodes_nbr] = node;
      ++nodes_nbr;
      break;
    }
    case EDGE_WEIGHT_TYPE: {
      edge_weight_type_ = FindOrDieEnumKeyword(TSPLIB_EDGE_WEIGHT_TYPES_KEYWORDS,
                                               words[1],
                                               TSPLIB_EDGE_WEIGHT_TYPES_COUNT,
                                               "Unknown edge weight type",
                                               line_number_);
      //  Do we need to compute the distances?
      switch (edge_weight_type_) {
        case EXPLICIT: {
          need_to_compute_distances_ = false;
          break;
        }
        case EUC_2D: {
          need_to_compute_distances_ = true;
          two_dimension_ = true;
          symmetric_ = true;
          visualizable_ = true;
          TWOD_dist_fun_ = &TSPLIBDistanceFunctions::TWOD_euc_2d_distance;
          break;
        }
        case EUC_3D: {
          need_to_compute_distances_ = true;
          two_dimension_ = false;
          symmetric_ = true;
          visualizable_ = true;
          THREED_dist_fun_ = &TSPLIBDistanceFunctions::THREED_euc_3d_distance;
          break;
        }
        case MAX_2D: {
          need_to_compute_distances_ = true;
          two_dimension_ = true;
          symmetric_ = true;
          visualizable_ = true;
          TWOD_dist_fun_ = &TSPLIBDistanceFunctions::TWOD_max_2d_distance;
          break;
        }
        case MAX_3D: {
          need_to_compute_distances_ = true;
          two_dimension_ = false;
          symmetric_ = true;
          visualizable_ = true;
          THREED_dist_fun_ = &TSPLIBDistanceFunctions::THREED_max_3d_distance;
          break;
        }
        case MAN_2D: {
          need_to_compute_distances_ = true;
          two_dimension_ = true;
          symmetric_ = true;
          visualizable_ = true;
          TWOD_dist_fun_ = &TSPLIBDistanceFunctions::TWOD_man_2d_distance;
          break;
        }
        case MAN_3D: {
          need_to_compute_distances_ = true;
          two_dimension_ = false;
          symmetric_ = true;
          visualizable_ = true;
          THREED_dist_fun_ = &TSPLIBDistanceFunctions::THREED_man_3d_distance;
          break;
        }
        case CEIL_2D: {
          need_to_compute_distances_ = true;
          two_dimension_ = true;
          symmetric_ = true;
          visualizable_ = true;
          TWOD_dist_fun_ = &TSPLIBDistanceFunctions::TWOD_ceil_2d_distance;
          break;
        }
        case CEIL_3D: {
          need_to_compute_distances_ = true;
          two_dimension_ = false;
          symmetric_ = true;
          visualizable_ = true;
          THREED_dist_fun_ = &TSPLIBDistanceFunctions::THREED_ceil_3d_distance;
          break;
        }
        case GEO:
        case GEOM: {
          need_to_compute_distances_ = true;
          two_dimension_ = true;
          symmetric_ = true;
          visualizable_ = true;
          TWOD_dist_fun_ = &TSPLIBDistanceFunctions::TWOD_geo_distance;
          break;
        }
        case ATT: {
          need_to_compute_distances_ = true;
          two_dimension_ = true;
          symmetric_ = true;
          visualizable_ = true;
          TWOD_dist_fun_ = &TSPLIBDistanceFunctions::TWOD_att_distance;
          break;
        }
      } //  switch (edge_weight_type_)
      tsplib_state_unknown_ = true;
      break;
    }
    case EDGE_WEIGHT_FORMAT: {
      edge_weight_format_type_ = FindOrDieEnumKeyword(TSPLIB_EDGE_WEIGHT_FORMAT_TYPES_KEYWORDS,
                                                      words[1],
                                                      TSPLIB_EDGE_WEIGHT_FORMAT_TYPES_COUNT,
                                                      "Unknown edge weight format type",
                                                      line_number_);
      tsplib_state_unknown_ = true;
      break;
    }
    case EDGE_DATA_FORMAT: {
      edge_data_format_type_ = FindOrDieEnumKeyword(TSPLIB_EDGE_DATA_FORMAT_TYPES_KEYWORDS,
                                                    words[1],
                                                    TSPLIB_EDGE_DATA_FORMAT_TYPES_COUNT,
                                                    "Unknown edge data format type",
                                                    line_number_);
      tsplib_state_unknown_ = true;
      break;
    }
    case NODE_COORD_TYPE: {
      node_coord_type_ = FindOrDieEnumKeyword(TSPLIB_NODE_COORD_TYPE_TYPES_KEYWORDS,
                                              words[1],
                                              TSPLIB_NODE_COORD_TYPE_TYPES_COUNT,
                                              "Unknown node coord format type",
                                              line_number_);
      tsplib_state_unknown_ = true;
      break;
    }
    case DISPLAY_DATA_TYPE: {
      display_data_type_ = FindOrDieEnumKeyword(TSPLIB_DISPLAY_DATA_TYPE_TYPES_KEYWORDS,
                                                words[1],
                                                TSPLIB_DISPLAY_DATA_TYPE_TYPES_COUNT,
                                                "Unknown display data format type",
                                                line_number_);
      switch (display_data_type_) {
        case NO_DISPLAY:
          break;
        case COORD_DISPLAY:
        case TWOD_DISPLAY:
          visualizable_ = true;
          break;
      }
      tsplib_state_unknown_ = true;
      break;
    }
    case NODE_COORD_SECTION: {
      if (nodes_nbr == -1) {
        ++nodes_nbr;
        visualizable_ = true;
        break;
      }
      ++nodes_nbr;
      switch (node_coord_type_) {
        case TWOD_COORDS: {
          CHECK_EQ(words.size(), 3) << "Node coord data not conform on line " << line_number_;
          CHECK_LE(atoi32(words[0].c_str()), size_) << "Unknown node number " << atoi32(words[0].c_str()) << " on  line " << line_number_;
          coordinates_[atoi32(words[0].c_str()) -1] = Point(atof(words[1].c_str()), atof(words[2].c_str()));
          break;
        }
        case THREED_COORDS: {
          CHECK_EQ(words.size(), 4) << "Node coord data not conform on line " << line_number_;
          CHECK_LE(atoi32(words[0].c_str()), size_) << "Unknown node number " << atoi32(words[0].c_str()) << " on line " << line_number_;
          coordinates_[atoi32(words[0].c_str()) -1] = Point(atof(words[1].c_str()), atof(words[2].c_str()), atof(words[3].c_str()));
          break;
        }
        case NO_COORDS: {
          LOG(FATAL) << "Coordinate is non existent but there is a node coordinate section???";
          break;
        }
        default:
          LOG(FATAL) << "Coordinate type is not defined.";
      }
      if (nodes_nbr == size_) {
        SetHasCoordinates();
        //  Compute distance if needed
        TSPLIBDistanceFunctions tsplib_dist_function(node_coord_type_, edge_weight_type_);
        int64 dist = 0;
        if (need_to_compute_distances_) {
          LG << "Computing distance matrix...";
          // TWO DIMENSION
          if (two_dimension_) {
            //  SYMMETRIC
            if (symmetric_) {
              for (int i = 0; i < size_; ++i) {
                for (int j = i + 1; j < size_; ++j ) {
                  dist = tsplib_dist_function.TWOD_distance(coordinates_[i], coordinates_[j]);
                  SetMatrix(i,j) = dist;
                  SetMatrix(j,i) = dist;
                }
              }
              for (int i = 0; i < size_; ++i) {
                SetMatrix(i,i) = 0LL;
              }
              //  NOT SYMMETRIC
            } else {
              for (int i = 0; i < size_; ++i) {
                for (int j = 0; j < size_; ++j ) {
                  if (i == j) {
                    SetMatrix(i,j) = 0LL;
                  } else {
                    SetMatrix(i,j) = dist;
                  }
                }
              }
            }
            // THREE DIMENSION
          } else {

          }
          LG << "Computing distance matrix... Done!";
        }  // if (tsplib_dist_function.NeedToComputeDistances())
        tsplib_state_unknown_ = true;
        nodes_nbr = -1;
      }
      break;
    }  //  case NODE_COORD_SECTION:
    case DISPLAY_DATA_SECTION: {
      if (nodes_nbr == -1) {
        ++nodes_nbr;
        break;
      }
      if (display_data_type_ == TWOD_DISPLAY) {
        CHECK_EQ(words.size(), 3) << "Display data not conform on line " << line_number_;
        CHECK_LE(atoi32(words[0].c_str()), size_) << "Unknown node number " << atoi32(words[0].c_str()) << " on line " << line_number_;
        display_coords_[atoi32(words[0].c_str()) -1] = Point(atof(words[1].c_str()), atof(words[2].c_str()));
        ++nodes_nbr;
        if (nodes_nbr == size_) {
          SetHasDisplayCoordinates();
          tsplib_state_unknown_ = true;
          nodes_nbr = -1;
        }
      } else {
        tsplib_state_unknown_ = true;
        nodes_nbr = -1;
      }
      break;
    }
    case EDGE_DATA_SECTION: {
      if (words.size() == 1 && words[0] == "-1") {
        //  complete matrix
        //TO DO
        read_matrix_done = true;
        tsplib_state_unknown_ = true;
        break;
      }
      switch(edge_data_format_type_) {
        case EDGE_LIST: {
          CHECK_EQ(words.size(), 2) << "Edge not well defined on line " << line_number_;
          break;
        }
        case ADJ_LIST: {
          break;
        }
      }
    }
    case EDGE_WEIGHT_SECTION: {
      if (nodes_nbr == -1) {
        ++nodes_nbr;
        read_matrix_done = false;
        break;
      }
      switch (edge_weight_format_type_) {
        case FULL_MATRIX: {
          CHECK_EQ(words.size(),size_) << "Matrix not full on line " << line_number_;
          for (int index = 0; index < size_; ++index) {
            int64 dist = atoi64(words[index].c_str());
            SetMatrix(nodes_nbr, index) = dist;
          }
          if (nodes_nbr == size_ - 1) {
            read_matrix_done = true;
          }
          break;
        }
        case UPPER_ROW: {
          CHECK_EQ(words.size(), size_ - nodes_nbr - 1) << " Wrong number of tokens on line " << line_number_;
          SetMatrix(nodes_nbr, nodes_nbr) = 0LL;
          for (int index = 0; index < size_ - nodes_nbr - 1; ++index) {
            int64 dist = atoi64(words[index].c_str());
            SetMatrix(nodes_nbr, index + nodes_nbr + 1) = dist;
            SetMatrix(index + nodes_nbr + 1, nodes_nbr) = dist;
          }
          if (nodes_nbr == size_ - 2) {
            read_matrix_done = true;
          }
          break;
        }
        case UPPER_DIAG_ROW: {  // BUGGY?
          CHECK_EQ(words.size(), size_ - nodes_nbr - 1);
          for (int index = 0; index < size_ - nodes_nbr ; ++index) {
            int64 dist = atoi64(words[index].c_str());
            std::cout << dist << " ";
            SetMatrix(nodes_nbr, index + nodes_nbr) = dist;
            SetMatrix(index + nodes_nbr , nodes_nbr) = dist;
          }
          std::cout << std::endl;
          if (nodes_nbr == size_ - 2) {
            read_matrix_done = true;
          }
          break;
        }
        case LOWER_ROW: { //  TO BE CHECKED
          CHECK_EQ(words.size(), nodes_nbr + 1);
          SetMatrix(nodes_nbr, nodes_nbr) = 0LL;
          for (int index = 0; index < nodes_nbr + 1; ++index) {
            int64 dist = atoi64(words[index].c_str());
            std::cout << dist << " ";
            SetMatrix(nodes_nbr, index) = dist;
            SetMatrix(index , nodes_nbr) = dist;
          }
          std::cout << std::endl;
          if (nodes_nbr == size_ - 2) {
            read_matrix_done = true;
            break;
          }
          break;
        }
      }  //  switch (edge_weight_format_type_)

      if (read_matrix_done) {
        tsplib_state_unknown_ = true;
        nodes_nbr = -1;
        break;
      }
      ++nodes_nbr;
    }  //  case EDGE_WEIGHT_SECTION:
  } //  switch 
  } // ProcessNewLine()

} // namespace operations_research


#endif // OR_TOOLS_TUTORIALS_CPLUSPLUS_TSPLIB_READER_H
