
from ortools.sat.python import cp_model

#project name, duration, starts earliest, ends latest, demand role A, demand role S, demand role J
projects = [
            ['P1',3, 0, 9, 1, 0, 1],
            ['P2',8, 0, 9, 1, 1, 0],
            ['P3',3, 0, 9, 1, 0, 2],
            ['P4',4, 0, 9, 1, 0, 1]
            ]

num_projects = len(projects)

roles = ['A','S','J']

#Roles available at each time step
available_roles = [
    [2,2,2,2,2,2,2,2,2,2], #Role A
    [1,1,1,1,1,1,1,1,1,1], #Role S
    [1,1,1,1,1,1,1,2,2,2]  #Role J
]

all_projects = range(num_projects)
all_roles = range(len(roles))

# Creates the model.
model = cp_model.CpModel()

#Creating decision variables

#starts and ends of the projects
starts = [model.NewIntVar(projects[j][2], projects[j][3] + 1 , 'start_%i' % j) for j in all_projects]
ends = [model.NewIntVar(projects[j][2], projects[j][3] + 1, 'end_%i' % j) for j in all_projects]
intervals = [model.NewIntervalVar(starts[j], projects[j][1], ends[j], 'interval_%i' % j) for j in all_projects]

# Role A has a capacity 2. Every project uses it.
demands = [1 for _ in all_projects]
model.AddCumulative(intervals, demands, 2)

# Role S has a capacity of 1
model.AddNoOverlap([intervals[i] for i in all_projects if projects[i][5]])

# Project J has a capacity of 1 or 2.
used_capacity = model.NewIntervalVar(0, 7, 7, 'unavailable')
intervals_for_project_j = intervals + [used_capacity]
demands_for_project_j = [projects[j][6] for j in all_projects] + [1]
model.AddCumulative(intervals_for_project_j, demands_for_project_j, 2)

#We want the projects to start as early as possible
model.Minimize(sum(starts))

# Solve model.
solver = cp_model.CpSolver()
solver.parameters.log_search_progress = True
status=solver.Solve(model)

for it in zip(starts, ends):
    print("[%i, %i]" % (solver.Value(it[0]), solver.Value(it[1])))
