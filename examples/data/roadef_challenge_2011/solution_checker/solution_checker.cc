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

#include "solution_checker.h"

#include <algorithm>

namespace roadef_challenge {

const int kMaxNumMachines = 5000;
const int kMaxNumResources = 20;
const int kMaxNumProcesses = 50000;
const int kMaxNumServices = 50000;
const int kMaxNumNeighborhoods = 1000;
const int kMaxNumDependencies = 5000;
const int kMaxNumLocations = 1000;
const int kMaxNumBalanceCosts = 10;
const int kMaxBooleanValue = 1;
const int kMaxIntValue = kint32max;

// --------------------------------------------------------
// RemainingCapacities
// --------------------------------------------------------
RemainingCapacities::RemainingCapacities(const Capacities& initial_capacities,
                                         const Resources& resources)
    : remaining_capacities_(),
      transient_remaining_capacities_(),
      resources_(resources) {
  CHECK_EQ(initial_capacities.size(), resources_.size());
  InitRemainingCapacities(initial_capacities);
}

RemainingCapacities::~RemainingCapacities() {}

void RemainingCapacities::InitRemainingCapacities(
    const Capacities& initial_capacities) {
  remaining_capacities_ = initial_capacities;
  transient_remaining_capacities_ = initial_capacities;
}

void RemainingCapacities::Consume(const Requirements& requirements) {
  const ResourceIndex num_resources = GetNumberOfResources();
  CHECK_EQ(num_resources, ResourceIndex(requirements.size()));
  for (ResourceIndex resource_id(0); resource_id < num_resources;
       ++resource_id) {
    const int64_t consumption = requirements.at(resource_id);
    remaining_capacities_.at(resource_id) -= consumption;
    transient_remaining_capacities_.at(resource_id) -= consumption;
  }
}

void RemainingCapacities::UndoConsumption(const Requirements& requirements) {
  const ResourceIndex num_resources = GetNumberOfResources();
  CHECK_EQ(num_resources, ResourceIndex(requirements.size()));
  for (ResourceIndex resource_id(0); resource_id < num_resources;
       ++resource_id) {
    const int64_t consumption = requirements.at(resource_id);
    remaining_capacities_.at(resource_id) += consumption;
    const Resource& resource = resources(resource_id);
    if (!resource.is_transient) {
      transient_remaining_capacities_.at(resource_id) += consumption;
    }
  }
}

int64_t RemainingCapacities::GetMinTransientValue() const {
  return *std::min_element(transient_remaining_capacities_.begin(),
                           transient_remaining_capacities_.end());
}

int64_t RemainingCapacities::GetLoadCost(
    const Capacities& safety_remaining_capacities) const {
  int64_t load_cost = 0;
  const ResourceIndex num_resources = GetNumberOfResources();
  CHECK_EQ(num_resources, ResourceIndex(safety_remaining_capacities.size()));
  for (ResourceIndex resource_id(0); resource_id < num_resources;
       ++resource_id) {
    const int load_cost_weight = resources_.at(resource_id).load_cost_weight;
    const int64_t delta = safety_remaining_capacities.at(resource_id) -
                        remaining_capacities_.at(resource_id);
    load_cost += load_cost_weight * std::max(delta, int64_t{0});
  }
  return load_cost;
}

int64_t RemainingCapacities::GetBalanceCost(
    const BalanceCost& balance_cost) const {
  const int64_t remaining_on_target =
      balance_cost.target *
      remaining_capacities_.at(balance_cost.first_resource_id);
  const int64_t remaining =
      remaining_capacities_.at(balance_cost.second_resource_id);
  return balance_cost.weight *
         std::max(int64_t{0}, remaining_on_target - remaining);
}

// --------------------------------------------------------
// Process
// --------------------------------------------------------
Process::Process(ProcessIndex id, const Requirements& requirements,
                 int move_cost, const Service& service)
    : id_(id),
      requirements_(requirements),
      move_cost_(move_cost),
      service_(service) {}

Process::~Process() {}

// --------------------------------------------------------
// Service
// --------------------------------------------------------
Service::Service(ServiceIndex id, NumberOfLocations spread_min,
                 const Dependencies& dependencies)
    : id_(id),
      spread_min_(spread_min),
      dependencies_(dependencies),
      processes_() {}

Service::~Service() { gtl::STLDeleteElements(&processes_); }

// --------------------------------------------------------
// Machine
// --------------------------------------------------------
Machine::Machine(MachineIndex id, NeighborhoodIndex neighborhood_id,
                 LocationIndex location_id, const Capacities& capacities,
                 const Capacities& safety_capacities,
                 const Resources& resources,
                 const MoveToMachineCosts& move_to_machine_costs)
    : id_(id),
      neighborhood_id_(neighborhood_id),
      location_id_(location_id),
      capacities_(capacities),
      resources_(resources),
      move_to_machine_costs_(move_to_machine_costs),
      safety_remaining_capacities_(capacities),
      remaining_capacities_(capacities, resources) {
  const ResourceIndex num_resources = VectorSize(resources);
  CHECK_EQ(num_resources, capacities_.size());
  CHECK_EQ(num_resources, safety_remaining_capacities_.size());
  for (ResourceIndex resource_id(0); resource_id < num_resources;
       ++resource_id) {
    safety_remaining_capacities_.at(resource_id) -=
        safety_capacities.at(resource_id);
  }
}

Machine::~Machine() {}

void Machine::InitRemainingCapacities() {
  remaining_capacities_.InitRemainingCapacities(capacities_);
}

void Machine::ProcessMoveIn(const Process& process) {
  remaining_capacities_.Consume(process.requirements());
}

void Machine::ProcessMoveOut(const Process& process) {
  remaining_capacities_.UndoConsumption(process.requirements());
}

bool Machine::HasNegativeRemainingCapacity() const {
  return remaining_capacities_.GetNumberOfResources() > 0 &&
         remaining_capacities_.GetMinTransientValue() < 0;
}

int64_t Machine::GetLoadCost() const {
  return remaining_capacities_.GetLoadCost(safety_remaining_capacities_);
}

int64_t Machine::GetBalanceCost(const BalanceCost& balance_cost) const {
  return remaining_capacities_.GetBalanceCost(balance_cost);
}

// --------------------------------------------------------
// SolutionChecker
// --------------------------------------------------------

namespace {
template <class T>
void CheckNotNullInVector(const std::vector<T*>& v) {
  for (int i = 0; i < v.size(); ++i) {
    CHECK_NOTNULL(v.at(i));
  }
}

void CheckAssigments(const ProcessAssignments& assignments,
                     MachineIndex num_machines) {
  const ProcessIndex num_processes = VectorSize(assignments);
  for (ProcessIndex process_id(0); process_id < num_processes; ++process_id) {
    CHECK_LE(0, assignments.at(process_id));
    CHECK_GT(num_machines, assignments.at(process_id));
  }
}

}  // anonymous namespace

SolutionChecker::SolutionChecker(
    const Machines& machines, const Services& services,
    const Processes& processes, const BalanceCosts& balance_costs,
    int process_move_cost_weight, int service_move_cost_weight,
    int machine_move_cost_weight, const ProcessAssignments& initial_assignments,
    const ProcessAssignments& new_assignments)
    : machines_(machines),
      services_(services),
      processes_(processes),
      balance_costs_(balance_costs),
      process_move_cost_weight_(process_move_cost_weight),
      service_move_cost_weight_(service_move_cost_weight),
      machine_move_cost_weight_(machine_move_cost_weight),
      initial_assignments_(initial_assignments),
      new_assignments_(new_assignments) {
  CheckNotNullInVector(machines_);
  CheckNotNullInVector(services_);
  CheckNotNullInVector(processes_);

  const ProcessIndex num_processes = VectorSize(processes);
  const ProcessIndex num_processes_in_initial_assignments =
      VectorSize(initial_assignments);
  const ProcessIndex num_processes_in_new_assignments =
      VectorSize(new_assignments);
  CHECK_EQ(num_processes, num_processes_in_initial_assignments);
  CHECK_EQ(num_processes, num_processes_in_new_assignments);

  const MachineIndex num_machines = VectorSize(machines);
  CheckAssigments(initial_assignments, num_machines);
  CheckAssigments(new_assignments, num_machines);

  ComputeRemainingCapacities();
}

SolutionChecker::~SolutionChecker() {}

bool SolutionChecker::Check() const {
  return CheckRemainingCapacities() && CheckConflictConstraints() &&
         CheckSpreadConstraints() && CheckDependencyConstraints();
}

int64_t SolutionChecker::GetObjectiveCost() const {
  const int64_t load_cost = GetLoadCost();
  const int64_t balance_cost = GetBalanceCost();
  const int64_t process_move_cost = GetProcessMoveCost();
  const int64_t service_move_cost = GetServiceMoveCost();
  const int64_t machine_move_cost = GetMachineMoveCost();
  const int64_t total_cost = load_cost + balance_cost + process_move_cost +
                           service_move_cost + machine_move_cost;
  return total_cost;
}

bool SolutionChecker::HasProcessMoved(const Process& process) const {
  const ProcessIndex process_id = process.id();
  const MachineIndex initial_machine_id = initial_assignments_.at(process_id);
  const MachineIndex new_machine_id = new_assignments_.at(process_id);
  return initial_machine_id != new_machine_id;
}

void SolutionChecker::ComputeRemainingCapacities() {
  // Initialize remaining capacities.
  const MachineIndex num_machines = GetNumberOfMachines();
  for (MachineIndex machine_id(0); machine_id < num_machines; ++machine_id) {
    Machine* const machine = machines_.at(machine_id);
    machine->InitRemainingCapacities();
  }

  // Update consumptions at initial state.
  const ProcessIndex num_processes = GetNumberOfProcesses();
  for (ProcessIndex process_id(0); process_id < num_processes; ++process_id) {
    const Process& process = processes(process_id);
    const MachineIndex machine_id = initial_assignments_.at(process_id);
    Machine* const machine = machines_.at(machine_id);
    machine->ProcessMoveIn(process);
  }

  // Compute remaining capacities in the new state.
  for (ProcessIndex process_id(0); process_id < num_processes; ++process_id) {
    const Process& process = processes(process_id);
    if (HasProcessMoved(process)) {
      const MachineIndex initial_machine_id =
          initial_assignments_.at(process_id);
      const MachineIndex new_machine_id = new_assignments_.at(process_id);
      Machine* const initial_machine = machines_.at(initial_machine_id);
      Machine* const new_machine = machines_.at(new_machine_id);
      initial_machine->ProcessMoveOut(process);
      new_machine->ProcessMoveIn(process);
    }
  }
}

bool SolutionChecker::CheckRemainingCapacities() const {
  const MachineIndex num_machines = GetNumberOfMachines();
  for (MachineIndex machine_id(0); machine_id < num_machines; ++machine_id) {
    const Machine& machine = machines(machine_id);
    if (machine.HasNegativeRemainingCapacity()) {
      LOG(INFO) << "Machine " << machine_id
                << " has a negative remaining capacity." << std::endl;
      return false;
    }
  }
  return true;
}

bool SolutionChecker::CheckConflictConstraints() const {
  const ServiceIndex num_services = GetNumberOfServices();
  for (ServiceIndex service_id(0); service_id < num_services; ++service_id) {
    const Service& service = services(service_id);
    std::vector<bool> is_machine_used(machines_.size(), false);
    LocalProcessIndex num_local_processes = service.GetNumberOfProcesses();
    for (LocalProcessIndex local_process_id(0);
         local_process_id < num_local_processes; ++local_process_id) {
      const Process& process = service.processes(local_process_id);
      const ProcessIndex process_id = process.id();
      const MachineIndex machine_id = new_assignments_.at(process_id);
      if (is_machine_used.at(machine_id)) {
        LOG(INFO) << "Service " << service_id
                  << " has two processes running on "
                  << "the same machine " << machine_id << "." << std::endl;
        return false;
      }
      is_machine_used.at(machine_id) = true;
    }
  }
  return true;
}

bool SolutionChecker::CheckSpreadConstraints() const {
  const ServiceIndex num_services = GetNumberOfServices();
  for (ServiceIndex service_id(0); service_id < num_services; ++service_id) {
    const Service& service = services(service_id);
    std::vector<bool> is_location_used(machines_.size(), false);
    NumberOfLocations spread(0);
    LocalProcessIndex num_local_processes = service.GetNumberOfProcesses();
    for (LocalProcessIndex local_process_id(0);
         local_process_id < num_local_processes; ++local_process_id) {
      const Process& process = service.processes(local_process_id);
      const ProcessIndex process_id = process.id();
      const MachineIndex machine_id = new_assignments_.at(process_id);
      const Machine& machine = machines(machine_id);
      const LocationIndex location_id = machine.location_id();
      if (!is_location_used.at(location_id)) {
        ++spread;
        is_location_used.at(location_id) = true;
      }
    }

    const NumberOfLocations spread_min = service.spread_min();
    if (spread < spread_min) {
      LOG(INFO) << "Service " << service_id << " runs in " << spread
                << " different locations. It should run in at least "
                << spread_min << " different locations." << std::endl;
      return false;
    }
  }
  return true;
}

bool SolutionChecker::CheckDependencyConstraint(
    const Service& dependent_service, const Service& service) const {
  // Mark all neighborhood where a process of service runs.
  std::vector<bool> used_neighborhoods(machines_.size(), false);
  LocalProcessIndex num_local_processes = service.GetNumberOfProcesses();
  for (LocalProcessIndex local_process_id(0);
       local_process_id < num_local_processes; ++local_process_id) {
    const Process& process = service.processes(local_process_id);
    const ProcessIndex process_id = process.id();
    const MachineIndex machine_id = new_assignments_.at(process_id);
    const Machine& machine = machines(machine_id);
    const NeighborhoodIndex neighborhood_id = machine.neighborhood_id();
    used_neighborhoods.at(neighborhood_id) = true;
  }

  // Check if processes of dependent_service runs on marked machines.
  num_local_processes = dependent_service.GetNumberOfProcesses();
  for (LocalProcessIndex local_process_id(0);
       local_process_id < num_local_processes; ++local_process_id) {
    const Process& process = dependent_service.processes(local_process_id);
    const ProcessIndex process_id = process.id();
    const MachineIndex machine_id = new_assignments_.at(process_id);
    const Machine& machine = machines(machine_id);
    const NeighborhoodIndex neighborhood_id = machine.neighborhood_id();
    if (!used_neighborhoods.at(neighborhood_id)) {
      LOG(INFO) << "Process " << process_id << " of service "
                << dependent_service.id()
                << " should run in the neighborhood of a process of service "
                << service.id() << " runs." << std::endl;
      return false;
    }
  }
  return true;
}

bool SolutionChecker::CheckDependencyConstraints() const {
  const ServiceIndex num_services = GetNumberOfServices();
  for (ServiceIndex dependent_service_id(0);
       dependent_service_id < num_services; ++dependent_service_id) {
    const Service& dependent_service = services(dependent_service_id);
    const DependencyIndex num_dependencies =
        dependent_service.GetNumberOfDependencies();
    for (DependencyIndex dependency_id(0); dependency_id < num_dependencies;
         ++dependency_id) {
      const ServiceIndex service_id =
          dependent_service.dependencies(dependency_id);
      const Service& service = services(service_id);
      if (!CheckDependencyConstraint(dependent_service, service)) {
        return false;
      }
    }
  }
  return true;
}

int64_t SolutionChecker::GetLoadCost() const {
  int64_t cost = 0;
  const MachineIndex num_machines = GetNumberOfMachines();
  for (MachineIndex machine_id(0); machine_id < num_machines; ++machine_id) {
    const Machine& machine = machines(machine_id);
    cost += machine.GetLoadCost();
  }
  return cost;
}

int64_t SolutionChecker::GetBalanceCost() const {
  int64_t cost = 0;
  const MachineIndex num_machines = GetNumberOfMachines();
  const BalanceCostIndex num_balance_costs(balance_costs_.size());
  for (BalanceCostIndex balance_id(0); balance_id < num_balance_costs;
       ++balance_id) {
    const BalanceCost& balance_cost = balance_costs(balance_id);
    for (MachineIndex machine_id(0); machine_id < num_machines; ++machine_id) {
      const Machine& machine = machines(machine_id);
      cost += machine.GetBalanceCost(balance_cost);
    }
  }
  return cost;
}

int64_t SolutionChecker::GetProcessMoveCost() const {
  int64_t cost = 0;
  const ProcessIndex num_processes = GetNumberOfProcesses();
  for (ProcessIndex process_id(0); process_id < num_processes; ++process_id) {
    const Process& process = processes(process_id);
    if (HasProcessMoved(process)) {
      cost += process.move_cost();
    }
  }
  return process_move_cost_weight_ * cost;
}

int64_t SolutionChecker::GetServiceMoveCost() const {
  int max_num_moves = 0;
  const ServiceIndex num_services = GetNumberOfServices();
  for (ServiceIndex service_id(0); service_id < num_services; ++service_id) {
    const Service& service = services(service_id);
    int num_moves = 0;
    LocalProcessIndex num_local_processes = service.GetNumberOfProcesses();
    for (LocalProcessIndex local_process_id(0);
         local_process_id < num_local_processes; ++local_process_id) {
      const Process& process = service.processes(local_process_id);
      if (HasProcessMoved(process)) {
        ++num_moves;
      }
    }
    max_num_moves = std::max(max_num_moves, num_moves);
  }
  return service_move_cost_weight_ * max_num_moves;
}

int64_t SolutionChecker::GetMachineMoveCost() const {
  int64_t cost = 0;
  const ProcessIndex num_processes = GetNumberOfProcesses();
  for (ProcessIndex process_id(0); process_id < num_processes; ++process_id) {
    const MachineIndex initial_machine_id = initial_assignments_.at(process_id);
    const MachineIndex new_machine_id = new_assignments_.at(process_id);
    const Machine& machine = machines(initial_machine_id);
    cost += machine.move_to_machine_costs(new_machine_id);
  }
  return machine_move_cost_weight_ * cost;
}

// --------------------------------------------------------
// DataParser
// --------------------------------------------------------
DataParser::DataParser(const std::vector<int>& raw_model_data,
                       const std::vector<int>& raw_initial_assignments_data,
                       const std::vector<int>& raw_new_assignments_data)
    : raw_model_data_(raw_model_data),
      raw_initial_assignments_data_(raw_initial_assignments_data),
      raw_new_assignments_data_(raw_new_assignments_data),
      raw_data_iterator_(0),
      initial_assignments_(),
      new_assignments_(),
      resources_(),
      machines_(),
      services_(),
      processes_(),
      balance_costs_(),
      process_move_cost_weight_(0),
      service_move_cost_weight_(0),
      machine_move_cost_weight_(0) {
  Parse();
}

DataParser::~DataParser() {
  gtl::STLDeleteElements(&machines_);
  gtl::STLDeleteElements(&services_);
}

int DataParser::GetNextModelValue(int max_value) {
  const int next_value = raw_model_data_.at(raw_data_iterator_);
  if (next_value < 0) {
    LOG(INFO) << "Value at position " << raw_data_iterator_
              << " should be positive; current value is " << next_value << "."
              << std::endl;
  }

  if (next_value > max_value) {
    LOG(INFO) << "Value at position " << raw_data_iterator_
              << " should be smaller than " << max_value
              << " ; current value is " << next_value << "." << std::endl;
  }
  CHECK(next_value >= 0 && next_value <= max_value);

  ++raw_data_iterator_;
  return next_value;
}

template <class T>
void DataParser::GetModelVector(size_t size, int max_value,
                                std::vector<T>* model_vector) {
  CHECK(model_vector != nullptr);
  model_vector->clear();
  for (int i = 0; i < size; ++i) {
    const T new_element(GetNextModelValue(max_value));
    model_vector->push_back(new_element);
  }
}

void DataParser::Parse() {
  ParseModel();
  ParseAssignments(raw_initial_assignments_data_, &initial_assignments_);
  ParseAssignments(raw_new_assignments_data_, &new_assignments_);
}

void DataParser::ParseModel() {
  raw_data_iterator_ = 0;

  ParseResources();
  ParseMachines();
  ParseServices();
  ParseProcesses();
  ParseBalanceCosts();
  ParseWeights();

  CHECK_EQ(raw_data_iterator_, raw_model_data_.size());
}

void DataParser::ParseResources() {
  const int num_resources = GetNextModelValue(kMaxNumResources);
  resources_.clear();
  for (int resource_id = 0; resource_id < num_resources; ++resource_id) {
    const bool is_transient = GetNextModelValue(kMaxBooleanValue);
    const int load_cost_weight = GetNextModelValue(kMaxIntValue);
    resources_.push_back(Resource(is_transient, load_cost_weight));
  }
}

void DataParser::ParseMachines() {
  const int num_machines = GetNextModelValue(kMaxNumMachines);
  const int num_resources = resources_.size();

  machines_.clear();
  for (int machine_id = 0; machine_id < num_machines; ++machine_id) {
    const NeighborhoodIndex neighborhood_id(GetNextModelValue(num_machines));
    const LocationIndex location_id(GetNextModelValue(num_machines));
    Capacities capacities;
    GetModelVector(num_resources, kMaxIntValue, &capacities);

    Capacities safety_capacities;
    GetModelVector(num_resources, kMaxIntValue, &safety_capacities);

    MoveToMachineCosts move_to_machine_costs;
    GetModelVector(num_machines, kMaxIntValue, &move_to_machine_costs);

    Machine* const machine = new Machine(
        MachineIndex(machine_id), neighborhood_id, location_id, capacities,
        safety_capacities, resources_, move_to_machine_costs);
    machines_.push_back(machine);
  }
}

void DataParser::ParseServices() {
  const int num_machines = machines_.size();
  const int num_services = GetNextModelValue(kMaxNumServices);
  services_.clear();
  for (int service_id = 0; service_id < num_services; ++service_id) {
    const NumberOfLocations spread_min(GetNextModelValue(num_machines));
    const int num_dependencies = GetNextModelValue(kMaxNumDependencies);
    Dependencies dependencies;
    GetModelVector(num_dependencies, num_services - 1, &dependencies);
    Service* const service =
        new Service(ServiceIndex(service_id), spread_min, dependencies);
    services_.push_back(service);
  }
}

void DataParser::ParseProcesses() {
  const int num_processes = GetNextModelValue(kMaxNumProcesses);
  const int num_resources = resources_.size();
  const int num_services = services_.size();

  processes_.clear();
  for (int process_id = 0; process_id < num_processes; ++process_id) {
    const ServiceIndex service_id(GetNextModelValue(num_services - 1));
    Service* const service = services_.at(service_id);
    CHECK(service != nullptr);

    Requirements requirements;
    GetModelVector(num_resources, kMaxIntValue, &requirements);
    const int move_cost = GetNextModelValue(kMaxIntValue);
    Process* const process = new Process(ProcessIndex(process_id), requirements,
                                         move_cost, *service);
    processes_.push_back(process);
    service->AddProcess(process);
  }
}

void DataParser::ParseBalanceCosts() {
  const int num_resources = resources_.size();
  const int num_balance_costs = GetNextModelValue(kMaxNumBalanceCosts);
  balance_costs_.clear();
  for (int balance_id = 0; balance_id < num_balance_costs; ++balance_id) {
    const ResourceIndex first_resource_id(GetNextModelValue(num_resources - 1));
    const ResourceIndex second_resource_id(
        GetNextModelValue(num_resources - 1));
    const int target = GetNextModelValue(kMaxIntValue);
    const int weight = GetNextModelValue(kMaxIntValue);
    const BalanceCost balance_cost(first_resource_id, second_resource_id,
                                   target, weight);
    balance_costs_.push_back(balance_cost);
  }
}

void DataParser::ParseWeights() {
  process_move_cost_weight_ = GetNextModelValue(kMaxIntValue);
  service_move_cost_weight_ = GetNextModelValue(kMaxIntValue);
  machine_move_cost_weight_ = GetNextModelValue(kMaxIntValue);
}

void DataParser::ParseAssignments(const std::vector<int>& assignments,
                                  ProcessAssignments* process_assignments) {
  CHECK(process_assignments != nullptr);
  process_assignments->clear();
  const size_t num_machines = machines_.size();
  const size_t num_assignments = assignments.size();
  const size_t num_processes = processes_.size();
  CHECK_EQ(num_processes, num_assignments);
  for (int assignment_id = 0; assignment_id < num_assignments;
       ++assignment_id) {
    const MachineIndex machine_id(assignments.at(assignment_id));
    CHECK(machine_id >= 0 && machine_id < num_machines);
    process_assignments->push_back(machine_id);
  }
}

}  // namespace roadef_challenge
