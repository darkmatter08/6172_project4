#!/usr/bin/python

"""
Runs test suite to compare player implementation to reference implementation

 - Procedure to add new statistic

   (1) Write a procedure that outputs a statistic of some kind (see get_avg_nps as example)
   (2) Add a (key,value) pair to stats variable in main procedure with following structure

        key  :         value
       LABEL : [ f(ref_o), f(exe_o) ]

       where LABEL is name of test statistic (string), f is the procedure to compute the
       statisitic.
"""

import sys
from subprocess import call
from os import listdir, remove

## Globals

class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'

MISMATCH_STR = bcolors.FAIL + ' < MISMATCH' + bcolors.ENDC

# Location of Reference Impl
REFERENCE_EXE = "./ref_player/leiserchess"

## Utilities

# Print help options
def print_usage():
  print bcolors.HEADER + 'Usage:'
  print '\n./run_tests.py <player_executable> <test_dir>'
  print '\n\t<player_executable> : leiserchess executable to test'
  print '\t<test_dir>          : directory of tests to run impl against'
  print '\nNOTE: pass in "--help" or "-h" to see all options' + bcolors.ENDC 

# Print summary of test results
def print_summary(avg_nps_incr):
  print '\n' + bcolors.HEADER + 'Summary:' + bcolors.ENDC
  print '\tAVG_NPS_INCR: ' + str(avg_nps_incr)

# Print for passed test 
def print_pass(f_name, stats):
  print '[ ' + bcolors.OKGREEN + 'PASS' + bcolors.ENDC + ' ] : ' + f_name
  ref_stats = [(label, stat[0]) for label, stat in stats.iteritems()]
  exe_stats = [(label, stat[1]) for label, stat in stats.iteritems()]
  print '\tReference Impl Stats:'
  for stat in ref_stats:
    print '\t\t' + stat[0] + ': ' + str(stat[1])
  print '\tPlayer Impl Stats:'
  for stat in exe_stats:
    print '\t\t' + stat[0] + ': ' + str(stat[1])

# Find index of element that does not match
def find_error(l1, l2):
  for i in range(len(l1)):
    if l1[i] != l2[i]:
      return i

# Print for failed test
def print_fail(f_name, expected, actual):
  print '[ ' + bcolors.FAIL + 'FAIL' + bcolors.ENDC + ' ] : ' + f_name
  err_idx = find_error(expected, actual)
  print '\tExpected:'
  for i in range(len(expected)):
    print '\t\t' + expected[i] + (MISMATCH_STR if err_idx == i else '')
  print '\tActual:'
  for i in range(len(actual)):
    print '\t\t' + actual[i] + (MISMATCH_STR if err_idx == i else '')

# Return average of elements of list l
def avg(l):
  return sum(l) / float(len(l))

# Return best move found
def get_bestmove(f_name):
  with open(f_name, 'r') as f:
    line = f.readline()
    while not line.startswith('bestmove'):
      line = f.readline()
  return line.rstrip()

# Return average nps over all depths 
def get_avg_nps(f_name):
  nps = []
  with open(f_name, 'r') as f:
    for line in f:
      if line.startswith('info depth'):
        nps.append(int(line.rstrip().split(' ')[-1]))
  return avg(nps)

# Return list of paths explored and best move found
def get_bestmove_path(f_name):
  path = []
  with open(f_name, 'r') as f:
    for line in f:
      if line.startswith('info score'):
        path.append(line.rstrip())
  # append the best move found for comparison
  path.append(get_bestmove(f_name))
  return path

if __name__ == '__main__':
  # Get command-line args
  exe = sys.argv[1]
  if exe == '--help' or exe == '-h':
    print_usage()
    exit(0)
  test_dir = sys.argv[2]
  if not test_dir.endswith('/'):
    test_dir += '/'

  print bcolors.OKBLUE + 'Running Tests...' + bcolors.ENDC

  nps_incr = []
  count = 0  # Track test number
  for test_file in listdir(test_dir):
    # Store output of test in intermediate files
    ref_o = 'ref_result_' + str(count)
    exe_o = 'exe_result_' + str(count)

    # Run each implementation on test files
    call(REFERENCE_EXE + ' ' + test_dir +  test_file + ' > ' + ref_o, shell=True)
    call(exe + ' ' + test_dir +  test_file + ' > ' + exe_o, shell=True)
    
    # Get best move path found for each impl
    ref_result = get_bestmove_path(ref_o)
    exe_result = get_bestmove_path(exe_o)

    passed = True
    if ref_result == exe_result:
      # If path explored by both impls is correct, test passes
      stats = {'AVG_NPS':[get_avg_nps(ref_o), get_avg_nps(exe_o)]}
      nps_incr.append((stats['AVG_NPS'][1] - stats['AVG_NPS'][0]) / float(stats['AVG_NPS'][0]))
      print_pass(test_file, stats)
    else:
      # Else test fails and location of mismatch is shown
      passed = False
      print_fail(test_file, ref_result, exe_result)

    # Remove intermediate files
    remove(ref_o)
    remove(exe_o)

    if not passed:
      print bcolors.FAIL + 'TEST FAILED!' + bcolors.ENDC
      exit(0)
    count += 1
  print bcolors.OKGREEN + 'ALL TESTS PASSED!' + bcolors.ENDC
  print_summary(avg(nps_incr))
