import logging, sys, inspect
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


current_ortools_version = "VVVV"
minimum_protobuf_version = "PROTOBUF_TAG"

def notinstalled(modulename):
	return modulename + """ is not installed for \"""" + sys.executable + """\"
Please run \"""" + str(sys.executable) + """ setup.py install --user\""""

def wrong_version(module, modulename, minimum_version, installed_version):
	return """
You are using """ + modulename + """-""" + installed_version + """ : """ + inspect.getfile(module) + """
The minimum required version is : """ + minimum_version + """
Please run \"""" + str(sys.executable) + """ setup.py install --user\" to upgrade
If the problem persists, then """ + inspect.getfile(module) + """ is binding the newely installed version of """ + modulename + """
You should either remove it, or use PYTHONPATH to manage your sys.path. If you decide to use PYTHONPATH, do it to run the ortools examples as well.
Check https://docs.python.org/3/tutorial/modules.html#the-module-search-path from more information."""

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

	if version_info[0] >= 3:
		ortools_name = "py3-ortools"
	else:
		ortools_name = "ortools"
	#try to import ortools
	try:
		import ortools
	except ImportError:
	    logging.error (notinstalled(ortools_name))
	    raise SystemExit

	#try to import protobuf
	try:
		import google.protobuf
	except ImportError:
	    logging.error (notinstalled("protobuf"))
	    raise SystemExit

	#check ortools version
	try:
		if parse_version(current_ortools_version) > parse_version(ortools.__version__):
			raise Exception
		logging.info("or-tools version : " + ortools.__version__)
		logging.info(inspect.getfile(ortools) + "\n")
	except (AttributeError, Exception):
		logging.error (wrong_version(ortools, ortools_name, current_ortools_version, ortools.__version__))
		raise SystemExit

	#check protobuf version
	try:
		if parse_version(minimum_protobuf_version) > parse_version(google.protobuf.__version__):
			raise Exception
		logging.info("protobuf version : " + google.protobuf.__version__)
		logging.info(inspect.getfile(google.protobuf) + "\n")
	except (AttributeError, Exception):
		logging.error(wrong_version(google.protobuf, "protobuf", minimum_protobuf_version, google.protobuf.__version__))
		raise SystemExit

	# Check if python can load the libraries' modules
	# this is useful when the library architecture is not compatbile with the python executable,
	# or when the library's dependencies are not available or not compatible.
	from ortools.constraint_solver import _pywrapcp
	from ortools.linear_solver import _pywraplp
	from ortools.algorithms import _pywrapknapsack_solver
	from ortools.graph import _pywrapgraph


