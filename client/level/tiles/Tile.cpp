#include "Tile.hpp"

namespace client {
namespace tile {

byte render(byte id, int ticks) {
    int animticks = ticks % 240;
    switch (id) {
    case GRASS:
        return 0;
    case FLOWER:
        return 1;
    case WATER:
        if (animticks >= 60 && animticks < 120) {
            return 3;
        } else if (animticks >= 120 && animticks < 180) {
            return 4;
        } else if (animticks >= 180 && animticks < 240) {
            return 3;
        }
        return 2;
    }
    return 0;
}

} // namespace tile
} // namespace client
