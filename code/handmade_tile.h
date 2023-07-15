#if !defined(HANDMADE_TILE_H)

enum Direction {
    DIR_DOWN,
    DIR_LEFT,
    DIR_UP,
    DIR_RIGHT,
};

struct Tilemap_Position {
    // @Note Fixed point tile location.
    // The high 24 bits are the tile chunk index.
    // The low 8 bits are the tile position in the chunk.
    u32 x;
    u32 y;
    u32 z;
    // Tile related offset.
    Vector2 tile_offset;
};

//@Todo Rename it to Tile_Chunk_Position?
struct Raw_Chunk_Position {
    u32 chunk_x;
    u32 chunk_y;
    u32 chunk_z;

    u32 tile_x;
    u32 tile_y;
};

struct Tile_Chunk {
    u32* tiles;
};

struct Tilemap {
    u32 chunk_shift;
    u32 chunk_mask;
    real32 tile_side_in_meters;
    u32 chunk_dim;

    u32 count_x;
    u32 count_y;
    u32 count_z;

    Tile_Chunk* tile_chunks;
};

#define HANDMADE_TILE_H
#endif