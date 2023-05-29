#include "handmade.h"
#include "handmade_tile.h"

void canonicalize_coord(Tilemap* tilemap, u32* coord, real32* offset) {
    s32 tile_offset = floor_real32_to_s32(*offset / tilemap->tile_side_in_meters);
    *coord += tile_offset;
    *offset -= (real32)tile_offset * tilemap->tile_side_in_meters;
}

void canonicalize_position(Tilemap* tilemap, Tilemap_Position* pos) {
    canonicalize_coord(tilemap, &pos->x, &pos->offset_x);
    canonicalize_coord(tilemap, &pos->y, &pos->offset_y);
}

Tile_Chunk* get_tile_chunk(Tilemap* tilemap, u32 x, u32 y) {
    Tile_Chunk* result = NULL;
    if (x < tilemap->num_world_cols && y < tilemap->num_world_rows) {
        result = &tilemap->tile_chunks[y * tilemap->num_world_cols + y];        
    }
    return result;
}

Raw_Chunk_Position get_raw_chunk_pos(Tilemap* tilemap, u32 x, u32 y) {
    Raw_Chunk_Position result;

    result.chunk_x = x >> tilemap->chunk_shift;
    result.chunk_y = y >> tilemap->chunk_shift;

    result.tile_x = x & tilemap->chunk_mask;
    result.tile_y = y & tilemap->chunk_mask;

    return result;
}

void set_tile_value(Tilemap* tilemap, u32 x, u32 y, u32 value) {
    Raw_Chunk_Position raw = get_raw_chunk_pos(tilemap, x, y);
    Tile_Chunk* chunk = get_tile_chunk(tilemap, raw.chunk_x, raw.chunk_y);
    chunk->tiles[raw.tile_x * tilemap->chunk_dim + x] = value;
}

u32 get_tile_value(Tilemap* tilemap, Tile_Chunk* chunk, u32 x, u32 y) {
    u32 val = chunk->tiles[y * tilemap->chunk_dim + x];
    return val;
}

Raw_Chunk_Position get_raw_chunk_pos_for(Tilemap* tilemap, Tilemap_Position* pos) {
    Raw_Chunk_Position result;

    result.chunk_x = pos->x >> tilemap->chunk_shift;
    result.chunk_y = pos->y >> tilemap->chunk_shift;

    result.tile_x = pos->x & tilemap->chunk_mask;
    result.tile_y = pos->y & tilemap->chunk_mask;

    return result;
}

bool is_point_empty(Tilemap* tilemap, Tilemap_Position* world_pos) {
    Raw_Chunk_Position tile_pos = get_raw_chunk_pos_for(tilemap, world_pos);
    Tile_Chunk* tile_chunk = get_tile_chunk(tilemap, tile_pos.chunk_x, tile_pos.chunk_y);

    if (tile_chunk) {
        // @Note Since we use unsigned value to represent the tile_x and tile_y, there's no need to check it's negative.
        if (tile_pos.tile_x >= tilemap->num_tilemap_cols || tile_pos.tile_y >= tilemap->num_tilemap_rows) {
            return false;
        }

        u32 val = get_tile_value(tilemap, tile_chunk, tile_pos.tile_x, tile_pos.tile_y);
        
        if (val != 1) {
            return true;
        }
    }

    return false;
}
