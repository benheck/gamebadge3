#ifndef ENUMS_H
#define ENUMS_H
#pragma once

enum MobPatterns {
    MP_01,  // Straight line
    MP_02,  // Straight line, then shift down 16px at halfway point
    MP_03,  // Straight line, then shift up 16px at halfway point
    MP_04,  // At 2/3, half circle up and back out the right
    MP_05,  // Wave
};

enum GameState {
    GS_TITLE,
    GS_GAME,
    GS_DEBUG,
    GS_GAMEOVER,
    GS_DEATH
};

#endif
