#include "handmade.h"
#include "handmade_tile.h"

void canonicalize_coord(Tilemap* tilemap, u32* coord, real32* offset) {
    s32 tile_offset = round_real32_to_s32(*offset / tilemap->tile_side_in_meters);
    *coord += tile_offset;
    *offset -= (real32)tile_offset * tilemap->tile_side_in_meters;
}

void canonicalize_position(Tilemap* tilemap, Tilemap_Position* pos) {
    canonicalize_coord(tilemap, &pos->x, &pos->offset_x);
    canonicalize_coord(tilemap, &pos->y, &pos->offset_y);
}

Tile_Chunk* get_tile_chunk(Tilemap* tilemap, u32 x, u32 y, u32 z) {
    Tile_Chunk* result = NULL;
    if (x < tilemap->count_x
        && y < tilemap->count_y
        && z < tilemap->count_z) {
        result = &tilemap->tile_chunks[z * tilemap->count_y * tilemap->count_x + y * tilemap->count_x + x];        
    }
    return result;
}

Raw_Chunk_Position get_raw_chunk_pos(Tilemap* tilemap, u32 x, u32 y, u32 z) {
    Raw_Chunk_Position result;

    result.chunk_x = x >> tilemap->chunk_shift;
    result.chunk_y = y >> tilemap->chunk_shift;
    result.chunk_z = z; // @Temporary

    result.tile_x = x & tilemap->chunk_mask;
    result.tile_y = y & tilemap->chunk_mask;

    return result;
}

void set_tile_value(Memory_Arena* memory_arena,  Tilemap* tilemap, u32 x, u32 y, u32 z, u32 value) {
    Raw_Chunk_Position raw = get_raw_chunk_pos(tilemap, x, y, z);
    Tile_Chunk* chunk = get_tile_chunk(tilemap, raw.chunk_x, raw.chunk_y, raw.chunk_z);

    if(!chunk->tiles) {
        u32 count = tilemap->chunk_dim * tilemap->chunk_dim;
        chunk->tiles = PUSH_ARRAY(memory_arena, count, u32);

        for(u32 idx = 0; idx < count; idx++) {
            chunk->tiles[idx] = 1;
        }
    }

    chunk->tiles[raw.tile_y * tilemap->chunk_dim + raw.tile_x] = value;
}

u32 get_tile_value(Tilemap* tilemap, u32 x, u32 y, u32 z) {
    u32 result = 0; // @Temproray
    Raw_Chunk_Position raw = get_raw_chunk_pos(tilemap, x, y, z);
    Tile_Chunk* chunk = get_tile_chunk(tilemap, raw.chunk_x, raw.chunk_y, raw.chunk_z);

    if (chunk && chunk->tiles) {
        if (raw.tile_x < tilemap->chunk_dim && raw.tile_y < tilemap->chunk_dim) {
            result = chunk->tiles[raw.tile_y * tilemap->chunk_dim + raw.tile_x];
        }
    }
    return result;
}

Raw_Chunk_Position get_raw_chunk_pos_for(Tilemap* tilemap, Tilemap_Position* pos) {
    Raw_Chunk_Position result;

    result.chunk_x = pos->x >> tilemap->chunk_shift;
    result.chunk_y = pos->y >> tilemap->chunk_shift;
    result.chunk_z = pos->z;

    result.tile_x = pos->x & tilemap->chunk_mask;
    result.tile_y = pos->y & tilemap->chunk_mask;
    

    return result;
}

bool is_point_empty(Tilemap* tilemap, Tilemap_Position* world_pos) {
    bool result = false;
    u32 value = get_tile_value(tilemap, world_pos->x, world_pos->y, world_pos->z);
    if (value == 1 || value == 3) { // @Temproray
        result = true;
    }
    return result;
}

bool same_position(Tilemap_Position a, Tilemap_Position b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}