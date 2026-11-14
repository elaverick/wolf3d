#ifdef NDEBUG
#undef NDEBUG
#endif
#include "native_map_codec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void put16(uint8_t *p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void test_carmack_valid(void){
 const uint8_t near_data[]={0x34,0x12,0x02,0xa7,0x01};
 const uint8_t far_data[]={0x78,0x56,0x02,0xa8,0x00,0x00};
 const uint8_t escape[]={0x00,0xa7,0x5a};
 uint16_t out[3]={0},one[1]={0};
 assert(wolf_native_carmack_expand(near_data,sizeof near_data,out,sizeof out));
 assert(out[0]==0x1234&&out[1]==0x1234&&out[2]==0x1234);
 assert(wolf_native_carmack_expand(far_data,sizeof far_data,out,sizeof out));
 assert(out[0]==0x5678&&out[1]==0x5678&&out[2]==0x5678);
 assert(wolf_native_carmack_expand(escape,sizeof escape,one,sizeof one)&&one[0]==0xa75a);
}
static void test_carmack_invalid(void){
 const uint8_t shortword[]={0x34};
 const uint8_t nooffset[]={0x01,0xa7};
 const uint8_t zerooffset[]={0x34,0x12,0x01,0xa7,0x00};
 const uint8_t badoffset[]={0x34,0x12,0x01,0xa7,0x02};
 const uint8_t oversize[]={0x34,0x12,0x02,0xa7,0x01};
 const uint8_t trailing[]={0x34,0x12,0x99,0x99};
 uint16_t out[3]={0};
 assert(!wolf_native_carmack_expand(shortword,sizeof shortword,out,2));
 assert(!wolf_native_carmack_expand(nooffset,sizeof nooffset,out,2));
 assert(!wolf_native_carmack_expand(zerooffset,sizeof zerooffset,out,4));
 assert(!wolf_native_carmack_expand(badoffset,sizeof badoffset,out,4));
 assert(!wolf_native_carmack_expand(oversize,sizeof oversize,out,4));
 assert(!wolf_native_carmack_expand(trailing,sizeof trailing,out,2));
}
static void test_rlew(void){
 const uint16_t valid[]={1,2,0xabcd,3,7,9};
 const uint16_t truncated[]={0xabcd,2};
 const uint16_t zero[]={0xabcd,0,7};
 const uint16_t large[]={0xabcd,4,7};
 const uint16_t extra[]={1,2,3};
 unsigned out[5]={0},small[3]={0};
 assert(wolf_native_rlew_expand(valid,6,out,5,0xabcd));
 assert(out[0]==1&&out[1]==2&&out[2]==7&&out[3]==7&&out[4]==7);
 assert(!wolf_native_rlew_expand(truncated,2,small,2,0xabcd));
 assert(!wolf_native_rlew_expand(zero,3,small,2,0xabcd));
 assert(!wolf_native_rlew_expand(large,3,small,3,0xabcd));
 assert(!wolf_native_rlew_expand(extra,3,small,2,0xabcd));
}
static void test_complete_plane_formats(void){
 const uint16_t words[]={8,1,2,0xabcd,2,7};
 uint8_t carmack[2+sizeof words],rlew[sizeof words];
 unsigned out[4]={0};size_t i;
 put16(carmack,(uint16_t)sizeof words);
 for(i=0;i<6;i++){put16(carmack+2+i*2,words[i]);put16(rlew+i*2,words[i]);}
 assert(wolf_native_map_decode_plane(carmack,sizeof carmack,out,4,0xabcd,true));
 assert(out[0]==1&&out[1]==2&&out[2]==7&&out[3]==7);
 memset(out,0,sizeof out);
 assert(wolf_native_map_decode_plane(rlew,sizeof rlew,out,4,0xabcd,false));
 assert(out[0]==1&&out[1]==2&&out[2]==7&&out[3]==7);
}
static void test_complete_plane_invalid(void){
 const uint8_t wrong[]={6,0,1,0,2,0};
 const uint8_t truncated[]={8,0,1,0};
 unsigned out[4]={0};
 assert(!wolf_native_map_decode_plane(wrong,sizeof wrong,out,4,0xabcd,false));
 assert(!wolf_native_map_decode_plane(wrong,1,out,4,0xabcd,false));
 assert(!wolf_native_map_decode_plane(truncated,sizeof truncated,out,4,0xabcd,true));
}
int main(void){
 test_carmack_valid();test_carmack_invalid();test_rlew();
 test_complete_plane_formats();test_complete_plane_invalid();
 puts("native map codec tests passed");return 0;
}
