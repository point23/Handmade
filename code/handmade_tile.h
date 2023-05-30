#if !defined(HANDMADE_TILE_H)

struct Tilemap_Position {
    // @Note Fixed point tile location.
    // The high 24 bits are the tile chunk index.
    // The low 8 bits are the tile position in the chunk.
    u32 x;
    u32 y;

    // Tile related pos.
    real32 offset_x;
    real32 offset_y; 
};

//@Todo Rename it to Tile_Chunk_Position?
struct Raw_Chunk_Position {
    u32 chunk_x;
    u32 chunk_y;

    u32 tile_x;
    u32 tile_y;
};

struct Tile_Chunk {
    u32* tiles;
};

struct Tilemap {
    u32 chunk_shift;
    u32 chunk_mask;

    u32 num_tilemap_cols;
    u32 num_tilemap_rows;

    u32 num_world_cols;
    u32 num_world_rows;

    u32 chunk_dim;

    real32 lower_left_x;
    real32 lower_left_y;

    real32 tile_side_in_meters;
    Tile_Chunk* tile_chunks;
};

#define HANDMADE_TILE_H
#endif