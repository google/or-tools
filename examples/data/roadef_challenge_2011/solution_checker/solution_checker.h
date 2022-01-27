// Copyright 2010-2021 Google LLC
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

#ifndef SOLUTION_CHECKER_H_
#define SOLUTION_CHECKER_H_

#include <iostream>
#include <vector>

#include "assert.h"

using namespace std;
typedef long long int int64_t;

namespace roadef_challenge {

typedef int MachineIndex;
typedef int ServiceIndex;
typedef int ProcessIndex;
typedef int LocalProcessIndex;
typedef int ResourceIndex;
typedef int NeighborhoodIndex;
typedef int LocationIndex;
typedef int NumberOfLocations;
typedef int BalanceCostIndex;
typedef int DependencyIndex;

typedef std::vector<int64_t> Capacities;
typedef std::vector<int> Requirements;
typedef std::vector<int> ProcessAssignments;
typedef std::vector<int> MoveToMachineCosts;
typedef std::vector<int> Dependencies;

#define CHECK(x) assert(x)
#define CHECK_EQ(x, y) CHECK((x) == (y))
#define CHECK_LE(x, y) CHECK((x) <= (y))
#define CHECK_GT(x, y) CHECK((x) > (y))
#define CHECK_NOTNULL(ptr) CHECK(ptr);
#define LG cout
#define kint32max 0x7fffffff

template <class T>
void STLDeleteElements(std::vector<T*>* elements) {
  CHECK_NOTNULL(elements);
  for (int i = 0; i < elements->size(); ++i) {
    delete elements->at(i);
  }
  elements->clear();
}

template <class T>
size_t VectorSize(const std::vector<T>& v) {
  return v.size();
}

struct Resource {
  Resource(bool _is_transient, int _load_cost_weight)
      : is_transient(_is_transient), load_cost_weight(_load_cost_weight) {}

  bool is_transient;
  int load_cost_weight;
};
typedef std::vector<Resource> Resources;

struct BalanceCost {
  BalanceCost(ResourceIndex _first_resource_id,
              ResourceIndex _second_resource_id, int _target, int _weight)
      : first_resource_id(_first_resource_id),
        second_resource_id(_second_resource_id),
        target(_target),
        weight(_weight) {}

  ResourceIndex first_resource_id;
  ResourceIndex second_resource_id;
  int target;
  int weight;
};
typedef std::vector<BalanceCost> BalanceCosts;

// This class is a collection of remaining capacities per resource.
// It deals with both remaining capacities (used for load and balance costs)
// and transient remaining capacities (used for transient usage hard
// constraints).
class RemainingCapacities {
 public:
  RemainingCapacities(const Capacities& capacities, const Resources& resources);
  virtual ~RemainingCapacities();

  // Initializes the remaining capacities and transient remaining capacities.
  void InitRemainingCapacities(const Capacities& initial_capacities);

  // For all resources, consumes the required capacity.
  // This method is used when a process is moved to a new machine.
  void Consume(const Requirements& requirements);

  // For all resources, undoes consumption of the required capacity.
  // This method is used when a process is moved out of the initial machine.
  // Due to transient usage, capacity may not be released on all resources.
  void UndoConsumption(const Requirements& requirements);

  // Returns the smallest capacity of the transient remaining capacities.
  int64_t GetMinTransientValue() const;

  // Returns the weighted sum of load costs of all resources.
  int64_t GetLoadCost(const Capacities& safety_remaining_capacities) const;

  // Returns the weighted sum of balance costs of all resources.
  int64_t GetBalanceCost(const BalanceCost& balance_cost) const;

  ResourceIndex GetNumberOfResources() const { return VectorSize(resources_); }

  const Resource& resources(ResourceIndex resource_id) const {
    return resources_.at(resource_id);
  }

 private:
  Capacities remaining_capacities_;
  Capacities transient_remaining_capacities_;
  const Resources& resources_;
};

class Service;

// This class contains all needed information about processes to check hard
// constraints and compute objective costs.
class Process {
 public:
  Process(ProcessIndex id, const Requirements& requirements, int move_cost,
          const Service& service);
  virtual ~Process();

  ProcessIndex id() const { return id_; }
  const Requirements& requirements() const { return requirements_; }
  int move_cost() const { return move_cost_; }
  const Service& service() const { return service_; }

 private:
  const ProcessIndex id_;
  const Requirements requirements_;
  const int move_cost_;
  const Service& service_;
};
typedef std::vector<const Process*> Processes;
typedef std::vector<const Process*> LocalProcesses;

// This class contains all needed information about services to check hard
// constraints and compute objective costs.
class Service {
 public:
  Service(ServiceIndex id, NumberOfLocations spread_min,
          const Dependencies& dependencies);
  virtual ~Service();

  void AddProcess(const Process* const process) {
    CHECK(process != nullptr);
    processes_.push_back(process);
  }

  ServiceIndex id() const { return id_; }
  NumberOfLocations spread_min() const { return spread_min_; }

  DependencyIndex GetNumberOfDependencies() const {
    return VectorSize(dependencies_);
  }

  ServiceIndex dependencies(DependencyIndex dependency_id) const {
    return dependencies_.at(dependency_id);
  }

  LocalProcessIndex GetNumberOfProcesses() const {
    return VectorSize(processes_);
  }

  const Process& processes(LocalProcessIndex process_id) const {
    return *(processes_.at(process_id));
  }

 private:
  const ServiceIndex id_;
  const NumberOfLocations spread_min_;
  const Dependencies dependencies_;
  LocalProcesses processes_;
};
typedef std::vector<Service*> Services;

// This class contains all needed information about machines to check hard
// constraints and compute objective costs.
class Machine {
 public:
  Machine(MachineIndex id, NeighborhoodIndex neighborhood_id,
          LocationIndex location_id, const Capacities& capacities,
          const Capacities& safety_capacities, const Resources& resources,
          const MoveToMachineCosts& move_to_machine_costs);
  virtual ~Machine();

  // Initializes remaining capacities.
  void InitRemainingCapacities();

  // Updates remaining capacities when a process moves out of this machine.
  void ProcessMoveIn(const Process& process);

  // Updates remaining capacities when a process moves out of this machine.
  void ProcessMoveOut(const Process& process);

  // Returns true when at least one remaining capacity is negative, i.e.
  // transient usage constraint fails.
  bool HasNegativeRemainingCapacity() const;

  // Returns the weighted load cost of the machine.
  int64_t GetLoadCost() const;

  // Returns the weighted balance cost of the machine.
  int64_t GetBalanceCost(const BalanceCost& balance_cost) const;

  MachineIndex id() const { return id_; }
  LocationIndex location_id() const { return location_id_; }
  NeighborhoodIndex neighborhood_id() const { return neighborhood_id_; }

  MachineIndex GetNumberOfMoveToMachineCosts() const {
    return VectorSize(move_to_machine_costs_);
  }

  int move_to_machine_costs(MachineIndex machine_id) const {
    return move_to_machine_costs_.at(machine_id);
  }

  const Capacities& capacities() const { return capacities_; }

 private:
  const MachineIndex id_;
  const NeighborhoodIndex neighborhood_id_;
  const LocationIndex location_id_;
  const Capacities capacities_;
  const Resources& resources_;
  const MoveToMachineCosts move_to_machine_costs_;

  Capacities safety_remaining_capacities_;
  RemainingCapacities remaining_capacities_;
};
typedef std::vector<Machine*> Machines;

// This class checks all hard constraints and compute the total objective cost.
class SolutionChecker {
 public:
  SolutionChecker(const Machines& machines, const Services& services,
                  const Processes& processes, const BalanceCosts& balance_costs,
                  int process_move_cost_weight, int service_move_cost_weight,
                  int machine_move_cost_weight,
                  const ProcessAssignments& initial_assignments,
                  const ProcessAssignments& new_assignments);
  virtual ~SolutionChecker();

  // Checks hard constraints. Returns true if all constraints are satisfied,
  // false otherwise.
  bool Check() const;

  // Returns the total objective cost as defined in the problem description
  // document. Note this method assumes all hard constraints are satisfied.
  int64_t GetObjectiveCost() const;

 private:
  // Returns true if process doesn't run on the same machine in the
  // initial assignment and in the new assignment.
  bool HasProcessMoved(const Process& process) const;

  MachineIndex GetNumberOfMachines() const { return VectorSize(machines_); }
  const Machine& machines(MachineIndex machine_id) const {
    return *(machines_.at(machine_id));
  }

  ServiceIndex GetNumberOfServices() const { return VectorSize(services_); }
  const Service& services(ServiceIndex service_id) const {
    return *(services_.at(service_id));
  }

  ProcessIndex GetNumberOfProcesses() const { return VectorSize(processes_); }
  const Process& processes(ProcessIndex process_id) const {
    return *(processes_.at(process_id));
  }

  BalanceCostIndex GetNumberOfBalanceCosts() const {
    return VectorSize(balance_costs_);
  }
  const BalanceCost& balance_costs(BalanceCostIndex balance_cost_id) const {
    return balance_costs_.at(balance_cost_id);
  }

  // Computes the remaining capacities (transient or not) for all machines for
  // all resources. This method is called by the constructor. Then remaining
  // capacities can be used to check capacity and transient usage hard
  // constraints and compute load and balance costs.
  void ComputeRemainingCapacities();

  // Returns true if capacity and transient usage hard constraints are
  // satisfied, false otherwise.
  bool CheckRemainingCapacities() const;

  // Returns true if conflict constraints are satisfied, false otherwise.
  bool CheckConflictConstraints() const;

  // Returns true if spread constraints are satisfied, false otherwise.
  bool CheckSpreadConstraints() const;

  // Returns true if dependency constraints are satisfied, false otherwise.
  bool CheckDependencyConstraints() const;

  // Returns true if dependency constraint between dependent_service and
  // service is satisfied. Returns false otherwise.
  bool CheckDependencyConstraint(const Service& dependent_service,
                                 const Service& service) const;

  // Returns the weigthed sum of all load costs.
  int64_t GetLoadCost() const;

  // Returns the weighted sum of all balance costs.
  int64_t GetBalanceCost() const;

  // Returns the weighted sum of all process move costs.
  int64_t GetProcessMoveCost() const;

  // Returns the weighted sum of all service move costs.
  int64_t GetServiceMoveCost() const;

  // Returns the weighted sum of all machine move costs.
  int64_t GetMachineMoveCost() const;

  const Machines& machines_;
  const Services& services_;
  const Processes& processes_;
  const BalanceCosts& balance_costs_;
  const int process_move_cost_weight_;
  const int service_move_cost_weight_;
  const int machine_move_cost_weight_;

  const ProcessAssignments& initial_assignments_;
  const ProcessAssignments& new_assignments_;
};

// This class parses raw data according to data formats defined in the problem
// description document, and creates needed objects for the solution checker.
class DataParser {
 public:
  DataParser(const std::vector<int>& raw_model_data,
             const std::vector<int>& raw_initial_assignments_data,
             const std::vector<int>& raw_new_assignments_data);
  virtual ~DataParser();

  const Machines& machines() const { return machines_; }
  const Services& services() const { return services_; }
  const Processes& processes() const { return processes_; }
  const BalanceCosts& balance_costs() const { return balance_costs_; }
  int process_move_cost_weight() const { return process_move_cost_weight_; }
  int service_move_cost_weight() const { return service_move_cost_weight_; }
  int machine_move_cost_weight() const { return machine_move_cost_weight_; }

  const ProcessAssignments& initial_assignments() const {
    return initial_assignments_;
  }
  const ProcessAssignments& new_assignments() const { return new_assignments_; }

 private:
  int GetNextModelValue(int max_value);

  template <class T>
  void GetModelVector(size_t size, int max_value, std::vector<T>* model_vector);

  void Parse();
  void ParseModel();
  void ParseResources();
  void ParseMachines();
  void ParseServices();
  void ParseProcesses();
  void ParseBalanceCosts();
  void ParseWeights();
  void ParseAssignments(const std::vector<int>& assignment,
                        ProcessAssignments* process_assignment);

  const std::vector<int>& raw_model_data_;
  const std::vector<int>& raw_initial_assignments_data_;
  const std::vector<int>& raw_new_assignments_data_;
  int raw_data_iterator_;

  ProcessAssignments initial_assignments_;
  ProcessAssignments new_assignments_;
  Resources resources_;
  Machines machines_;
  Services services_;
  Processes processes_;
  BalanceCosts balance_costs_;
  int process_move_cost_weight_;
  int service_move_cost_weight_;
  int machine_move_cost_weight_;
};

}  // namespace roadef_challenge

#endif  // SOLUTION_CHECKER_H_
