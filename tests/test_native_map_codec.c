#ifdef NDEBUG
#undef NDEBUG
#endif
#include "native_map_codec.h"
#include <assert.h>
#include <stdio.h>
static void test_carmack_literals_near_far_escape(void)
{
 const uint8_t near_data[]={0x34,0x12,0x02,0xa7,0x01};
 const uint8_t far_data[]={0x78,0x56,0x02,0xa8,0x00,0x00};
 const uint8_t escaped[]={0x00,0xa7,0x5a};
 uint16_t out[3]={0},one[1]={0};
 assert(wolf_native_carmack_expand(near_data,sizeof(near_data),out,sizeof(out)));
 assert(out[0]==0x1234&&out[1]==0x1234&&out[2]==0x1234);
 assert(wolf_native_carmack_expand(far_data,sizeof(far_data),out,sizeof(out)));
 assert(out[0]==0x5678&&out[1]==0x5678&&out[2]==0x5678);
 assert(wolf_native_carmack_expand(escaped,sizeof(escaped),one,sizeof(one)));
 assert(one[0]==0xa75a);
}
static void test_carmack_rejects_malformed(void)
{
 const uint8_t truncated_lit[]={0x34};
 const uint8_t truncated_near[]={0x01,0xa7};
 const uint8_t near_zero[]={0x34,0x12,0x01,0xa7,0x00};
 const uint8_t near_far[]={0x34,0x12,0x01,0xa7,0x02};
 const uint8_t oversized[]={0x34,0x12,0x02,0xa7,0x01};
 uint16_t out[3]={0};
 assert(!wolf_native_carmack_expand(truncated_lit,sizeof(truncated_lit),out,2));
 assert(!wolf_native_carmack_expand(truncated_near,sizeof(truncated_near),out,2));
 assert(!wolf_native_carmack_expand(near_zero,sizeof(near_zero),out,4));
 assert(!wolf_native_carmack_expand(near_far,sizeof(near_far),out,4));
 assert(!wolf_native_carmack_expand(oversized,sizeof(oversized),out,6));
}
static void test_rlew_valid_and_malformed(void)
{
 const uint16_t valid[]={1,2,0xabcd,3,7,9};
 const uint16_t truncated[]={0xabcd,2};
 const uint16_t zero[]={0xabcd,0,7};
 const uint16_t oversized[]={0xabcd,4,7};
 const uint16_t short_input[]={1,2};
 unsigned out[5]={0},small[3]={0};
 assert(wolf_native_rlew_expand(valid,6,out,5,0xabcd));
 assert(out[0]==1&&out[1]==2&&out[2]==7&&out[3]==7&&out[4]==7);
 assert(!wolf_native_rlew_expand(truncated,2,small,2,0xabcd));
 assert(!wolf_native_rlew_expand(zero,3,small,2,0xabcd));
 assert(!wolf_native_rlew_expand(oversized,3,small,3,0xabcd));
 assert(!wolf_native_rlew_expand(short_input,2,small,3,0xabcd));
}
int main(void){test_carmack_literals_near_far_escape();test_carmack_rejects_malformed();test_rlew_valid_and_malformed();puts("native map codec tests passed");return 0;}
