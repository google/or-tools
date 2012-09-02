from setuptools import setup
from os.path import join as pjoin

setup(
    name='or-tools',
    version='1.0.VVVV',
    packages=[ 'constraint_solver',
               'linear_solver',
               'graph',
               'algorithms'
             ],
    install_requires = ['google-apputils >= 0.3', 'or-tools-module'],
    dependency_links = ['http://google-apputils-python.googlecode.com/files/'],
    data_files=[('constraint_solver',
                 [pjoin('constraint_solver', '_pywrapcp.dll'),
                  pjoin('constraint_solver', '_pywraprouting.dll']),
                ('linear_solver',
                 [pjoin('linear_solver', '_pywraplp.dll'), ]),
                ('graph',
                 [pjoin('graph', '_pywrapgraph.dll'), ]),
                ('algorithms',
                 [pjoin('algorithms', '_pywrapknapsack_solver.dll'), ]),
               ],
    license='Apache 2.0',
    author = "Google Inc",
    author_email = "lperron@google.com",
    description = "Google OR-Tools Python Module",
    keywords = ("operations research, constraint programming, " +
                "linear programming" + "flow algorithms" +
                "python"),
    url = "http://code.google.com/p/or-tools/",
)
