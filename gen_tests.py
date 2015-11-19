#!/usr/bin/python

"""
Randomly generates FEN configurations and creates appropriate leiserchess files for testing.
"""

import sys
from random import randint, random

## Globals

NUM_ROWS = 10
NUM_COLS = 10
NUM_PIECES = 16
PAWNS_PER_SIDE = 7

MAX_DEPTH = 6

# Probability that entry is empty given every piece is on the board
P_EMPTY = 1 - NUM_PIECES / float(NUM_ROWS * NUM_COLS)

WHITE = 'W'
BLACK = 'B'

KING_DIRECTIONS = ['nn', 'ss', 'ee', 'ww']
PAWN_DIRECTIONS = ['nw', 'ne', 'se', 'sw']

## Class Definitions

class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'

class King:
  def __init__(self, color):
    assert color == WHITE or color == BLACK
    self.color = color
    rand_idx = randint(0, 3)
    self.direction = KING_DIRECTIONS[rand_idx]
    if self.color == WHITE:
      self.direction = self.direction.upper()

class Pawn:
  def __init__(self, color):
    assert color == WHITE or color == BLACK
    self.color = color
    rand_idx = randint(0, 3)
    self.direction = PAWN_DIRECTIONS[rand_idx]
    if self.color == WHITE:
      self.direction = self.direction.upper()

## Utilities

# Returns whether there is both a black and white king
# on the board
def is_legal(fen_str):
  return ((KING_DIRECTIONS[0] in fen_str
        or KING_DIRECTIONS[1] in fen_str
        or KING_DIRECTIONS[2] in fen_str
        or KING_DIRECTIONS[3] in fen_str)
      and (KING_DIRECTIONS[0].upper() in fen_str
        or KING_DIRECTIONS[1].upper() in fen_str
        or KING_DIRECTIONS[2].upper() in fen_str
        or KING_DIRECTIONS[3].upper() in fen_str))

# Returns a fen_str representing a potential board 
# configuration. Not necessarily legal.
def gen_test():
  pieces = []
  # Create a set of all possible pieces
  for color in [BLACK, WHITE]:
    pieces.append(King(color))
    for pawn in range(PAWNS_PER_SIDE):
      pieces.append(Pawn(color))
  
  fen = []
  num_pieces_left = NUM_PIECES
  for row in range(NUM_ROWS):
    # Construct a fen representation for each rank
    # of the board.
    fen_str = ''
    # Track number of empty spaces
    consecutive_empty = 0
    for col in range(NUM_COLS):
      rand_idx = None
      if num_pieces_left > 0:
        rand_idx = randint(0, num_pieces_left - 1)
      leaveEmpty = random() < P_EMPTY
      if not leaveEmpty and rand_idx != None:
        # If we should fill in this entry and piece exists
        piece = pieces.pop(rand_idx)
        if consecutive_empty != 0:
          # If empty space is non-zero between this entry
          # and location of previous piece in row
          fen_str += str(consecutive_empty)
          consecutive_empty = 0
        fen_str += piece.direction
        num_pieces_left -= 1
      else:
        consecutive_empty += 1
    if consecutive_empty != 0:
      # Check in case last several entries do not hold a piece
      fen_str += str(consecutive_empty)
    fen.append(fen_str)

  # Build FEN string and include which player's turn it is
  position = '/'.join(fen) + ' ' + (WHITE if randint(0, 1) else BLACK)
  return position

def print_usage():
  print bcolors.HEADER + 'Usage:'
  print '\n./gen_tests.py <num_tests>'
  print '\n\t<num_tests : number of random tests to generate'
  print '\nNOTE: pass in "--help" or "-h" to see all options' + bcolors.ENDC

if __name__ == '__main__':
  # Get command-line args
  num_tests = sys.argv[1]
  if num_tests == '--help' or num_tests == '-h':
    print_usage()
    exit(0)
  num_tests = int(num_tests)
  test_dir = sys.argv[2]
  if not test_dir.endswith('/'):
    test_dir += '/'
  
  print bcolors.OKBLUE + 'Generating %(num_tests)d random tests...' % locals() + bcolors.ENDC
  print bcolors.WARNING + '\tMAX_DEPTH     : ' + str(MAX_DEPTH) + bcolors.ENDC
  print bcolors.WARNING + '\tP[LEFT_EMPTY] : ' + str(P_EMPTY) + bcolors.ENDC
  for test_num in range(num_tests):
    fen_str = gen_test()
    while not is_legal(fen_str):
      fen_str = gen_test()
    depth = randint(1, MAX_DEPTH)
    with open(test_dir + 'rand_t' + str(test_num), 'w') as f:
      f.write('position fen ' + fen_str)
      f.write('\ngo depth ' + str(depth))
      f.write('\nquit')
  print bcolors.OKBLUE + 'DONE!' + bcolors.ENDC
