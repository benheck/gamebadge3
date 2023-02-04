#include "GameLevel.h"


GameLevel::GameLevel()
{

}

size_t GameLevel::MapWidth()
{
    return tilemap.size() / NUM_TILE_ROWS;
}

unsigned short GameLevel::LastMapPage()
{
    return MapWidth() / NUM_TILE_COLS - 1;
}


GameLevel::~GameLevel()
{
    
}
