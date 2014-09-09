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
//  Common base for TSPTW solutions.

#ifndef OR_TOOLS_TUTORIALS_CPLUSPLUS_CVRP_SOLUTION_H
#define OR_TOOLS_TUTORIALS_CPLUSPLUS_CVRP_SOLUTION_H

#include <vector>

#include "constraint_solver/routing.h"
#include "base/split.h"
#include "base/filelinereader.h"
#include "base/join.h"
#include "base/bitmap.h"

#include "routing_common/routing_solution.h"
#include "cvrp_data.h"

DEFINE_bool(numbering_solution_nodes_from_zero, true, "Number the nodes in the solution starting from 0?");

namespace operations_research {

  class CVRPSolution : public RoutingSolution {
  public:
    typedef std::vector<std::vector<RoutingModel::NodeIndex> >::iterator vehicle_iterator;
    typedef std::vector<std::vector<RoutingModel::NodeIndex> >::const_iterator const_vehicle_iterator;
    typedef std::vector<RoutingModel::NodeIndex>::iterator node_iterator;
    typedef std::vector<RoutingModel::NodeIndex>::const_iterator const_node_iterator;
    CVRPSolution(const CVRPData & data, std::string filename): RoutingSolution(data), data_(data), depot_(data.Depot()),
    loaded_solution_obj_(-1) {
        LoadInstance(filename);
    }
    //  We could have used RoutingModel::AssignmentToRoutes()
    CVRPSolution(const CVRPData & data, const RoutingModel * const routing, const Assignment * const sol): RoutingSolution(data), data_(data),
      depot_(data.Depot()), loaded_solution_obj_(-1) {
      CHECK_NOTNULL(routing);
      CHECK_NOTNULL(sol);
      depot_ = routing->IndexToNode(routing->GetDepot());
      for (int32 vehicle = 0; vehicle < routing->vehicles(); ++vehicle) {
        int64 start_node = routing->Start(vehicle);
        //  first node after depot
        int64 node = sol->Value(routing->NextVar(start_node));
        for (; !routing->IsEnd(node);
          node = sol->Value(routing->NextVar(node))) {
          RoutingModel::NodeIndex node_id = routing->IndexToNode(node);
          Add(node_id,vehicle);
        }
      }
    }

    virtual ~CVRPSolution() {}

    RoutingModel::NodeIndex Depot() const {
      return depot_;
    }

    std::string Name() const {
      return name_;
    }

    void SetName(const std::string & name) {
      name_ = name;
    }

    //  We only consider complete solutions.
    virtual void LoadInstance(std::string filename);
    virtual bool IsSolution() const;
    virtual bool IsFeasibleSolution() const;
    virtual int64 ComputeObjectiveValue() const;
    int NumberOfVehicles() const {
      return number_of_vehicles_;
    }
    virtual bool Add(RoutingModel::NodeIndex i, int route_number) {
      if (sol_.size() == route_number) {
        std::vector<RoutingModel::NodeIndex> v;
        sol_.push_back(v);
      }
      sol_[route_number].push_back(i);
      return true;
    }

    void WriteAssignment(const RoutingModel * routing, Assignment * const sol) {
      CHECK_NOTNULL(routing);
      CHECK_NOTNULL(sol);
      routing->RoutesToAssignment(sol_,
                          true,
                          true,
                          sol);
    }

std::vector<std::vector<RoutingModel::NodeIndex> > const & Routes() const {
  return sol_;
}

    //iterators
    vehicle_iterator vehicle_begin() { return sol_.begin(); }
    const_vehicle_iterator vehicle_begin() const { return sol_.begin(); }
    vehicle_iterator vehicle_end() { return sol_.end(); }
    const_vehicle_iterator vehicle_end() const { return sol_.end(); }

    node_iterator node_begin(vehicle_iterator v_iter) {return v_iter->begin();}
    const_node_iterator node_begin(const_vehicle_iterator v_iter) const {return v_iter->begin();}
    node_iterator node_end(vehicle_iterator v_iter) {return v_iter->end();}
    const_node_iterator node_end(const_vehicle_iterator v_iter) const {return v_iter->end();}

    virtual void Print(std::ostream & out) const;
    virtual void Write(const std::string & filename) const ;
  protected:
    std::vector<std::vector<RoutingModel::NodeIndex> > sol_;
    std::vector<int64> capacities_;
  private:
    const CVRPData & data_;
    RoutingModel::NodeIndex depot_;
    int line_number_;
    void ProcessNewLine(char* const line);
    void InitLoadInstance() {
      line_number_ = 0;
      number_of_vehicles_ = 0;
      sol_.clear();
      name_ = "";
      comment_ = "";
    }

    std::string name_;
    std::string comment_;
    int64 loaded_solution_obj_;
    int number_of_vehicles_;
  };

  void CVRPSolution::LoadInstance(std::string filename) {
    InitLoadInstance();
    FileLineReader reader(filename.c_str());
    reader.set_line_callback(NewPermanentCallback(
                             this,
                             &CVRPSolution::ProcessNewLine));
    reader.Reload();
    if (!reader.loaded_successfully()) {
      LOG(FATAL) << "Could not open TSPTW solution file: " << filename;
    }
  }

  //  Test if all nodes are serviced once and only once
  bool CVRPSolution::IsSolution() const {
    // Test if same number of nodes
    if (data_.Size() != Size()) {
      return false;
    }

    // Test if all nodes are used
    Bitmap used(Size());
 
    for (int i = 0; i < sol_.size(); ++i) {
      for (int j = 0; j < sol_[i].size(); ++j) {
        int index = sol_[i][j].value();
        if (used.Get(index)) {
          LG << "already used index = " << index;
          return false;
        } else {
          used.Set(index,true);
        }
      }
    }

    //  Test if depot was not used in the solution
    return !used.Get(depot_.value());
  }

  //  Test if capacities are respected.
  bool CVRPSolution::IsFeasibleSolution() const {
    if (!IsSolution()) {
      return false;
    }

    const int64 vehicle_capacity = data_.Capacity();
    RoutingModel::NodeIndex node;
    int64 route_capacity_left = vehicle_capacity;
    int vehicle_index = 1;
    
    for (const_vehicle_iterator v_iter = vehicle_begin(); v_iter != vehicle_end(); ++v_iter) {
      route_capacity_left = vehicle_capacity;
      VLOG(1) << "Route " << vehicle_index << " with capacity " << route_capacity_left;
      for (const_node_iterator n_iter = node_begin(v_iter); n_iter != node_end(v_iter); ++n_iter ) {
        node = *n_iter;
        
        route_capacity_left -= data_.Demand(node);
        VLOG(1) << "Servicing node " << node.value() + 1 << " with demand " << data_.Demand(node) << " (capacity left: " << route_capacity_left << ")";
        if (route_capacity_left < 0) {
          return false;
        }
      }
      ++vehicle_index;
    }

    return true;
  }



  int64 CVRPSolution::ComputeObjectiveValue() const {
    int64 obj = 0;
    RoutingModel::NodeIndex from_node, to_node;

    for (const_vehicle_iterator v_iter = vehicle_begin(); v_iter != vehicle_end(); ++v_iter) {
      from_node = depot_;
      for (const_node_iterator n_iter = node_begin(v_iter); n_iter != node_end(v_iter); ++n_iter ) {
        to_node = *n_iter;
        obj += data_.Distance(from_node, to_node);
        from_node = to_node;
      }
      //  Last arc
      obj += data_.Distance(to_node, depot_);
    }

    return obj;
  }

  void CVRPSolution::Print(std::ostream& out) const {
    int32 vehicle_index = 0;
    for (const_vehicle_iterator v_iter = vehicle_begin(); v_iter != vehicle_end(); ++v_iter) {
      out << "Route " << StrCat("#", vehicle_index + 1, ":");
      for (const_node_iterator n_iter = node_begin(v_iter); n_iter != node_end(v_iter); ++n_iter ) {
        out << " " << (*n_iter).value() + (FLAGS_numbering_solution_nodes_from_zero? 0 : 1);
      }
      out << std::endl;
      ++vehicle_index;
    }
    out << "cost " << ComputeObjectiveValue() << std::endl;

  }

  void CVRPSolution::Write(const std::string & filename) const {
    WriteToFile<CVRPSolution> writer(this, filename);
    writer.SetMember(&operations_research::CVRPSolution::Print);
    writer.Run();
  }

void CVRPSolution::ProcessNewLine(char*const line) {
  ++line_number_;
  static const char kWordDelimiters[] = " #:";
  std::vector<std::string> words;
  words = strings::Split(line, kWordDelimiters, strings::SkipEmpty());

  if (words[0] == "Route") {
    const int number_of_served_nodes = words.size() - 2;
    CHECK_GE(number_of_served_nodes, 1);
    for (int node = 0; node < number_of_served_nodes; ++node) {
      int32 node_id = atoi32(words[node + 2]) + (FLAGS_numbering_solution_nodes_from_zero? 0 : -1);
      CHECK_LE(node_id, size_) << "Node " << node_id << " is greater than size " << size_ << " of solution.";
      Add(RoutingModel::NodeIndex(node_id ), number_of_vehicles_);
    }
    ++number_of_vehicles_;

    return;
  }

  if (words[0] == "cost") {
    CHECK_EQ(words.size(), 2) << "Only objective value allowed on cost line of CVRP solution file at line " << line_number_;
    loaded_solution_obj_ = atoi64(words[1]);
    return;
  }
  LOG(FATAL) << "Unrecognized line in CVRP solution file at line: " << line_number_;
}  //  void ProcessNewLine(char* const line)


}  //  namespace operations_research

#endif //  OR_TOOLS_TUTORIALS_CPLUSPLUS_CVRP_SOLUTION_H