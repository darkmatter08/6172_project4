// Copyright (c) 2015 MIT License by 6.172 Staff

#include "./move_gen.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "./tbassert.h"
#include "./fen.h"
#include "./search.h"
#include "./util.h"

#define MAX(x, y)  ((x) > (y) ? (x) : (y))
#define MIN(x, y)  ((x) < (y) ? (x) : (y))

int USE_KO;  // Respect the Ko rule

static char *color_strs[2] = {"White", "Black"};

char *color_to_str(color_t c) {
  return color_strs[c];
}

// -----------------------------------------------------------------------------
// Piece getters and setters. Color, then type, then orientation.
// -----------------------------------------------------------------------------

bool check_piece_lists(position_t *p) {
  /* for (color_t c = 0; c < 2; c++) {
   *   for (int i = 0; i < HALF_NUM_PAWNS; i++) {
   *     if (p->ploc[c][i] == 0) {
   *       if (p->p_piece[c][i] != 0) {
   *         tbassert(false, "foo");
   *         return false;
   *       }
   *       continue;
   *     }
   *     if (p->board[p->ploc[c][i]] != p->p_piece[c][i]) {
   *       tbassert(false, "foo");
   *       return false;
   *     }
   *     if (ptype_of(p->p_piece[c][i]) != PAWN || color_of(p->p_piece[c][i]) != c) {
   *       tbassert(false, "foo");
   *       return false;
   *     }
   *   }
   *   if (p->board[p->kloc[c]] != p->k_piece[c]) {
   *     tbassert(false, "foo");
   *     return false;
   *   }
   * } */
  return true;
}


int check_position_integrity(position_t *p) {
  for (fil_t f = 0; f < BOARD_WIDTH; f++) {
    square_t sq = (FIL_ORIGIN + f) * ARR_WIDTH + RNK_ORIGIN;
    for (rnk_t r = 0; r < BOARD_WIDTH; r++, sq++) {
      if (ptype_of(get_piece(p, sq)) == PAWN) {
        int pawn_found = 0;
        for (int color = WHITE; color < 2; color++) {
          for (int i = 0; i < HALF_NUM_PAWNS; i++) {
            if (p->ploc[color][i] == sq) {
              pawn_found = 1;
              color = 2; // Needed to break out of outer loop
              break;
            }
          }
        }
        if (pawn_found == 0) {
          tbassert(false, "foo");
          return 0;
        }
      }
    }
  }

  return 1;
}

int check_pawn_counts(position_t *p) {
  int live_pawn_count = 0;
  for (fil_t f = 0; f < BOARD_WIDTH; f++) {
    square_t sq = (FIL_ORIGIN + f) * ARR_WIDTH + RNK_ORIGIN;
    for (rnk_t r = 0; r < BOARD_WIDTH; r++, sq++) {
      if (ptype_of(get_piece(p, sq)) == PAWN) {
        live_pawn_count++;
      }
    }
  }
  int ploc_count = 0;
  for (int color = WHITE; color < 2; color++) {
    for (int i = 0; i < HALF_NUM_PAWNS; i++) {
      if (p->ploc[color][i] != 0) {
        ploc_count++;
      }
    }
  }
  if (ploc_count == live_pawn_count) {
    return 1;
  }
  return 0;
}

// King orientations
char *king_ori_to_rep[2][NUM_ORI] = { { "NN", "EE", "SS", "WW" },
                                      { "nn", "ee", "ss", "ww" } };

// Pawn orientations
char *pawn_ori_to_rep[2][NUM_ORI] = { { "NW", "NE", "SE", "SW" },
                                      { "nw", "ne", "se", "sw" } };

char *nesw_to_str[NUM_ORI] = {"north", "east", "south", "west"};

// -----------------------------------------------------------------------------
// Board, squares
// -----------------------------------------------------------------------------

static uint64_t   zob[ARR_SIZE][1<<PIECE_SIZE];
static uint64_t   zob_color;
uint64_t myrand();

// Zobrist hashing
uint64_t compute_zob_key(position_t *p) {
  uint64_t key = 0;
  for (fil_t f = 0; f < BOARD_WIDTH; f++) {
    square_t sq = (FIL_ORIGIN + f) * ARR_WIDTH + RNK_ORIGIN;
    for (rnk_t r = 0; r < BOARD_WIDTH; r++, sq++) {
      key ^= zob[sq][get_piece(p, sq)];
    }
  }
  if (color_to_move_of(p) == BLACK)
    key ^= zob_color;

  return key;
}

void init_zob() {
  for (int i = 0; i < ARR_SIZE; i++) {
    for (int j = 0; j < (1 << PIECE_SIZE); j++) {
      zob[i][j] = myrand();
    }
  }
  zob_color = myrand();
}

// reflect[beam_dir][pawn_orientation]
// -1 indicates back of Pawn
static int reflect[NUM_ORI][NUM_ORI] = {
  //  NW  NE  SE  SW
  { -1, -1, EE, WW},   // NN
  { NN, -1, -1, SS},   // EE
  { WW, EE, -1, -1 },  // SS
  { -1, NN, SS, -1 }   // WW
};

int reflect_of(int beam_dir, int pawn_ori) {
  tbassert(beam_dir >= 0 && beam_dir < NUM_ORI, "beam-dir: %d\n", beam_dir);
  tbassert(pawn_ori >= 0 && pawn_ori < NUM_ORI, "pawn-ori: %d\n", pawn_ori);
  return reflect[beam_dir][pawn_ori];
}

// converts a square to string notation, returns number of characters printed
int square_to_str(square_t sq, char *buf, size_t bufsize) {
  fil_t f = fil_of(sq);
  rnk_t r = rnk_of(sq);
  if (f >= 0) {
    return snprintf(buf, bufsize, "%c%d", 'a'+ f, r);
  } else  {
    return snprintf(buf, bufsize, "%c%d", 'z' + f + 1, r);
  }
}

// direction map
static int dir[8] = { -ARR_WIDTH - 1, -ARR_WIDTH, -ARR_WIDTH + 1, -1, 1,
                      ARR_WIDTH - 1, ARR_WIDTH, ARR_WIDTH + 1 };
int dir_of(int i) {
  tbassert(i >= 0 && i < 8, "i: %d\n", i);
  return dir[i];
}


// directions for laser: NN, EE, SS, WW
static int beam[NUM_ORI] = {1, ARR_WIDTH, -1, -ARR_WIDTH};

int beam_of(int direction) {
  tbassert(direction >= 0 && direction < NUM_ORI, "dir: %d\n", direction);
  return beam[direction];
}

// -----------------------------------------------------------------------------
// Move getters and setters.
// -----------------------------------------------------------------------------

// converts a move to string notation for FEN
void move_to_str(move_t mv, char *buf, size_t bufsize) {
  square_t f = from_square(mv);  // from-square
  square_t t = to_square(mv);    // to-square
  rot_t r = rot_of(mv);          // rotation
  const char *orig_buf = buf;

  buf += square_to_str(f, buf, bufsize);
  if (f != t) {
    buf += square_to_str(t, buf, bufsize - (buf - orig_buf));
  } else {
    switch (r) {
      case NONE:
        buf += square_to_str(t, buf, bufsize - (buf - orig_buf));
        break;
      case RIGHT:
        buf += snprintf(buf, bufsize - (buf - orig_buf), "R");
        break;
      case UTURN:
        buf += snprintf(buf, bufsize - (buf - orig_buf), "U");
        break;
      case LEFT:
        buf += snprintf(buf, bufsize - (buf - orig_buf), "L");
        break;
      default:
        tbassert(false, "Whoa, now.  Whoa, I say.\n");  // Bad, bad, bad
        break;
    }
  }
}

// Generate all moves from position p.  Returns number of moves.
// strict currently ignored
int generate_all(position_t * restrict p, sortable_move_t * restrict sortable_move_list,
                 bool strict) {
  color_t color_to_move = color_to_move_of(p);
  // Make sure that the enemy_laser map is marked
  char laser_map[ARR_SIZE];
  for (int i = 0; i < ARR_SIZE; ++i) {
    laser_map[i] = 4;   // Invalid square
  }
  mark_laser_path(p, laser_map, opp_color(color_to_move));

  int move_count = 0;

  for (fil_t f = 0; f < BOARD_WIDTH; f++) {
    square_t sq = (FIL_ORIGIN + f) * ARR_WIDTH + RNK_ORIGIN;
    for (rnk_t r = 0; r < BOARD_WIDTH; r++, sq++) {
      piece_t x = get_piece(p, sq);

      ptype_t typ = ptype_of(x);
      color_t color = color_of(x);

      switch (typ) {
        case EMPTY:
          break;
        case PAWN:
          if (laser_map[sq] == 1) continue;  // Piece is pinned down by laser.
        case KING:
          if (color != color_to_move) {  // Wrong color
            break;
          }
          // directions
          for (int d = 0; d < 8; d++) {
            int dest = sq + dir_of(d);
            // Skip moves into invalid squares, squares occupied by
            // kings, nonempty squares if x is a king, and squares with
            // pawns of matching color
            if (ptype_of(get_piece(p, dest)) == INVALID ||
                ptype_of(get_piece(p, dest)) == KING ||
                (typ == KING && ptype_of(get_piece(p, dest)) != EMPTY) ||
                (typ == PAWN && ptype_of(get_piece(p, dest)) == PAWN &&
                 color == color_of(get_piece(p, dest)))) {
              continue;    // illegal square
            }

            WHEN_DEBUG_VERBOSE(char buf[MAX_CHARS_IN_MOVE]);
            WHEN_DEBUG_VERBOSE({
                move_to_str(move_of(typ, (rot_t) 0, sq, dest), buf, MAX_CHARS_IN_MOVE);
                DEBUG_LOG(1, "Before: %s ", buf);
              });
            tbassert(move_count < MAX_NUM_MOVES, "move_count: %d\n", move_count);
            sortable_move_list[move_count++] = move_of(typ, (rot_t) 0, sq, dest);

            WHEN_DEBUG_VERBOSE({
                move_to_str(get_move(sortable_move_list[move_count-1]), buf, MAX_CHARS_IN_MOVE);
                DEBUG_LOG(1, "After: %s\n", buf);
              });
          }

          // rotations - three directions possible
          for (int rot = 1; rot < 4; ++rot) {
            tbassert(move_count < MAX_NUM_MOVES, "move_count: %d\n", move_count);
            sortable_move_list[move_count++] = move_of(typ, (rot_t) rot, sq, sq);
          }
          if (typ == KING) {  // Also generate null move
            tbassert(move_count < MAX_NUM_MOVES, "move_count: %d\n", move_count);
            sortable_move_list[move_count++] = move_of(typ, (rot_t) 0, sq, sq);
          }
          break;
        case INVALID:
        default:
          tbassert(false, "Bogus, man.\n");  // Couldn't BE more bogus!
      }
    }
  }

  WHEN_DEBUG_VERBOSE({
      DEBUG_LOG(1, "\nGenerated moves: ");
      for (int i = 0; i < move_count; ++i) {
        char buf[MAX_CHARS_IN_MOVE];
        move_to_str(get_move(sortable_move_list[i]), buf, MAX_CHARS_IN_MOVE);
        DEBUG_LOG(1, "%s ", buf);
      }
      DEBUG_LOG(1, "\n");
    });

  return move_count;
}

int generate_king_moves(position_t *p, square_t sq, sortable_move_t *sortable_move_list, int move_count) {
  for (int d = 0; d < 8; d++) {
    int dest = sq + dir_of(d);
    piece_t piece = get_piece(p, dest);
    ptype_t typ = ptype_of(piece);
    if (typ == EMPTY) {
      sortable_move_list[move_count++] = move_of(KING, (rot_t) 0, sq, dest);
    }
  }
  for (int rot = 1; rot < 4; ++rot) {
    sortable_move_list[move_count++] = move_of(KING, (rot_t) rot, sq, sq);
  }
  sortable_move_list[move_count++] = move_of(KING, (rot_t) 0, sq, sq);

  return move_count;
}

int generate_pawn_moves(position_t *p, square_t sq, color_t c, sortable_move_t *sortable_move_list, int move_count, char *laser_map) {
  if (laser_map[sq] == 1) {
    return move_count;
  }

  for (int d = 0; d < 8; d++) {
    int dest = sq + dir_of(d);
    if (!square_inbounds(dest)) {
      continue;
    }
    bool hits_same_pawn = false;
    for (int i = 0; i < HALF_NUM_PAWNS; i++) {
      if (dest == p->ploc[color_to_move_of(p)][i]) {
        hits_same_pawn = false;
        break;
      }
    }
    if (!hits_same_pawn && dest != p->kloc[WHITE] && dest != p->kloc[BLACK]) {
      sortable_move_list[move_count++] = move_of(PAWN, (rot_t) 0, sq, dest);
    }
  }
  for (int rot = 1; rot < 4; ++rot) {
    sortable_move_list[move_count++] = move_of(PAWN, (rot_t) rot, sq, sq);
  }

  return move_count;
}

// Generate all moves from position p.  Returns number of moves.
// strict currently ignored
int generate_all_opt(position_t *p, sortable_move_t *sortable_move_list,
                 bool strict) {
  color_t color_to_move = color_to_move_of(p);
  // Make sure that the enemy_laser map is marked

  char laser_map[ARR_SIZE];
  for (int i = 0; i < ARR_SIZE; ++i) {
    laser_map[i] = 4;   // Invalid square
  }
  mark_laser_path(p, laser_map, opp_color(color_to_move));

  int move_count = 0;

  square_t k = p->kloc[color_to_move];
  move_count = generate_king_moves(p, k, sortable_move_list, move_count);
  for (int i = 0; i < HALF_NUM_PAWNS; i++) {
    square_t pawn = p->ploc[color_to_move][i];
    if (pawn != 0) {
      move_count = generate_pawn_moves(p, pawn, color_to_move, sortable_move_list, move_count, laser_map);
    }
  }

  return move_count;
}

static inline void swap_positions(position_t * restrict old, position_t * restrict p) {
  p->ply = old->ply + 1;
  p->key = old->key;

  for (int i = 0; i < 2; i++) {
    p->kloc[i] = old->kloc[i];
    p->k_piece[i] = old->k_piece[i];
  }

  for (int c = 0; c < 2; c++) {
    for (int i = 0; i < HALF_NUM_PAWNS; i++) {
      p->ploc[c][i] = old->ploc[c][i];
      p->p_piece[c][i] = old->p_piece[c][i];
    }
  }
}

inline square_t low_level_make_move(position_t * restrict old, position_t * restrict p, move_t mv) {
  tbassert(mv != 0, "mv was zero.\n");

  square_t stomped_dst_sq = 0;

  WHEN_DEBUG_VERBOSE(char buf[MAX_CHARS_IN_MOVE]);
  WHEN_DEBUG_VERBOSE({
      move_to_str(mv, buf, MAX_CHARS_IN_MOVE);
      DEBUG_LOG(1, "low_level_make_move: %s\n", buf);
    });

  tbassert(old->key == compute_zob_key(old),
           "old->key: %"PRIu64", zob-key: %"PRIu64"\n",
           old->key, compute_zob_key(old));

  WHEN_DEBUG_VERBOSE({
      fprintf(stderr, "Before:\n");
      display(old);
    });

  square_t from_sq = from_square(mv);
  square_t to_sq = to_square(mv);
  rot_t rot = rot_of(mv);

  WHEN_DEBUG_VERBOSE({
      DEBUG_LOG(1, "low_level_make_move 2:\n");
      square_to_str(from_sq, buf, MAX_CHARS_IN_MOVE);
      DEBUG_LOG(1, "from_sq: %s\n", buf);
      square_to_str(to_sq, buf, MAX_CHARS_IN_MOVE);
      DEBUG_LOG(1, "to_sq: %s\n", buf);
      switch (rot) {
        case NONE:
          DEBUG_LOG(1, "rot: none\n");
          break;
        case RIGHT:
          DEBUG_LOG(1, "rot: R\n");
          break;
        case UTURN:
          DEBUG_LOG(1, "rot: U\n");
          break;
        case LEFT:
          DEBUG_LOG(1, "rot: L\n");
          break;
        default:
          tbassert(false, "Not like a boss at all.\n");  // Bad, bad, bad
          break;
      }
    });

  *p = *old;
  p->ply++;

  p->history = old;
  p->last_move = mv;

  tbassert(from_sq < ARR_SIZE && from_sq > 0, "from_sq: %d\n", from_sq);
  tbassert(get_piece(p, from_sq) < (1 << PIECE_SIZE) && get_piece(p, from_sq) >= 0,
           "get_piece(p, from_sq): %d\n", get_piece(p, from_sq));
  tbassert(to_sq < ARR_SIZE && to_sq > 0, "to_sq: %d\n", to_sq);
  tbassert(get_piece(p, to_sq) < (1 << PIECE_SIZE) && get_piece(p, to_sq) >= 0,
           "get_piece(p, to_sq): %d\n", get_piece(p, to_sq));

  p->key ^= zob_color;   // swap color to move

  piece_t from_piece = get_piece(p, from_sq);
  piece_t to_piece;
  color_t from_color = color_to_move_of(old);
  color_t to_color;
  if (to_sq == from_sq) {
    // rotation
    to_piece = from_piece;
    to_color = from_color;
  } else {
    // moved
    to_color = opp_color(from_color);
    to_piece = get_piece_with_color(p, to_color, to_sq);
  }
  ptype_t from_type = ptype_of(from_piece);
  ptype_t to_type = ptype_of(to_piece);

  // Pieces block each other, unless a pawn is stomping an enemy pawn
  tbassert(EMPTY == ptype_of(to_piece) ||
           from_sq == to_sq ||
           (PAWN == ptype_of(from_piece) &&
            PAWN == ptype_of(to_piece) &&
            color_of(to_piece) == opp_color(color_of(from_piece))),
           "from-type: %d, to-type: %d, from-sq: %d, to-sq: %d, from-color: %d, to-color: %d\n",
           ptype_of(from_piece), ptype_of(to_piece),
           from_sq, to_sq,
           color_of(from_piece), color_of(to_piece));

  if (to_sq != from_sq) {  // move, not rotation
    if (PAWN == from_type &&
        PAWN == to_type &&
        to_color != from_color) {
      // We're stomping a piece.  Return the destination of the
      // stomped piece.  Let the caller remove the piece from the
      // board.
      stomped_dst_sq = from_sq;
      for (int i = 0; i < HALF_NUM_PAWNS; i++) { // loop over black pawns
        if (p->ploc[from_color][i] == from_sq) {
          p->ploc[from_color][i] = to_sq;
          break;
        }
      }
      for (int i = 0; i < HALF_NUM_PAWNS; i++) {
        if (p->ploc[to_color][i] == to_sq) {
          p->ploc[to_color][i] = from_sq;
          break;
        }
      }

    } else if (PAWN == from_type && EMPTY == to_type) {
      for (int i = 0; i < HALF_NUM_PAWNS; i++) {
        if (from_sq == p->ploc[from_color][i]) {
          p->ploc[from_color][i] = to_sq;
          break;
        }
      }
    }

    // Hash key updates
    p->key ^= zob[from_sq][from_piece];  // remove from_piece from from_sq
    p->key ^= zob[to_sq][to_piece];  // remove to_piece from to_sq

    p->key ^= zob[to_sq][from_piece];  // place from_piece in to_sq
    p->key ^= zob[from_sq][to_piece];  // place to_piece in from_sq

    // Update King locations if necessary
    if (from_type == KING) {
      p->kloc[from_color] = to_sq;
    }
    if (to_type == KING) {
      p->kloc[to_color] = from_sq;
    }

  } else {  // rotation
    // remove from_piece from from_sq in hash
    p->key ^= zob[from_sq][from_piece];
    set_ori(&from_piece, rot + ori_of(from_piece));  // rotate from_piece
    if (from_type == PAWN) {
      for (int i = 0; i < HALF_NUM_PAWNS; i++) {
        if (p->ploc[from_color][i] == from_sq) {
          p->p_piece[from_color][i] = from_piece;
          break;
        }
      }
    } else {
      p->k_piece[from_color] = from_piece;
    }
    p->key ^= zob[from_sq][from_piece];              // ... and in hash
  }

  tbassert(p->key == compute_zob_key(p),
           "p->key: %"PRIu64", zob-key: %"PRIu64"\n",
           p->key, compute_zob_key(p));

  WHEN_DEBUG_VERBOSE({
      fprintf(stderr, "After:\n");
      display(p);
    });

  return stomped_dst_sq;
}


// returns square of piece to be removed from board or 0
square_t fire(position_t *p) {
  color_t fake_color_to_move = (color_to_move_of(p) == WHITE) ? BLACK : WHITE;
  square_t sq = p->kloc[fake_color_to_move];
  int bdir = ori_of(p->k_piece[fake_color_to_move]);

  tbassert(ptype_of(get_piece(p, p->kloc[fake_color_to_move])) == KING,
           "ptype_of(get_piece(p, p->kloc[fake_color_to_move])): %d\n",
           ptype_of(get_piece(p, p->kloc[fake_color_to_move])));

  while (true) {
    sq += beam_of(bdir);
    tbassert(sq < ARR_SIZE && sq >= 0, "sq: %d\n", sq);

    switch (ptype_of(get_piece(p, sq))) {
      case EMPTY:  // empty square
        break;
      case PAWN:  // Pawn
        bdir = reflect_of(bdir, ori_of(get_piece(p, sq)));
        if (bdir < 0) {  // Hit back of Pawn
          return sq;
        }
        break;
      case KING:  // King
        return sq;  // sorry, game over my friend!
        break;
      case INVALID:  // Ran off edge of board
        return 0;
        break;
      default:  // Shouldna happen, man!
        tbassert(false, "Like porkchops and whipped cream.\n");
        break;
    }
  }
}


// return victim pieces or KO
victims_t make_move(position_t *old, position_t *p, move_t mv) {
  tbassert(mv != 0, "mv was zero.\n");

  WHEN_DEBUG_VERBOSE(char buf[MAX_CHARS_IN_MOVE]);

  // move phase 1 - moving a piece, which may result in a stomp
  square_t stomped_sq = low_level_make_move(old, p, mv);

  WHEN_DEBUG_VERBOSE({
      if (stomped_sq != 0) {
        square_to_str(stomped_sq, buf, MAX_CHARS_IN_MOVE);
        DEBUG_LOG(1, "Stomping piece on %s\n", buf);
      }
    });

  if (stomped_sq == 0) {
    p->victims.stomped = 0;

    // Don't check for Ko yet.

  } else {  // we definitely stomped something
    p->victims.stomped = get_piece(p, stomped_sq);

    p->key ^= zob[stomped_sq][p->victims.stomped];   // remove from board
    color_t color = color_to_move_of(p);
    piece_t stomped_piece = 0;
    for (int i = 0; i < HALF_NUM_PAWNS; i++) {
      if (stomped_sq == p->ploc[color][i]) {
        stomped_piece = p->p_piece[color][i];
        p->ploc[color][i] = 0;
        p->p_piece[color][i] = 0;
        break;
      }
    }
    p->key ^= zob[stomped_sq][stomped_piece];

    tbassert(p->key == compute_zob_key(p),
             "p->key: %"PRIu64", zob-key: %"PRIu64"\n",
             p->key, compute_zob_key(p));

    WHEN_DEBUG_VERBOSE({
        square_to_str(stomped_sq, buf, MAX_CHARS_IN_MOVE);
        DEBUG_LOG(1, "Stomped piece on %s\n", buf);
      });
  }

  // move phase 2 - shooting the laser
  square_t victim_sq = fire(p);

  WHEN_DEBUG_VERBOSE({
      if (victim_sq != 0) {
        square_to_str(victim_sq, buf, MAX_CHARS_IN_MOVE);
        DEBUG_LOG(1, "Zapping piece on %s\n", buf);
      }
    });
  if (victim_sq == 0) {
    p->victims.zapped = 0;

    if (USE_KO &&  // Ko rule
        zero_victims(p->victims) &&
        (p->key == (old->key ^ zob_color))) {
      return KO();
    }
  } else {  // we definitely hit something with laser
    p->victims.zapped = get_piece(p, victim_sq);
    p->key ^= zob[victim_sq][p->victims.zapped];   // remove from board
    remove_piece(p, victim_sq);

    p->key ^= zob[victim_sq][0];

    tbassert(p->key == compute_zob_key(p),
             "p->key: %"PRIu64", zob-key: %"PRIu64"\n",
             p->key, compute_zob_key(p));

    WHEN_DEBUG_VERBOSE({
        square_to_str(victim_sq, buf, MAX_CHARS_IN_MOVE);
        DEBUG_LOG(1, "Zapped piece on %s\n", buf);
      });
  }
  tbassert(check_piece_lists(p), "lists are off\n");
  return p->victims;
}

// help to verify the move generator
void do_perft(position_t *gme, int depth, int ply) {

}

void display(position_t *p) {
  char buf[MAX_CHARS_IN_MOVE];

  printf("\ninfo Ply: %d\n", p->ply);
  printf("info Color to move: %s\n", color_to_str(color_to_move_of(p)));

  square_to_str(p->kloc[WHITE], buf, MAX_CHARS_IN_MOVE);
  printf("info White King: %s, ", buf);
  square_to_str(p->kloc[BLACK], buf, MAX_CHARS_IN_MOVE);
  printf("info Black King: %s\n", buf);

  if (p->last_move != 0) {
    move_to_str(p->last_move, buf, MAX_CHARS_IN_MOVE);
    printf("info Last move: %s\n", buf);
  } else {
    printf("info Last move: NULL\n");
  }

  for (rnk_t r = BOARD_WIDTH - 1; r >=0 ; --r) {
    printf("\ninfo %1d  ", r);
    for (fil_t f = 0; f < BOARD_WIDTH; ++f) {
      square_t sq = square_of(f, r);

      tbassert(ptype_of(get_piece(p, sq)) != INVALID,
               "ptype_of(get_piece(p, sq)): %d\n", ptype_of(get_piece(p, sq)));
      /*if (p->blocked[sq]) {
        printf(" xx");
        continue;
      }*/
      if (ptype_of(get_piece(p, sq)) == EMPTY) {       // empty square
        printf(" --");
        continue;
      }

      int ori = ori_of(get_piece(p, sq));  // orientation
      color_t c = color_of(get_piece(p, sq));

      if (ptype_of(get_piece(p, sq)) == KING) {
        printf(" %2s", king_ori_to_rep[c][ori]);
        continue;
      }

      if (ptype_of(get_piece(p, sq)) == PAWN) {
        printf(" %2s", pawn_ori_to_rep[c][ori]);
        continue;
      }
    }
  }

  printf("\n\ninfo    ");
  for (fil_t f = 0; f < BOARD_WIDTH; ++f) {
    printf(" %c ", 'a'+f);
  }
  printf("\n\n");
}

victims_t KO() {
  return ((victims_t) {KO_STOMPED, KO_ZAPPED});
}

victims_t ILLEGAL() {
  return ((victims_t) {ILLEGAL_STOMPED, ILLEGAL_ZAPPED});
}

bool is_KO(victims_t victims) {
  return (victims.stomped == KO_STOMPED) ||
      (victims.zapped == KO_ZAPPED);
}

bool is_ILLEGAL(victims_t victims) {
  return (victims.stomped == ILLEGAL_STOMPED) ||
      (victims.zapped == ILLEGAL_ZAPPED);
}

bool zero_victims(victims_t victims) {
  return (victims.stomped == 0) &&
      (victims.zapped == 0);
}

bool victim_exists(victims_t victims) {
  return (victims.stomped > 0) ||
      (victims.zapped > 0);
}
