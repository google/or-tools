#!/usr/bin/env python3
"""Check user python installation."""
import inspect
import logging
import optparse
import sys

# try to import setuptools
try:
  from setuptools import setup  # pylint: disable=g-import-not-at-top,unused-import
  from setuptools import Extension  # pylint: disable=g-import-not-at-top,unused-import
  from setuptools.command import easy_install  # pylint: disable=g-import-not-at-top,unused-import
except ImportError:
  raise ImportError("""setuptools is not installed for \"""" + sys.executable +
                    """\"
Follow this link for installing instructions :
https://pypi.python.org/pypi/setuptools
make sure you use \"""" + sys.executable + """\" during the installation""")

from pkg_resources import parse_version  # pylint: disable=g-import-not-at-top,unused-import

required_ortools_version = "VVVV"
required_protobuf_version = "PROTOBUF_TAG"


def notinstalled(modulename):
  return modulename + """ is not installed for \"""" + sys.executable + """\"
Run \"""" + sys.executable + """ setup.py install --user\" to install it"""


def absent_version(module, modulename):
  return """You are using a """ + modulename + """ module that doesn't have a __version__ attribute : """ + inspect.getfile(
      module) + """\"
Run \"""" + sys.executable + """ setup.py install --user\" to upgrade.
If the problem persists, remove the site-package that contains \"""" + inspect.getfile(
    module) + """\". You can do so either manually or by using pip."""


def wrong_version(module, modulename, required_version, installed_version):
  return """You are using """ + modulename + """-""" + installed_version + """ : """ + inspect.getfile(
      module
  ) + """, while the required version is : """ + required_version + """
Run \"""" + sys.executable + """ setup.py install --user\" to upgrade.
If the problem persists, remove the site-package that contains \"""" + inspect.getfile(
    module) + """\". You can do so either manually or by using pip."""


def log_error_and_exit(error_message):
  logging.error(error_message)
  raise SystemExit


def check_absent_version(module, modulename):
  if not hasattr(module, "__version__"):
    log_error_and_exit(absent_version(module, modulename))


if __name__ == "__main__":
  parser = optparse.OptionParser("Log level")
  parser.add_option(
      "-l",
      "--log",
      type="string",
      help="Available levels are CRITICAL (3), ERROR (2), WARNING (1), INFO (0), DEBUG (-1)",
      default="INFO")
  options, args = parser.parse_args()

  # Create the logger
  try:
    loglevel = getattr(logging, options.log.upper())
  except AttributeError:
    loglevel = {
        3: logging.CRITICAL,
        2: logging.ERROR,
        1: logging.WARNING,
        0: logging.INFO,
        -1: logging.DEBUG,
    }[int(options.log)]

  logging.basicConfig(
      format="[%(levelname)s] %(message)s", stream=sys.stdout, level=loglevel)

  # Display Python Version and path
  logging.info("Python path : %s", sys.executable)
  logging.info("Python version : %s", sys.version)

  # Choose the pypi package
  ortools_name = "ortools"

  # try to import ortools
  try:
    import ortools  # pylint: disable=g-import-not-at-top
  except ImportError:
    log_error_and_exit(notinstalled(ortools_name))

  # try to import protobuf
  try:
    import google.protobuf  # pylint: disable=g-import-not-at-top
  except ImportError:
    log_error_and_exit(notinstalled("protobuf"))

  # check ortools version
  try:
    check_absent_version(ortools, "ortools")
    if required_ortools_version != ortools.__version__:
      raise Exception
    logging.info("or-tools version : " + ortools.__version__ + "\n" +
                 inspect.getfile(ortools))
  except (AttributeError, Exception):  # pylint: disable=broad-except
    log_error_and_exit(
        wrong_version(ortools, ortools_name, required_ortools_version,
                      ortools.__version__))

  # check protobuf version
  try:
    check_absent_version(google.protobuf, "protobuf")
    if required_protobuf_version != google.protobuf.__version__:
      raise Exception
    logging.info("protobuf version : " + google.protobuf.__version__ + "\n" +
                 inspect.getfile(google.protobuf))
  except (AttributeError, Exception):  # pylint: disable=broad-except
    log_error_and_exit(
        wrong_version(google.protobuf, "protobuf", required_protobuf_version,
                      google.protobuf.__version__))

  # Check if python can load the libraries' modules
  # this is useful when the library architecture is not compatible with the
  # python executable, or when the library's dependencies are not available or
  # not compatible.
  from ortools.constraint_solver import _pywrapcp  # pylint: disable=g-import-not-at-top,unused-import
  from ortools.linear_solver import _pywraplp  # pylint: disable=g-import-not-at-top,unused-import
  from ortools.algorithms import _pywrapknapsack_solver  # pylint: disable=g-import-not-at-top,unused-import
  from ortools.graph import _pywrapgraph  # pylint: disable=g-import-not-at-top,unused-import
