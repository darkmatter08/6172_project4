// Copyright (c) 2015 MIT License by 6.172 Staff

#ifndef MOVE_GEN_H
#define MOVE_GEN_H

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include "./tbassert.h"

#define MAX_NUM_MOVES 128      // real number = 7 x (8 + 3) + 1 x (8 + 4) = 89
#define MAX_PLY_IN_SEARCH 100  // up to 100 ply
#define MAX_PLY_IN_GAME 4096   // long game!  ;^)

// Used for debugging and display
#define MAX_CHARS_IN_MOVE 16  // Could be less
#define MAX_CHARS_IN_TOKEN 64

// the board (which is 10x10) is centered in a 12x12 array
#define ARR_WIDTH 12
#define ARR_SIZE (ARR_WIDTH * ARR_WIDTH)

// board is 10 x 10
#define BOARD_WIDTH 10

typedef int square_t;
typedef int8_t rnk_t;
typedef int8_t fil_t;

#define FIL_ORIGIN ((ARR_WIDTH - BOARD_WIDTH) / 2)
#define RNK_ORIGIN ((ARR_WIDTH - BOARD_WIDTH) / 2)

#define FIL_SHIFT 4
#define FIL_MASK 15
#define RNK_SHIFT 0
#define RNK_MASK 15
#define NUM_PAWNS 14
// -----------------------------------------------------------------------------
// pieces
// -----------------------------------------------------------------------------

#define PIECE_SIZE 5  // Number of bits in (ptype: 2, color: 1, orientation: 2)

typedef int8_t piece_t;

// -----------------------------------------------------------------------------
// piece types
// -----------------------------------------------------------------------------

#define PTYPE_SHIFT 2
#define PTYPE_MASK 3

typedef enum {
  EMPTY,
  PAWN,
  KING,
  INVALID
} ptype_t;

// -----------------------------------------------------------------------------
// colors
// -----------------------------------------------------------------------------

#define COLOR_SHIFT 4
#define COLOR_MASK 1

typedef enum {
  WHITE = 0,
  BLACK
} color_t;

// -----------------------------------------------------------------------------
// Orientations
// -----------------------------------------------------------------------------

#define NUM_ORI 4
#define ORI_SHIFT 0
#define ORI_MASK (NUM_ORI - 1)

typedef enum {
  NN,
  EE,
  SS,
  WW
} king_ori_t;

typedef enum {
  NW,
  NE,
  SE,
  SW
} pawn_ori_t;

// -----------------------------------------------------------------------------
// moves
// -----------------------------------------------------------------------------

// MOVE_MASK is 20 bits
#define MOVE_MASK 0xfffff

#define PTYPE_MV_SHIFT 18
#define PTYPE_MV_MASK 3
#define FROM_SHIFT 8
#define FROM_MASK 0xFF
#define TO_SHIFT 0
#define TO_MASK 0xFF
#define ROT_SHIFT 16
#define ROT_MASK 3

typedef uint32_t move_t;
typedef uint64_t sortable_move_t;

// Rotations
typedef enum {
  NONE,
  RIGHT,
  UTURN,
  LEFT
} rot_t;

// A single move can stomp one piece and zap another.
typedef struct victims_t {
  piece_t stomped;
  piece_t zapped;
} victims_t;

// returned by make move in illegal situation
#define KO_STOMPED -1
#define KO_ZAPPED -1
// returned by make move in ko situation
#define ILLEGAL_STOMPED -1
#define ILLEGAL_ZAPPED -1

// -----------------------------------------------------------------------------
// position
// -----------------------------------------------------------------------------

typedef struct position {
  piece_t      board[ARR_SIZE];
  struct position  *history;     // history of position
  uint64_t     key;              // hash key
  uint16_t      ply;              // Even ply are White, odd are Black
  move_t       last_move;        // move that led to this position
  victims_t    victims;          // pieces destroyed by shooter or stomper
  square_t     kloc[2];          // location of kings
  square_t     ploc[NUM_PAWNS];
} position_t;

static inline color_t color_to_move_of(position_t *p) {
  if ((p->ply & 1) == 0) {
    return WHITE;
  } else {
    return BLACK;
  }
}

static inline color_t color_of(piece_t x) {
  return (color_t) ((x >> COLOR_SHIFT) & COLOR_MASK);
}

static inline void set_color(piece_t *x, color_t c) {
  tbassert((c >= 0) & (c <= COLOR_MASK), "color: %d\n", c);
  *x = ((c & COLOR_MASK) << COLOR_SHIFT) |
      (*x & ~(COLOR_MASK << COLOR_SHIFT));
}

static inline ptype_t ptype_of(piece_t x) {
  return (ptype_t) ((x >> PTYPE_SHIFT) & PTYPE_MASK);
}

static inline void set_ptype(piece_t *x, ptype_t pt) {
  *x = ((pt & PTYPE_MASK) << PTYPE_SHIFT) |
      (*x & ~(PTYPE_MASK << PTYPE_SHIFT));
}

static inline int ori_of(piece_t x) {
  return (x >> ORI_SHIFT) & ORI_MASK;
}

static inline void set_ori(piece_t *x, int ori) {
  *x = ((ori & ORI_MASK) << ORI_SHIFT) |
      (*x & ~(ORI_MASK << ORI_SHIFT));
}

static inline ptype_t ptype_mv_of(move_t mv) {
  return (ptype_t) ((mv >> PTYPE_MV_SHIFT) & PTYPE_MV_MASK);
}

static inline square_t from_square(move_t mv) {
  return (mv >> FROM_SHIFT) & FROM_MASK;
}

static inline square_t to_square(move_t mv) {
  return (mv >> TO_SHIFT) & TO_MASK;
}

static inline rot_t rot_of(move_t mv) {
  return (rot_t) ((mv >> ROT_SHIFT) & ROT_MASK);
}

static inline move_t move_of(ptype_t typ, rot_t rot, square_t from_sq, square_t to_sq) {
  return ((typ & PTYPE_MV_MASK) << PTYPE_MV_SHIFT) |
      ((rot & ROT_MASK) << ROT_SHIFT) |
      ((from_sq & FROM_MASK) << FROM_SHIFT) |
      ((to_sq & TO_MASK) << TO_SHIFT);
}

static inline color_t opp_color(color_t c) {
  if (c == WHITE) {
    return BLACK;
  } else {
    return WHITE;
  }
}

// For no square, use 0, which is guaranteed to be off board
static inline square_t square_of(fil_t f, rnk_t r) {
  square_t s = ARR_WIDTH * (FIL_ORIGIN + f) + RNK_ORIGIN + r;
  tbassert((s >= 0) && (s < ARR_SIZE), "s: %d\n", s);
  return s;
}

// Finds file of square
static inline fil_t fil_of(square_t sq) {
  fil_t f = sq / ARR_WIDTH - FIL_ORIGIN;
  return f;
}

// Finds rank of square
static inline rnk_t rnk_of(square_t sq) {
  rnk_t r = sq % ARR_WIDTH - RNK_ORIGIN;
  return r;
}


// -----------------------------------------------------------------------------
// Function prototypes
// -----------------------------------------------------------------------------

int check_position_integrity(position_t *p);
int check_pawn_counts(position_t *p);
char *color_to_str(color_t c);
void init_zob();
int square_to_str(square_t sq, char *buf, size_t bufsize);
int dir_of(int i);
int reflect_of(int beam_dir, int pawn_ori);
int beam_of(int direction);
void move_to_str(move_t mv, char *buf, size_t bufsize);
int generate_all(position_t *p, sortable_move_t *sortable_move_list,
                 bool strict);
int generate_all_opt(position_t *p, sortable_move_t *sortable_move_list,
                 bool strict);
void do_perft(position_t *gme, int depth, int ply);
square_t low_level_make_move(position_t *old, position_t *p, move_t mv);
victims_t make_move(position_t *old, position_t *p, move_t mv);
void display(position_t *p);
uint64_t compute_zob_key(position_t *p);

victims_t KO();
victims_t ILLEGAL();

bool is_ILLEGAL(victims_t victims);
bool is_KO(victims_t victims);
bool zero_victims(victims_t victims);
bool victim_exists(victims_t victims);

void mark_laser_path(position_t * restrict p, char * restrict, color_t c);

#endif  // MOVE_GEN_H
