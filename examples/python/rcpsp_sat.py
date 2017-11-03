from __future__ import print_function

from ortools.sat.python import cp_model
from ortools.util import rcpsp_pb2
from ortools.util import pywraputil

import argparse
import time

parser = argparse.ArgumentParser()

parser.add_argument('--input', default = "",
                    help = 'Input file to parse and solve.')



def main(args):
  parser = pywraputil.RcpspParser()
  parser.LoadFile(args.input)
  problem = parser.Problem()
  print(str(problem))



if __name__ == '__main__':
  main(parser.parse_args())
