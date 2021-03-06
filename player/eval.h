// Copyright (c) 2015 MIT License by 6.172 Staff

#ifndef EVAL_H
#define EVAL_H

#include <stdbool.h>

#include "./search.h"
#include "./move_gen.h"

#define EV_SCORE_RATIO 100   // Ratio of ev_score_t values to score_t values

// ev_score_t values
#define PAWN_EV_VALUE (PAWN_VALUE*EV_SCORE_RATIO)

score_t eval(position_t *p, bool verbose);
void init_eval();
#endif  // EVAL_H
