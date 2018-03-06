// Copyright 2010-2017 Google
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

// Provides an example of the Vehicle Routing Problem with Time Windows and Breaks. All breaks are mandatory
// and must be performed without interruptions. Since breaks without interruptions are not available off-the-shelf
// a custom constraint is developed.
//
// A warning message is printed every time vehicle's break overlaps with a visit.

#include <algorithm>
#include <iterator>
#include <string>
#include <functional>
#include <ostream>
#include <iostream>
#include <vector>

#include "ortools/base/logging.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_flags.h"

DEFINE_bool(built_in_breaks, false, "Use breaks from the routing library that allow for interruptions.");

int64 duration_from_string(const std::string &text) {
    int hours = 0;
    int minutes = 0;
    int seconds = 0;

    std::istringstream input{text};

    input >> hours;
    input.ignore(1, ':');
    input >> minutes;
    input.ignore(1, ':');
    input >> seconds;

    return 3600 * hours + 60 * minutes + seconds;
}

struct Visit {
    Visit(std::size_t location, const std::string &begin, const std::string &end, const std::string &duration)
            : Visit{location,
                    duration_from_string(begin),
                    duration_from_string(end),
                    duration_from_string(duration)} {}

    Visit(std::size_t location, int64 begin, int64 end, int64 duration)
            : Location{location},
              Begin{begin},
              End{end},
              Duration{duration} {}

    const std::size_t Location;
    const int64 Begin;
    const int64 End;
    const int64 Duration;
};

std::ostream &operator<<(std::ostream &out, const Visit &visit) {
    out << visit.Location << " [" << visit.Begin << ", " << visit.End << "] " << visit.Duration;
    return out;
}

struct TimePeriod {
    TimePeriod(const std::string &start, const std::string &duration)
            : TimePeriod{duration_from_string(start),
                    duration_from_string(duration)} {}

    TimePeriod(int64 start, int64 duration)
            : Start{start},
              Duration{duration} {}

    TimePeriod Intersection(const TimePeriod &other) const {
        int64 later_start = std::max(Start, other.Start);
        int64 earlier_finish = std::min(Finish(), other.Finish());

        if (earlier_finish < later_start) {
            return {earlier_finish, 0};
        }

        return {later_start, earlier_finish - later_start};
    }

    bool operator==(const TimePeriod &other) {
        return Start == other.Start && Duration == other.Duration;
    }

    bool operator!=(const TimePeriod &other) {
        return !this->operator==(other);
    }

    int64 Finish() const { return Start + Duration; }

    const int64 Start;
    const int64 Duration;
};

struct Problem {
    static const std::string TIME_DIM;

    Problem(std::vector <Visit> visits,
            std::vector <std::vector<TimePeriod> > breaks,
            std::vector <std::vector<int64> > distances)
            : Depot{0},
              Visits(std::move(visits)),
              Breaks(std::move(breaks)),
              Distances(std::move(distances)) {}

    int64 distance(operations_research::RoutingModel::NodeIndex from_node,
                   operations_research::RoutingModel::NodeIndex to_node) const {
        if (from_node == Depot || to_node == Depot) {
            return 0;
        }

        const auto from = NodeToVisit(from_node).Location;
        const auto to = NodeToVisit(to_node).Location;
        return Distances.at(from).at(to);
    }

    int64 service_plus_distance(operations_research::RoutingModel::NodeIndex from_node,
                                operations_research::RoutingModel::NodeIndex to_node) const {
        if (from_node == Depot) {
            return 0;
        }

        const auto service_time = NodeToVisit(from_node).Duration;
        return service_time + distance(from_node, to_node);
    }

    const Visit &NodeToVisit(operations_research::RoutingModel::NodeIndex node) const {
        return Visits.at(static_cast<std::size_t>(node.value() - 1));
    }

    const operations_research::RoutingModel::NodeIndex Depot;
    const std::vector <Visit> Visits;
    const std::vector <std::vector<TimePeriod> > Breaks;
    const std::vector <std::vector<int64> > Distances;
};

const std::string Problem::TIME_DIM{"time"};

Problem CreateSample();

class BreakConstraint : public operations_research::Constraint {
public:
    BreakConstraint(const operations_research::RoutingDimension *dimension,
                    int vehicle,
                    std::vector<operations_research::IntervalVar *> break_intervals)
            : Constraint(dimension->model()->solver()),
              dimension_(dimension),
              vehicle_(vehicle),
              break_intervals_(std::move(break_intervals)),
              status_(solver()->MakeBoolVar(absl::StrCat("status ", vehicle))) {}

    void Post() override {
        operations_research::RoutingModel *const model = dimension_->model();
        operations_research::Constraint *const path_connected_const
                = solver()->MakePathConnected(model->Nexts(),
                                              {model->Start(vehicle_)},
                                              {model->End(vehicle_)},
                                              {status_});

        solver()->AddConstraint(path_connected_const);
        operations_research::Demon *const demon
                = MakeConstraintDemon0(solver(),
                                       this,
                                       &BreakConstraint::OnPathClosed,
                                       absl::StrCat("Path Closed %1%", vehicle_));
        status_->WhenBound(demon);
    }

    void InitialPropagate() override {
        if (status_->Bound()) {
            OnPathClosed();
        }
    }

private:
    void OnPathClosed() {
        if (status_->Max() == 0) {
            for (operations_research::IntervalVar *const break_interval : break_intervals_) {
                break_interval->SetPerformed(false);
            }
            return;
        }

        operations_research::RoutingModel *const model = dimension_->model();
        std::vector < operations_research::IntervalVar * > all_intervals;
        operations_research::IntervalVar *last_interval = nullptr;

        int64 current_index = model->NextVar(model->Start(vehicle_))->Value();
        while (!model->IsEnd(current_index)) {
            const auto next_index = model->NextVar(current_index)->Value();

            operations_research::IntervalVar *const current_interval = solver()->MakeFixedDurationIntervalVar(
                    dimension_->CumulVar(current_index),
                    dimension_->GetTransitValue(current_index, next_index, vehicle_),
                    absl::StrCat(current_index, "-", next_index));
            all_intervals.push_back(current_interval);

            if (last_interval != nullptr) {
                solver()->AddConstraint(solver()->MakeIntervalVarRelation(
                        current_interval, operations_research::Solver::STARTS_AFTER_END, last_interval));
            }

            last_interval = current_interval;
            current_index = next_index;
        }

        std::copy(std::begin(break_intervals_), std::end(break_intervals_), std::back_inserter(all_intervals));

        solver()->AddConstraint(
                solver()->MakeStrictDisjunctiveConstraint(all_intervals,
                                                          absl::StrCat("Vehicle breaks ", vehicle_)));
    }

    const operations_research::RoutingDimension *dimension_;
    const int vehicle_;
    std::vector<operations_research::IntervalVar *> break_intervals_;
    operations_research::IntVar *const status_;
};

int main(int argc, char **argv) {
    using namespace operations_research;

    google::InitGoogleLogging(argv[0]);
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    const auto problem = CreateSample();
    RoutingModel model(static_cast<int>(problem.Visits.size() + 1),
                       static_cast<int>(problem.Breaks.size()),
                       problem.Depot);
    model.SetArcCostEvaluatorOfAllVehicles(NewPermanentCallback(&problem, &Problem::distance));

    static const auto FIX_CUMULATIVE_TO_ZERO = true;
    static const auto MAX_TIME_SLACK = 24 * 60 * 60;
    static const auto CAPACITY = 24 * 60 * 60;
    model.AddDimension(NewPermanentCallback(&problem, &Problem::service_plus_distance),
                       MAX_TIME_SLACK,
                       CAPACITY,
                       FIX_CUMULATIVE_TO_ZERO,
                       Problem::TIME_DIM);

    auto time_dimension = model.GetMutableDimension(Problem::TIME_DIM);
    for (auto visit_node = problem.Depot + 1; visit_node < model.nodes(); ++visit_node) {
        const auto &visit = problem.NodeToVisit(visit_node);
        const auto visit_index = model.NodeToIndex(visit_node);

        time_dimension->CumulVar(visit_index)->SetRange(visit.Begin, visit.End);
        model.AddVariableMinimizedByFinalizer(time_dimension->CumulVar(visit_index));
        model.AddToAssignment(time_dimension->SlackVar(visit_index));

        static const auto DROP_PENALTY = 1000000;
        model.AddDisjunction({visit_node}, DROP_PENALTY);
    }

    for (auto variable_index = 0; variable_index < model.Size(); ++variable_index) {
        model.AddVariableMinimizedByFinalizer(time_dimension->CumulVar(variable_index));
    }

    for (auto vehicle = 0; vehicle < model.vehicles(); ++vehicle) {
        std::vector < IntervalVar * > breaks;
        auto break_index = 0;
        for (const auto &break_config : problem.Breaks[vehicle]) {
            auto interval = model.solver()->MakeFixedInterval(break_config.Start,
                                                              break_config.Duration,
                                                              absl::StrCat("Break ",
                                                                           break_index,
                                                                           " of vehicle ",
                                                                           vehicle));
            breaks.emplace_back(interval);
        }

        if (FLAGS_built_in_breaks) {
            time_dimension->SetBreakIntervalsOfVehicle(std::move(breaks), vehicle);
        } else {
            model.solver()->AddConstraint(
                    model.solver()->RevAlloc(new BreakConstraint(time_dimension, vehicle, std::move(breaks))));
        }

        model.AddVariableMinimizedByFinalizer(time_dimension->CumulVar(model.Start(vehicle)));
        model.AddVariableMinimizedByFinalizer(time_dimension->CumulVar(model.End(vehicle)));
    }

    operations_research::RoutingSearchParameters parameters = BuildSearchParametersFromFlags();
    parameters.set_first_solution_strategy(FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION);
    model.CloseModelWithParameters(parameters);

    const auto &Overlap = [](const TimePeriod &break_period,
                             const TimePeriod &visit_period,
                             int vehicle,
                             const Visit &visit) -> bool {
        static const auto MIN_INTERSECTION = 1;

        const auto intersection = break_period.Intersection(visit_period);
        if (intersection.Duration <= MIN_INTERSECTION) {
            return false;
        }

        LOG(ERROR) << "The time period [" << visit_period.Start
                   << ", " << visit_period.Finish() << "] allocated for the visit (" << visit << ") "
                   << "overlaps with the break [" << break_period.Start
                   << ", " << break_period.Finish() << "] of the vehicle (" << vehicle << ")";
        return true;
    };


    const Assignment *solution = model.SolveWithParameters(parameters);
    CHECK(solution != nullptr) << "No solution found";

    Assignment solution_to_check{solution};
    CHECK(model.solver()->CheckAssignment(&solution_to_check));

    auto overlap_detected = false;
    for (auto vehicle = 0; vehicle < model.vehicles(); ++vehicle) {
        const std::vector <TimePeriod> &break_periods = problem.Breaks.at(static_cast<std::size_t>(vehicle));

        auto order = solution->Value(model.NextVar(model.Start(vehicle)));
        while (!model.IsEnd(order)) {
            const auto &visit = problem.NodeToVisit(model.IndexToNode(order));
            const auto &visit_start_var = time_dimension->CumulVar(order);
            TimePeriod min_period{solution->Min(visit_start_var), visit.Duration};
            TimePeriod max_period{solution->Max(visit_start_var), visit.Duration};

            for (const auto &break_period: break_periods) {
                overlap_detected |= Overlap(break_period, min_period, vehicle, visit);
                if (min_period != max_period) {
                    overlap_detected |= Overlap(break_period, max_period, vehicle, visit);
                }
            }

            order = solution->Value(model.NextVar(order));
        }
    }

    LOG_IF(ERROR, overlap_detected) << "Some breaks are violated";
    google::FlushLogFiles(google::ERROR);
    return 0;
}

Problem CreateSample() {
    return Problem{
            std::vector < Visit > {
                    {0,  "09:00:00", "10:00:00", "00:45:00"},
                    {0,  "09:00:00", "10:00:00", "00:45:00"},
                    {0,  "12:15:00", "13:15:00", "00:45:00"},
                    {0,  "12:15:00", "13:15:00", "00:45:00"},
                    {0,  "16:30:00", "17:30:00", "00:45:00"},
                    {0,  "16:30:00", "17:30:00", "00:45:00"},
                    {0,  "20:00:00", "21:00:00", "00:30:00"},
                    {0,  "20:00:00", "21:00:00", "00:30:00"},
                    {1,  "09:30:00", "10:30:00", "00:30:00"},
                    {2,  "08:45:00", "09:45:00", "00:15:00"},
                    {3,  "07:00:00", "08:00:00", "01:00:00"},
                    {3,  "07:00:00", "08:00:00", "01:00:00"},
                    {4,  "09:30:00", "10:30:00", "00:30:00"},
                    {4,  "17:30:00", "18:30:00", "00:30:00"},
                    {4,  "19:30:00", "20:30:00", "00:30:00"},
                    {5,  "08:15:00", "09:15:00", "00:15:00"},
                    {5,  "17:00:00", "18:00:00", "00:30:00"},
                    {3,  "08:45:00", "09:45:00", "00:30:00"},
                    {3,  "12:15:00", "13:15:00", "00:30:00"},
                    {3,  "16:30:00", "17:30:00", "00:15:00"},
                    {3,  "18:30:00", "19:30:00", "00:15:00"},
                    {6,  "08:00:00", "09:00:00", "00:30:00"},
                    {6,  "19:30:00", "20:30:00", "00:30:00"},
                    {7,  "09:00:00", "10:00:00", "00:30:00"},
                    {7,  "12:30:00", "13:30:00", "00:30:00"},
                    {7,  "16:30:00", "17:30:00", "00:30:00"},
                    {7,  "09:00:00", "10:00:00", "00:45:00"},
                    {7,  "12:00:00", "13:00:00", "00:30:00"},
                    {7,  "17:00:00", "18:00:00", "00:30:00"},
                    {7,  "18:45:00", "19:45:00", "00:30:00"},
                    {8,  "08:00:00", "09:00:00", "00:30:00"},
                    {8,  "11:00:00", "12:00:00", "01:00:00"},
                    {8,  "16:15:00", "17:15:00", "00:15:00"},
                    {8,  "19:30:00", "20:30:00", "00:15:00"},
                    {9,  "07:30:00", "08:30:00", "00:45:00"},
                    {9,  "11:30:00", "12:30:00", "00:30:00"},
                    {9,  "16:45:00", "17:45:00", "00:30:00"},
                    {9,  "19:00:00", "20:00:00", "00:30:00"},
                    {0,  "08:30:00", "09:30:00", "00:30:00"},
                    {0,  "12:30:00", "13:30:00", "00:30:00"},
                    {0,  "16:30:00", "17:30:00", "00:15:00"},
                    {0,  "19:30:00", "20:30:00", "00:30:00"},
                    {10, "18:30:00", "19:30:00", "00:15:00"},
                    {5,  "08:15:00", "09:15:00", "00:15:00"},
                    {5,  "12:30:00", "13:30:00", "00:30:00"},
                    {5,  "17:45:00", "18:45:00", "00:15:00"},
                    {11, "14:45:00", "15:45:00", "00:30:00"},
                    {12, "08:00:00", "09:00:00", "00:30:00"},
                    {13, "09:00:00", "10:00:00", "00:30:00"},
                    {14, "17:30:00", "18:30:00", "00:30:00"}
            },
            std::vector < std::vector < TimePeriod > > {
                    {
                            {"00:00:00", "08:00:00"},
                            {"13:00:00", "03:00:00"},
                            {"21:00:00", "03:00:00"}
                    },
                    {
                            {"00:00:00", "07:30:00"},
                            {"10:30:00", "01:30:00"},
                            {"14:00:00", "10:00:00"}
                    },
                    {

                            {"00:00:00", "08:00:00"},
                            {"13:00:00", "11:00:00"}
                    },
                    {
                            {"00:00:00", "08:00:00"},
                            {"11:00:00", "13:00:00"}
                    },
                    {
                            {"00:00:00", "08:00:00"},
                            {"11:00:00", "00:30:00"},
                            {"13:30:00", "03:00:00"},
                            {"19:00:00", "00:30:00"},
                            {"22:00:00", "02:00:00"}
                    },
                    {
                            {"00:00:00", "07:30:00"},
                            {"10:30:00", "01:30:00"},
                            {"14:00:00", "10:00:00"}
                    },
                    {
                            {"00:00:00", "08:00:00"},
                            {"10:30:00", "01:30:00"},
                            {"14:00:00", "10:00:00"}
                    },
                    {
                            {"00:00:00", "07:30:00"},
                            {"10:30:00", "01:30:00"},
                            {"14:00:00", "10:00:00"}
                    },
                    {
                            {"00:00:00", "09:00:00"},
                            {"11:00:00", "13:00:00"}
                    },
                    {
                            {"00:00:00", "07:30:00"},
                            {"10:30:00", "01:30:00"},
                            {"14:00:00", "10:00:00"}
                    },
                    {
                            {"00:00:00", "08:00:00"},
                            {"11:00:00", "00:30:00"},
                            {"13:30:00", "03:00:00"},
                            {"19:00:00", "00:30:00"},
                            {"22:00:00", "02:00:00"}
                    },
                    {
                            {"00:00:00", "07:30:00"},
                            {"13:00:00", "11:00:00"}
                    },
                    {
                            {"00:00:00", "16:30:00"},
                            {"21:30:00", "02:30:00"}
                    },
                    {
                            {"00:00:00", "07:30:00"},
                            {"11:00:00", "01:00:00"},
                            {"14:00:00", "10:00:00"}},
                    {
                            {"00:00:00", "08:00:00"},
                            {"11:00:00", "00:30:00"},
                            {"13:30:00", "03:00:00"},
                            {"19:00:00", "00:30:00"},
                            {"22:00:00", "02:00:00"}
                    },
                    {
                            {"00:00:00", "08:00:00"},
                            {"14:00:00", "03:00:00"},
                            {"21:00:00", "03:00:00"}
                    },
                    {
                            {"00:00:00", "07:30:00"},
                            {"10:30:00", "01:30:00"},
                            {"14:00:00", "10:00:00"}
                    },
                    {
                            {"00:00:00", "07:30:00"},
                            {"10:30:00", "05:30:00"},
                            {"19:30:00", "00:30:00"},
                            {"22:00:00", "02:00:00"}
                    },
                    {
                            {"00:00:00", "08:00:00"},
                            {"11:00:00", "00:30:00"},
                            {"13:30:00", "03:00:00"},
                            {"19:00:00", "00:30:00"},
                            {"22:00:00", "02:00:00"}
                    },
                    {
                            {"00:00:00", "08:00:00"},
                            {"11:00:00", "00:30:00"},
                            {"13:30:00", "03:00:00"},
                            {"19:30:00", "00:30:00"},
                            {"22:00:00", "02:00:00"}
                    },
                    {
                            {"00:00:00", "07:30:00"},
                            {"10:30:00", "06:00:00"},
                            {"21:30:00", "02:30:00"}
                    },
                    {
                            {"00:00:00", "08:00:00"},
                            {"11:00:00", "13:00:00"}
                    },
                    {
                            {"00:00:00", "07:30:00"},
                            {"10:30:00", "01:30:00"},
                            {"14:00:00", "10:00:00"}
                    },
                    {
                            {"00:00:00", "15:00:00"},
                            {"19:00:00", "05:00:00"}
                    }
            },
            {
                    {0, 722, 884, 604, 1562, 1129, 855, 655, 547, 432, 327, 945, 1170, 333, 517},
                    {722, 0, 1455, 1006, 1944, 819, 1425, 1376, 1269, 291, 1048, 1516, 1184, 392, 425},
                    {884, 1455, 0, 651, 2070, 1906, 229, 1083, 1140, 1173, 1134, 154, 1935, 1074, 1293},
                    {604, 1006, 651, 0, 2089, 1611, 621, 1127, 1074, 742, 870, 712, 1713, 753, 1004},
                    {1562, 1944, 2070, 2089, 0, 1509, 2186, 1146, 1015, 1993, 1322, 1942, 951, 1895, 1645},
                    {1129, 819, 1906, 1611, 1509, 0, 1877, 1414, 1173, 1073, 1167, 1967, 623, 920, 690},
                    {855, 1425, 229, 621, 2186, 1877, 0, 1224, 1171, 1143, 1106, 382, 1906, 1044, 1265},
                    {655, 1376, 1083, 1127, 1146, 1414, 1224, 0, 241, 1086, 448, 955, 1090, 988, 1063},
                    {547, 1269, 1140, 1074, 1015, 1173, 1171, 241, 0, 978, 333, 1012, 849, 880, 956},
                    {432, 291, 1173, 742, 1993, 1073, 1143, 1086, 978, 0, 758, 1234, 1322, 194, 511},
                    {327, 1048, 1134, 870, 1322, 1167, 1106, 448, 333, 758, 0, 1185, 844, 660, 735},
                    {945, 1516, 154, 712, 1942, 1967, 382, 955, 1012, 1234, 1185, 0, 1832, 1136, 1355},
                    {1170, 1184, 1935, 1713, 951, 623, 1906, 1090, 849, 1322, 844, 1832, 0, 1167, 885},
                    {333, 392, 1074, 753, 1895, 920, 1044, 988, 880, 194, 660, 1136, 1167, 0, 330},
                    {517, 425, 1293, 1004, 1645, 690, 1265, 1063, 956, 511, 735, 1355, 885, 330, 0}
            }
    };
}