import logging, sys, inspect, os
from os.path import dirname, abspath
from optparse import OptionParser

#try to import setuptools
try:
    from setuptools import setup, Extension
    from setuptools.command import easy_install
except ImportError:
    raise ImportError("""setuptools is not installed for \"""" + sys.executable + """\"
Please follow this link for installing instructions :
https://pypi.python.org/pypi/setuptools
make sure you use \"""" + sys.executable + """\" during the installation""")
    raise SystemExit

from pkg_resources import parse_version

def notinstalled(modulename):
	return modulename + """ could not be imported for \"""" + sys.executable + """\"
Please set PYTHONPATH to the output of this command \"make print-OR_TOOLS_PYTHONPATH\" before running the examples"""

def wrong_version(module, modulename, minimum_version, installed_version):
	return """
You are using """ + modulename + """-""" + installed_version + """ : """ + inspect.getfile(module) + """
The minimum required version is : """ + minimum_version + """
Please run \"""" + str(sys.executable) + """ setup.py install --user\" to upgrade
If the problem persists, then """ + inspect.getfile(module) + """ is binding the newely installed version of """ + modulename + """
You should either remove it, or use PYTHONPATH to manage your sys.path. If you decide to use PYTHONPATH, do it to run the ortools examples as well.
Check https://docs.python.org/3/tutorial/modules.html#the-module-search-path from more information."""

def wrong_module(module_file, modulename):
	return """
The python examples are not importing the """ + modulename + """ module from the sources because it's binded by \"""" + module_file + """\".
Please delete the binding module and rerun this script again. 
"""

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
	
	logging.basicConfig(stream=sys.stdout, level=loglevel)
	
	logging.info("Python path : " + sys.executable)
	logging.info("Python version : " + sys.version + "\n")

	#try to import ortools
	try:
		import ortools
	except ImportError:
	    logging.error (notinstalled("ortools"))
	    raise SystemExit

	#check if we're using ortools from the sources or it's binded by pypi's module
	ortools_module_file = inspect.getfile(ortools)
	ortools_module_path = dirname(dirname(dirname(ortools_module_file)))
	ortools_project_path = dirname(dirname(dirname(abspath(inspect.getfile(inspect.currentframe())))))
	try:
		if(ortools_module_path == ortools_project_path):
			logging.info("Good! The python examples are importing the ortools module from the sources. The imported module is : " + inspect.getfile(ortools))
		else:
			raise Exception
	except (Exception):
		logging.error(wrong_module(ortools_module_file, "ortools"))
		raise SystemExit

	#try to import protobuf
	try:
		import google.protobuf
		logging.info("Protobuf is imported from " + inspect.getfile(google.protobuf))
	except ImportError:
	    logging.error (notinstalled("protobuf"))
	    raise SystemExit

	# Check if python can load the libraries' modules
	# this is useful when the library architecture is not compatbile with the python executable,
	# or when the library's dependencies are not available or not compatible.
	from ortools.constraint_solver import _pywrapcp
	from ortools.linear_solver import _pywraplp
	from ortools.algorithms import _pywrapknapsack_solver
	from ortools.graph import _pywrapgraph


