#ifndef STRUCTS_H
#define STRUCTS_H
#pragma once

#define DEAD        0
#define ALIVE       1
#define DYING       2

#define UFO         0
#define ASTEROID    1
#define CANNON      2

#define ACTIVE      0
#define SPENT       1

#define STRAIGHT    0

typedef struct player_typ
{
    int x, y;               // Ship's x and y position
    int speed_x, speed_y;   // Ship's speed
    int lives;              // Amount of lives remaining
    int state;              // Player's state - ALIVE, DEAD, or DYING
    int score;              // The player's score
    int bullet_index;       // Keeps track of which bullet # the player is on
} player, *player_ptr;

typedef struct enemy_typ
{
    int type;               // Enemy type - UFO, ASTEROID, CANNON
    int x, y;               // Enemy's x and y position
    int hp;                 // Amount of hitpoints the enemy has
    int points;             // Points the enemy is worth
    int speed_x, speed_y;   // Enemy's speed
    int state;              // Enemy's state - ALIVE, DEAD, or DYING
    int path;               // The flight path of the enemy
} enemy, *enemy_ptr;

typedef struct missle_typ
{
    int x, y;               // location of the missle
    int speed_x, speed_y;   // Missle's speed
    int state;              // Missle state - ACTIVE or SPENT
} missle, *missle_ptr;

#endif