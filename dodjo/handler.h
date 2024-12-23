#ifndef HANDLER_H
#define HANDLER_H

#include "player.h"

/// @brief Handle the interactions between the player and the items on the ground
/// @param p player
/// @param x player next x
/// @param y player next y
/// @return true if can move else return false
int handle(player* p, int x, int y);

/// @brief Check if coords are in chunk bounds
/// @param x pos x
/// @param y pos y
/// @return true if in bounds
bool is_in_box(int x, int y);

#endif