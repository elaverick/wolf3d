#ifndef WOLF3D_NATIVE_MAP_CODEC_H
#define WOLF3D_NATIVE_MAP_CODEC_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
bool wolf_native_carmack_expand(const uint8_t *source,size_t source_size,uint16_t *destination,size_t destination_size);
bool wolf_native_rlew_expand(const uint16_t *source,size_t source_words,unsigned *destination,size_t destination_words,uint16_t rlew_tag);
bool wolf_native_map_decode_plane(const uint8_t *source,size_t source_size,unsigned *destination,size_t destination_words,uint16_t rlew_tag,bool carmack_compressed);
#endif
