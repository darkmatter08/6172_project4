// Copyright (c) 2015 MIT License by 6.172 Staff

#include "./eval.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "./tbassert.h"

// -----------------------------------------------------------------------------
// Evaluation
// -----------------------------------------------------------------------------

typedef int32_t ev_score_t;  // Static evaluator uses "hi res" values

int RANDOMIZE;

int PCENTRAL;
int HATTACK;
int PBETWEEN;
int PCENTRAL;
int KFACE;
int KAGGRESSIVE;
int MOBILITY;
int PAWNPIN;

// Heuristics for static evaluation - described in the google doc
// mentioned in the handout.



char laser_map[2][ARR_SIZE];

float h_dist_lookup[ARR_SIZE][ARR_SIZE];
ev_score_t pcentral_lookup[BOARD_WIDTH][BOARD_WIDTH];


ev_score_t pcentral_calc(fil_t f, rnk_t r) {
  double df = BOARD_WIDTH/2 - f - 1;
  if (df < 0)  df = f - BOARD_WIDTH/2;
  double dr = BOARD_WIDTH/2 - r - 1;
  if (dr < 0) dr = r - BOARD_WIDTH/2;
  double bonus = 1 - sqrt(df * df + dr * dr) / (BOARD_WIDTH / sqrt(2));
  return PCENTRAL * bonus;
}


// setup lookup tables, etc
void init_eval() {
  // set all squares to laser_map to be INVALID value.
  // board squares set to 0 every time eval is called
  for (int i = 0; i < ARR_SIZE; ++i) {
    laser_map[BLACK][i] = 4;   // Invalid square
    laser_map[WHITE][i] = 4;   // Invalid square
  }
  for (fil_t fa = 0; fa < BOARD_WIDTH; fa++) {
    square_t sq = (FIL_ORIGIN + fa) * ARR_WIDTH + RNK_ORIGIN;
    for (rnk_t ra = 0; ra < BOARD_WIDTH; ra++, sq++) {
      pcentral_lookup[fa][ra] = pcentral_calc(fa, ra);
      for (fil_t fb = 0; fb < BOARD_WIDTH; fb++) {
        square_t b = (FIL_ORIGIN + fb) * ARR_WIDTH + RNK_ORIGIN;
        for (rnk_t rb = 0; rb < BOARD_WIDTH; rb++, b++) {
          int delta_fil = abs(fa - fb);
          int delta_rnk = abs(ra - rb);
          float x = (1.0 / (delta_fil + 1)) + (1.0 / (delta_rnk + 1));
          h_dist_lookup[sq][b] = x;
        }
      }
    }
  }
}

// PCENTRAL heuristic: Bonus for Pawn near center of board
ev_score_t pcentral(fil_t f, rnk_t r) {
  tbassert(pcentral_lookup[f][r] == pcentral_calc(f, r), "foo");
  return pcentral_lookup[f][r];
}

// returns true if c lies on or between a and b, which are not ordered
bool between(int c, int a, int b) {
  bool x = ((c >= a) && (c <= b)) || ((c <= a) && (c >= b));
  return x;
}

// PBETWEEN heuristic: Bonus for Pawn at (f, r) in rectangle defined by Kings at the corners
ev_score_t pbetween(position_t *p, fil_t f, rnk_t r) {
  bool is_between =
      between(f, fil_of(p->kloc[WHITE]), fil_of(p->kloc[BLACK])) &&
      between(r, rnk_of(p->kloc[WHITE]), rnk_of(p->kloc[BLACK]));
  return is_between ? PBETWEEN : 0;
}


// KFACE heuristic: bonus (or penalty) for King facing toward the other King
ev_score_t kface(position_t *p, fil_t f, rnk_t r) {
  square_t sq = square_of(f, r);
  piece_t x = p->board[sq];
  color_t c = color_of(x);
  square_t opp_sq = p->kloc[opp_color(c)];
  int delta_fil = fil_of(opp_sq) - f;
  int delta_rnk = rnk_of(opp_sq) - r;
  int bonus;

  switch (ori_of(x)) {
    case NN:
      bonus = delta_rnk;
      break;

    case EE:
      bonus = delta_fil;
      break;

    case SS:
      bonus = -delta_rnk;
      break;

    case WW:
      bonus = -delta_fil;
      break;

    default:
      bonus = 0;
      tbassert(false, "Illegal King orientation.\n");
  }

  return (bonus * KFACE) / (abs(delta_rnk) + abs(delta_fil));
}


// KFACE heuristic: bonus (or penalty) for King facing toward the other King
ev_score_t kface_opt(position_t *p, square_t k, int delta_fil, int delta_rnk) {
  piece_t x = p->board[k];
  int bonus;

  switch (ori_of(x)) {
    case NN:
      bonus = delta_rnk;
      break;

    case EE:
      bonus = delta_fil;
      break;

    case SS:
      bonus = -delta_rnk;
      break;

    case WW:
      bonus = -delta_fil;
      break;

    default:
      bonus = 0;
      tbassert(false, "Illegal King orientation.\n");
  }

  return (bonus * KFACE) / (abs(delta_rnk) + abs(delta_fil));
}

// KAGGRESSIVE heuristic: bonus for King with more space to back
ev_score_t kaggressive(position_t *p, fil_t f, rnk_t r, square_t sq, color_t c) {
  square_t opp_sq = p->kloc[opp_color(c)];
  fil_t of = fil_of(opp_sq);
  rnk_t _or = (rnk_t) rnk_of(opp_sq);

  int delta_fil = of - f;
  int delta_rnk = _or - r;

  int bonus = 0;

  if (delta_fil >= 0 && delta_rnk >= 0) {
    bonus = (f + 1) * (r + 1);
  } else if (delta_fil <= 0 && delta_rnk >= 0) {
    bonus = (BOARD_WIDTH - f) * (r + 1);
  } else if (delta_fil <= 0 && delta_rnk <= 0) {
    bonus = (BOARD_WIDTH - f) * (BOARD_WIDTH - r);
  } else if (delta_fil >= 0 && delta_rnk <= 0) {
    bonus = (f + 1) * (BOARD_WIDTH - r);
  }

  return (KAGGRESSIVE * bonus) / (BOARD_WIDTH * BOARD_WIDTH);
}

// KAGGRESSIVE heuristic: bonus for King with more space to back
ev_score_t kaggressive_opt(position_t *p, fil_t f, rnk_t r, int delta_fil, int delta_rnk) {
  int bonus = 0;

  if (delta_fil >= 0 && delta_rnk >= 0) {
    bonus = (f + 1) * (r + 1);
  } else if (delta_fil <= 0 && delta_rnk >= 0) {
    bonus = (BOARD_WIDTH - f) * (r + 1);
  } else if (delta_fil <= 0 && delta_rnk <= 0) {
    bonus = (BOARD_WIDTH - f) * (BOARD_WIDTH - r);
  } else if (delta_fil >= 0 && delta_rnk <= 0) {
    bonus = (f + 1) * (BOARD_WIDTH - r);
  }

  return (KAGGRESSIVE * bonus) / (BOARD_WIDTH * BOARD_WIDTH);
}

// Marks the path of the laser until it hits a piece or goes off the board.
//
// p : current board state
// laser_map : end result will be stored here. Every square on the
//             path of the laser is marked with mark_mask
// c : color of king shooting laser
// mark_mask: what each square is marked with
void mark_laser_path(position_t *p, char *laser_map, color_t c) {

  // Fire laser, recording in laser_map
  square_t sq = p->kloc[c];
  int bdir = ori_of(p->board[sq]);

  tbassert(ptype_of(p->board[sq]) == KING,
           "ptype: %d\n", ptype_of(p->board[sq]));
  laser_map[sq] |= 1;

  while (true) {
    sq += beam_of(bdir);
    laser_map[sq] |= 1;
    tbassert(sq < ARR_SIZE && sq >= 0, "sq: %d\n", sq);

    switch (ptype_of(p->board[sq])) {
      case EMPTY:  // empty square
        break;
      case PAWN:  // Pawn
        bdir = reflect_of(bdir, ori_of(p->board[sq]));
        if (bdir < 0) {  // Hit back of Pawn
          return;
        }
        break;
      case KING:  // King
        return;  // sorry, game over my friend!
        break;
      case INVALID:  // Ran off edge of board
        return;
        break;
      default:  // Shouldna happen, man!
        tbassert(false, "Not cool, man.  Not cool.\n");
        break;
    }
  }
}

// PAWNPIN Heuristic: count number of pawns that are pinned by the
//   opposing king's laser --- and are thus immobile.

int pawnpin(position_t *p, color_t color, char opposite_color_laser_map[ARR_SIZE]) {
  int pinned_pawns = 0;

  // Figure out which pawns are not pinned down by the laser.
  for (int i = 0; i < NUM_PAWNS; i++) {

    square_t sq = p->ploc[i];
    if (sq == 0) {
      continue;
    }
    if (opposite_color_laser_map[sq] == 0 &&
        color_of(p->board[sq]) == color) {
      pinned_pawns += 1;
    }
  }

  return pinned_pawns;
}

// MOBILITY heuristic: safe squares around king of color color.
int mobility(position_t *p, color_t color, char opposite_color_laser_map[ARR_SIZE]) {
  int mobility = 0;
  square_t king_sq = p->kloc[color];
  tbassert(ptype_of(p->board[king_sq]) == KING,
           "ptype: %d\n", ptype_of(p->board[king_sq]));
  tbassert(color_of(p->board[king_sq]) == color,
           "color: %d\n", color_of(p->board[king_sq]));

  if (opposite_color_laser_map[king_sq] == 0) {
    mobility++;
  }
  for (int d = 0; d < 8; ++d) {
    square_t sq = king_sq + dir_of(d);
    if (opposite_color_laser_map[sq] == 0) {
      mobility++;
    }
  }
  return mobility;
}

// MOBILITY heuristic: safe squares around king of color color.
int mobility_opt(position_t *p, square_t king_sq, char opposite_color_laser_map[ARR_SIZE]) {
  int mobility = 0;
  if (opposite_color_laser_map[king_sq] == 0) {
    mobility++;
  }
  for (int d = 0; d < 8; ++d) {
    square_t sq = king_sq + dir_of(d);
    if (opposite_color_laser_map[sq] == 0) {
      mobility++;
    }
  }
  return mobility;
}

// Harmonic-ish distance: 1/(|dx|+1) + 1/(|dy|+1)
float h_dist(square_t sq, square_t b) {
  return h_dist_lookup[sq][b];
}

// H_SQUARES_ATTACKABLE heuristic: for shooting the enemy king
int h_squares_attackable(position_t *p, color_t c, char laser_map[ARR_SIZE]) {
  square_t o_king_sq = p->kloc[opp_color(c)];
  tbassert(ptype_of(p->board[o_king_sq]) == KING,
           "ptype: %d\n", ptype_of(p->board[o_king_sq]));
  tbassert(color_of(p->board[o_king_sq]) != c,
           "color: %d\n", color_of(p->board[o_king_sq]));

  float h_attackable = 0;
  for (fil_t f = 0; f < BOARD_WIDTH; f++) {
    square_t sq = (FIL_ORIGIN + f) * ARR_WIDTH + RNK_ORIGIN;
    for (rnk_t r = 0; r < BOARD_WIDTH; r++, sq++) {
      if (laser_map[sq] != 0) {
        h_attackable += h_dist(sq, o_king_sq);
      }
    }
  }
  return h_attackable;
}

// H_SQUARES_ATTACKABLE heuristic: for shooting the enemy king
int h_squares_attackable_opt(position_t *p, square_t o_king_sq, color_t c) {
  float h_attackable = 0;
  position_t np = *p;

  // Fire laser, recording in laser_map
  square_t sq = np.kloc[c];
  int bdir = ori_of(np.board[sq]);
  h_attackable += h_dist(sq, o_king_sq);
  laser_map[c][sq] |= 1;

  while (true) {
    sq += beam_of(bdir);
    laser_map[c][sq] |= 1;
    tbassert(sq < ARR_SIZE && sq >= 0, "sq: %d\n", sq);

    switch (ptype_of(p->board[sq])) {
      case EMPTY:  // empty square
        h_attackable += h_dist(sq, o_king_sq);
        break;
      case PAWN:  // Pawn
        bdir = reflect_of(bdir, ori_of(p->board[sq]));
        h_attackable += h_dist(sq, o_king_sq);
        if (bdir < 0) {  // Hit back of Pawn
          return h_attackable;
        }
        break;
      case KING:  // King
        h_attackable += h_dist(sq, o_king_sq);
        return h_attackable;
        break;
      case INVALID:  // Ran off edge of board
        return h_attackable;
        break;
      default:  // Shouldna happen, man!
        tbassert(false, "Not cool, man.  Not cool.\n");
        break;
    }
  }
}

// Static evaluation.  Returns score
score_t eval(position_t *p, bool verbose) {
  tbassert(check_position_integrity(p), "pawn positions incorrect");
  tbassert(check_pawn_counts(p), "pawn counts are off");
  // seed rand_r with a value of 1, as per
  // http://linux.die.net/man/3/rand_r
  static __thread unsigned int seed = 1;
  // verbose = true: print out components of score
  ev_score_t score[2] = { 0, 0 };
  //  int corner[2][2] = { {INF, INF}, {INF, INF} };
  ev_score_t bonus;
  char buf[MAX_CHARS_IN_MOVE];

  for (int i = 0; i < NUM_PAWNS; i++) {
    square_t sq = p->ploc[i];
    if (sq == 0) {
      continue;
    }
    piece_t x = p->board[sq];
    color_t c = color_of(x);
    bonus = PAWN_EV_VALUE;
    if (verbose) {
      printf("MATERIAL bonus %d for %s Pawn on %s\n", bonus, color_to_str(c), buf);
    }
    score[c] += bonus;

    // PBETWEEN heuristic
    fil_t f = fil_of(sq);
    rnk_t r = rnk_of(sq);
    bonus = pbetween(p, f, r);
    if (verbose) {
      printf("PBETWEEN bonus %d for %s Pawn on %s\n", bonus, color_to_str(c), buf);
    }
    score[c] += bonus;

    // PCENTRAL heuristic
    bonus = pcentral(f, r);
    if (verbose) {
      printf("PCENTRAL bonus %d for %s Pawn on %s\n", bonus, color_to_str(c), buf);
    }
    score[c] += bonus;
  }

  // grab values that are used in many heuristic functions. Saves work in other methods
  square_t wk = p->kloc[WHITE];
  fil_t wk_f = fil_of(wk);
  rnk_t wk_r = rnk_of(wk);

  square_t bk = p->kloc[BLACK];
  fil_t bk_f = fil_of(bk);
  rnk_t bk_r = rnk_of(bk);

  // distances between kings in rank and file dimensions
  int delta_fil_w = wk_f - bk_f;
  int delta_rnk_w = wk_r - bk_r;
  int delta_fil_b = bk_f - wk_f;
  int delta_rnk_b = bk_r - wk_r;
  tbassert(kface(p, fil_of(wk), rnk_of(wk)) ==
           kface_opt(p, wk, delta_fil_b, delta_rnk_b),
           "kface does not match white\n");
  tbassert(kface(p, fil_of(bk), rnk_of(bk)) ==
           kface_opt(p, bk, delta_fil_w, delta_rnk_w),
           "kface does not match black\n");

  score[WHITE] += kface_opt(p, wk, delta_fil_b, delta_rnk_b);
  score[BLACK] += kface_opt(p, bk, delta_fil_w, delta_rnk_w);

  tbassert(kaggressive(p, wk_f, wk_r, wk, WHITE)
           == kaggressive_opt(p, wk_f, wk_r, delta_fil_b, delta_rnk_b),
           "kaggressive does not match white\n");
  tbassert(kaggressive(p, bk_f, bk_r, bk, BLACK) ==
           kaggressive_opt(p, bk_f, bk_r, delta_fil_w, delta_rnk_w),
           "kaggressive does not match white\n");

  score[WHITE] += kaggressive_opt(p, wk_f, wk_r, delta_fil_b, delta_rnk_b);
  score[BLACK] += kaggressive_opt(p, bk_f, bk_r, delta_fil_w, delta_rnk_w);

  for (color_t c = 0; c < 2; c++) {
    for (int i = 0; i < BOARD_WIDTH; i++) {
      int k = (FIL_ORIGIN + i) * ARR_WIDTH + RNK_ORIGIN;
      for (int j = 0; j < BOARD_WIDTH; j++, k++) {
        laser_map[c][k] = 0;
      }
    }
  }

  score[WHITE] += HATTACK * h_squares_attackable_opt(p, bk, WHITE);
  score[BLACK] += HATTACK * h_squares_attackable_opt(p, wk, BLACK);

  score[WHITE] += MOBILITY * mobility_opt(p, wk, laser_map[BLACK]);
  score[BLACK] += MOBILITY * mobility_opt(p, bk, laser_map[WHITE]);
  tbassert(mobility(p, WHITE, laser_map[BLACK]) ==
           mobility_opt(p, wk, laser_map[BLACK]),
           "mobility does not match white\n");
  tbassert(mobility(p, BLACK, laser_map[WHITE]) ==
           mobility_opt(p, bk, laser_map[WHITE]),
           "mobility does not match black\n");

  // PAWNPIN Heuristic --- is a pawn immobilized by the enemy laser.
  int w_pawnpin = PAWNPIN * pawnpin(p, WHITE, laser_map[BLACK]);
  score[WHITE] += w_pawnpin;
  int b_pawnpin = PAWNPIN * pawnpin(p, BLACK, laser_map[WHITE]);
  score[BLACK] += b_pawnpin;

  // score from WHITE point of view
  ev_score_t tot = score[WHITE] - score[BLACK];

  if (RANDOMIZE) {
    ev_score_t  z = rand_r(&seed) % (RANDOMIZE*2+1);
    tot = tot + z - RANDOMIZE;
  }

  if (color_to_move_of(p) == BLACK) {
    tot = -tot;
  }

  return tot / EV_SCORE_RATIO;
}
