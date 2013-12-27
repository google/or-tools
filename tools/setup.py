from setuptools import setup
from os.path import join as pjoin

setup(
    name='or-tools',
    version='1.VVVV',
    packages=['ortools'],
    install_requires = ['google-apputils >= 0.4'],
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
    author = 'Google Inc',
    author_email = 'lperron@google.com',
    description = 'Google OR-Tools python libraries and modules',
    keywords = ('operations research, constraint programming, ' +
                'linear programming' + 'flow algorithms' +
                'python'),
    url = 'http://code.google.com/p/or-tools/',
    classifiers = [
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: Apache Software License',
        'Operating System :: MacOS :: MacOS X',
        'Operating System :: Microsoft :: Windows',
        'Operating System :: POSIX :: Linux',
        'Programming Language :: Python',
        'Topic :: Office/Business :: Scheduling',
        'Topic :: Scientific/Engineering',
        'Topic :: Scientific/Engineering :: Mathematics',
        'Topic :: Software Development :: Libraries :: Python Modules'],
)
