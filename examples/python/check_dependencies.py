import logging, sys, inspect
from os.path import dirname, abspath
from optparse import OptionParser

#try to import setuptools
try:
    from setuptools import setup, Extension
    from setuptools.command import easy_install
except ImportError:
    raise ImportError("""setuptools is not installed for \"""" + sys.executable + """\"
Follow this link for installing instructions :
https://pypi.python.org/pypi/setuptools
make sure you use \"""" + sys.executable + """\" during the installation""")
    raise SystemExit

from pkg_resources import parse_version

def notinstalled(modulename):
	return modulename + """ could not be imported for \"""" + sys.executable + """\"
Set PYTHONPATH to the output of this command \"make print-OR_TOOLS_PYTHONPATH\" before running the examples"""

def wrong_module(module_file, modulename):
	return """
The python examples are not importing the """ + modulename + """ module from the sources.
Remove the site-package that contains \"""" + module_file + """\", either manually or by using pip, and rerun this script again."""

def log_error_and_exit(error_message):
	logging.error(error_message)
	raise SystemExit

# Returns the n_th parent of file
def n_dirname(n, file):
	directory = file
	for x in xrange(0, n):
		directory = dirname(directory)
	return directory

if __name__ == '__main__':
	parser = OptionParser('Log level')
	parser.add_option('-l','--log',type='string',help='Available levels are CRITICAL (3), ERROR (2), WARNING (1), INFO (0), DEBUG (-1)',default='INFO')
	options,args = parser.parse_args()
 
	try:
		loglevel = getattr(logging,options.log.upper())
	except AttributeError:
		loglevel = {3:logging.CRITICAL,
					2:logging.ERROR,
					1:logging.WARNING,
					0:logging.INFO,
					-1:logging.DEBUG,
					}[int(options.log)]
	
	logging.basicConfig(format='[%(levelname)s] %(message)s',stream=sys.stdout, level=loglevel)
	
	logging.info("Python path : " + sys.executable)
	logging.info("Python version : " + sys.version)

	#try to import ortools
	try:
		import ortools
	except ImportError:
	    logging.error (notinstalled("ortools"))
	    raise SystemExit

	#check if we're using ortools from the sources or it's binded by pypi's module
	ortools_module_file = inspect.getfile(ortools)
	ortools_module_path = n_dirname(3, ortools_module_file)
	ortools_project_path = n_dirname(3, abspath(inspect.getfile(inspect.currentframe())))
	try:
		if(ortools_module_path == ortools_project_path):
			logging.info("Good! The python examples are importing the ortools module from the sources. The imported module is : " + inspect.getfile(ortools))
		else:
			raise Exception
	except (Exception):
		log_error_and_exit(wrong_module(ortools_module_file, "ortools"))

	#try to import protobuf
	try:
		import google.protobuf
		logging.info("Protobuf is imported from " + inspect.getfile(google.protobuf))
	except ImportError:
	    log_error_and_exit(notinstalled("protobuf"))

	# Check if python can load the libraries' modules
	# this is useful when the library architecture is not compatbile with the python executable,
	# or when the library's dependencies are not available or not compatible.
	from ortools.constraint_solver import _pywrapcp
	from ortools.linear_solver import _pywraplp
	from ortools.algorithms import _pywrapknapsack_solver
	from ortools.graph import _pywrapgraph


