import sys
from six import print_
import inspect
from sys import executable

current_ortools_version = "VVVV"
minimum_protobuf_version = "PROTOBUF_TAG"
print_ ("Python path : " + sys.executable)
print_ ("Python version : " + sys.version + "\n")

def notinstalled(modulename):
	return modulename + """ is not installed for \"""" + sys.executable + """\"
Please run \"easy_install --user """ + modulename + """\""""

def wrong_version(module, modulename, required_version):
	return """
You're using an old version of """ + modulename + """ : """ + inspect.getfile(module) + """
The minimum required version is : """ + required_version + """
Please run \"""" + str(sys.executable) + """ setup.py install --user\" to upgrade
If the problem persits, then """ + inspect.getfile(module) + """ is binding the newely installed version of """ + modulename + """
You should either remove it, or use PYTHONPATH to manage your sys.path. If you decide to use PYTHONPATH, please do it to run the ortools examples as well.
Please check https://docs.python.org/3/tutorial/modules.html#the-module-search-path from more information."""


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

#try to import ortools
try:
	import ortools
except ImportError:
    print_ (notinstalled("ortools"))
    raise SystemExit

#try to import protobuf
try:
	import google.protobuf
except ImportError:
    print_ (notinstalled("protobuf"))
    raise SystemExit

#check ortools version
try:
	if parse_version(current_ortools_version) > parse_version(ortools.__version__):
		raise Exception
	print_ ("or-tools version : " + ortools.__version__)
	print_ (inspect.getfile(ortools) + "\n")
except (AttributeError, Exception):
	print_ (wrong_version(ortools, "ortools", current_ortools_version))
	raise SystemExit

#check protobuf version
#print_(cmp(minimum_protobuf_version, google.protobuf.__version__) + "\n")
try:
	if parse_version(minimum_protobuf_version) > parse_version(google.protobuf.__version__):
		raise Exception
	print_ ("protobuf version : " + google.protobuf.__version__)
	print_ (inspect.getfile(google.protobuf) + "\n")
except (AttributeError, Exception):
	print_ (wrong_version(google.protobuf, "protobuf", minimum_protobuf_version))
	raise SystemExit
