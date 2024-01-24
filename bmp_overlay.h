#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <thread>
#include <vector>
#include <immintrin.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int16_t s16;
typedef int32_t s32;

#define m128i __m128i


#pragma pack(push, 1)
struct bitmap_header
{
    u16 file_type; 
    u32 file_size;
    u16 reserved1;
    u16 reserved2;
    u32 bitmap_offset;
    u32 size;
    s32 width;
    s32 height;
    u16 planes;
    u16 bits_per_pixel;
    u32 compression;
    u32 size_of_bitmap;
    s32 horz_resolution;
    s32 vert_resolution;
    u32 colors_used;
    u32 colors_important;
};
#pragma pack(pop)


struct lane_v3
{
    m128i r;
    m128i g;
    m128i b;
};


lane_v3 set1_epi16(s16 a, s16 b, s16 c)
{
    lane_v3 result;
    result.r = _mm_set1_epi16(c);
    result.g = _mm_set1_epi16(b);
    result.b = _mm_set1_epi16(a);

    return result;
}


lane_v3 set_epi8_lo(u8 a, u8 b, u8 c, u8 d)
{

    lane_v3 result;
    result.r = _mm_set_epi8(
        0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff,
        0xff, d+2,  0xff, c+2,
        0xff, b+2,  0xff, a+2);
    result.g = _mm_set_epi8(
        0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff,
        0xff, d+1,  0xff, c+1,
        0xff, b+1,  0xff, a+1);
    
    result.b = _mm_set_epi8(
        0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff,
        0xff, d,    0xff, c,
        0xff, b,    0xff, a);
    
    return result;
}


lane_v3 set_epi8_hi(u8 a, u8 b, u8 c, u8 d)
{
    lane_v3 result;
    result.r = _mm_set_epi8(
        0xff, d+2,  0xff, c+2,
        0xff, b+2,  0xff, a+2,
        0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff);

    result.g = _mm_set_epi8(
        0xff, d+1,  0xff, c+1,
        0xff, b+1,  0xff, a+1,
        0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff);
    
    result.b = _mm_set_epi8(
        0xff, d,    0xff, c,
        0xff, b,    0xff, a,
        0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff);

    return result;
}
