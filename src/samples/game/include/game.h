#ifndef GAME_H
#define GAME_H

constexpr unsigned EntityNone = 0;
constexpr unsigned EntityDoor = 1;
constexpr unsigned EntityButton = 2;
constexpr unsigned EntityStart = 3;

inline const char* EntityTypeToString( unsigned type )
{
    if (type == EntityNone)   return "none";
    if (type == EntityDoor)   return "door";
    if (type == EntityButton) return "button";
    if (type == EntityStart)  return "start";
    return "none";
}

constexpr unsigned MaxDoorInputs = 3;

#endif
