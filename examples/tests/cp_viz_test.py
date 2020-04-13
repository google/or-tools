from ortools.constraint_solver import pywrapcp

def MyTest():
        solver = pywrapcp.Solver("MyTest")
        dvar=[]
        d1 = solver.IntVar(0, 2, 'd1' )
        d2 = solver.IntVar(0, 2, 'd2' )
        d3 = solver.IntVar(0, 2, 'd2' )

        dvar=[d1,d2,d3]

        solver.Add( d1 != d2)
        solver.Add( d1 != d3)
        solver.Add( d2 != d3)

        solution = solver.Assignment()
        solution.Add(dvar)

        db = solver.Phase( dvar,solver.CHOOSE_FIRST_UNBOUND, solver.ASSIGN_MIN_VALUE )
        cpviz = solver.TreeMonitor( dvar, "tree.xml", "visualization.xml");
        solver.NewSearch(db, cpviz)

        while( solver.NextSolution() ):
                for item in dvar:
                        print 'Var %s=%d ' %( item, item.Value() ),
                print
        solver.EndSearch()

if __name__ == '__main__':
        MyTest()
