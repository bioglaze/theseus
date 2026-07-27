#ifndef GAME_H
#define GAME_H

inline const char* EntityTypeToString( int type )
{
    if (type == 0) return "none";
    if (type == 1) return "door";
    if (type == 2) return "button";
    if (type == 3) return "start";
    return "none";
}

constexpr unsigned EntityNone = 0;
constexpr unsigned EntityDoor = 1;
constexpr unsigned EntityButton = 2;
constexpr unsigned EntityStart = 3;

constexpr unsigned MaxDoorInputs = 3;

#endif
