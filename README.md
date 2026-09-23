# C++ / C: The Shattered Depths

A turn-based terminal roguelike built from scratch with **C++ gameplay systems** and a small **C utility layer**.

## Features

- Procedurally generated multi-room dungeon
- Fog of war and explored-map rendering
- Player progression, XP, levels, stats and critical hits
- Multiple enemy types with different behavior
- Melee and ranged combat
- Loot, equipment, consumables and inventory
- Crafting
- Three-part quest chain
- Treasure, traps and healing shrines
- Dungeon stairs and escalating floors
- Save/load
- Deterministic seed support for reproducible runs
- No third-party libraries required

## Build on Ubuntu

```bash
sudo apt install build-essential cmake
make
./shattered_depths
```

Or:

```bash
cmake -S . -B build
cmake --build build -j
./build/shattered_depths
```

## Controls

The game is command driven so it works over SSH and in a normal terminal.

- `w a s d` - move
- `f` - attack the nearest visible enemy
- `g` - pick up nearby loot
- `i` - inventory
- `e` - equip/use an item
- `c` - craft a medkit from scrap
- `q` - quest status
- `>` - descend when standing on stairs
- `S` - save
- `L` - load
- `?` - help
- `x` - exit

Every movement/action advances the world by one turn.

## Goal

Descend through the dungeon, complete the three-part quest, gather equipment, and defeat the **Depth Warden** on the final floor.

## Project layout

- `src/main.cpp` - game engine, world generation, rendering, AI, combat and persistence
- `src/rng.c` / `include/rng.h` - C random-number and utility layer
- `CMakeLists.txt` - CMake build
- `Makefile` - simple Ubuntu build

The project intentionally avoids ncurses/SDL so the game remains easy to build and modify.
