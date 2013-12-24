from setuptools import setup
from os.path import join as pjoin

setup(
    name='or-tools',
    version='1.0.VVVV',
    packages=[ 'ortools',
               'ortools.constraint_solver',
               'ortools.linear_solver',
               'ortools.graph',
               'ortools.algorithms'
             ],
    install_requires = ['google-apputils >= 0.3'],
    dependency_links = ['http://google-apputils-python.googlecode.com/files/'],
    data_files=[(pjoin('ortools', 'constraint_solver'),
                 [pjoin('ortools', 'constraint_solver', '_pywrapcp.dll'),
                  pjoin('ortools', 'constraint_solver', '_pywraprouting.dll')]),
                (pjoin('ortools', 'linear_solver'),
                 [pjoin('ortools', 'linear_solver', '_pywraplp.dll'), ]),
                (pjoin('ortools', 'graph'),
                 [pjoin('ortools', 'graph', '_pywrapgraph.dll'), ]),
                (pjoin('ortools', 'algorithms'),
                 [pjoin('ortools', 'algorithms', '_pywrapknapsack_solver.dll'), ]),
               ],
    license='Apache 2.0',
    author = "Google Inc",
    author_email = "lperron@google.com",
    description = "Google OR-Tools python libraries and modules",
    keywords = ("operations research, constraint programming, " +
                "linear programming" + "flow algorithms" +
                "python"),
    url = "http://code.google.com/p/or-tools/",
)
