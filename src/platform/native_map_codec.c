#include "native_map_codec.h"
#include <stdlib.h>
#define CARMACK_NEAR_TAG 0xa7u
#define CARMACK_FAR_TAG 0xa8u
static uint16_t read_u16_le(const uint8_t *p){return (uint16_t)((uint16_t)p[0]|((uint16_t)p[1]<<8));}

bool wolf_native_carmack_expand(const uint8_t *source,size_t source_size,uint16_t *destination,size_t destination_size)
{
 size_t si=0,produced=0,words;
 if(!source||!destination||(destination_size&1u))return false;
 words=destination_size/sizeof(uint16_t);
 while(produced<words){
  uint16_t value;unsigned tag,count;
  if(si>source_size||source_size-si<2u)return false;
  value=read_u16_le(source+si);si+=2u;tag=value>>8;count=value&0xffu;
  if(tag!=CARMACK_NEAR_TAG&&tag!=CARMACK_FAR_TAG){destination[produced++]=value;continue;}
  if(count==0){if(si>=source_size)return false;destination[produced++]=(uint16_t)((tag<<8)|source[si++]);continue;}
  if((size_t)count>words-produced)return false;
  if(tag==CARMACK_NEAR_TAG){
   size_t offset,index;unsigned n;
   if(si>=source_size)return false;
   offset=source[si++];if(offset==0||offset>produced)return false;
   index=produced-offset;
   for(n=0;n<count;++n){if(index>=produced)return false;destination[produced++]=destination[index++];}
  }else{
   size_t index;unsigned n;
   if(source_size-si<2u)return false;
   index=read_u16_le(source+si);si+=2u;if(index>=produced)return false;
   for(n=0;n<count;++n){if(index>=produced)return false;destination[produced++]=destination[index++];}
  }
 }
 return si==source_size;
}

bool wolf_native_rlew_expand(const uint16_t *source,size_t source_words,unsigned *destination,size_t destination_words,uint16_t tag)
{
 size_t si=0,produced=0;
 if((!source&&source_words)||(!destination&&destination_words))return false;
 while(produced<destination_words){
  uint16_t value;
  if(si>=source_words)return false;
  value=source[si++];
  if(value!=tag){destination[produced++]=value;continue;}
  if(source_words-si<2u)return false;
  {
   size_t count=source[si++];uint16_t run=source[si++];
   if(count==0||count>destination_words-produced)return false;
   while(count--)destination[produced++]=run;
  }
 }
 return si==source_words;
}

bool wolf_native_map_decode_plane(const uint8_t *source,size_t source_size,unsigned *destination,size_t destination_words,uint16_t tag,bool carmack_compressed)
{
 const uint8_t *rlew_data;
 size_t rlew_size;
 uint16_t expanded_size;
 uint8_t *expanded=NULL;
 bool result;
 if(!source||!destination||destination_words==0||
    destination_words>UINT16_MAX/sizeof(uint16_t)||source_size<2u)return false;
 if(carmack_compressed){
  expanded_size=read_u16_le(source);
  if((expanded_size&1u)||expanded_size<4u||source_size<=2u)return false;
  expanded=(uint8_t *)malloc(expanded_size);
  if(!expanded)return false;
  result=wolf_native_carmack_expand(source+2,source_size-2,(uint16_t *)expanded,expanded_size);
  if(!result){free(expanded);return false;}
  if(read_u16_le(expanded)!=destination_words*sizeof(uint16_t)){free(expanded);return false;}
  rlew_data=expanded+2;
  rlew_size=(size_t)expanded_size-2u;
 }else{
  if(read_u16_le(source)!=destination_words*sizeof(uint16_t))return false;
  rlew_data=source+2;
  rlew_size=source_size-2u;
 }
 if(rlew_size&1u){free(expanded);return false;}
 result=wolf_native_rlew_expand((const uint16_t *)rlew_data,rlew_size/2u,destination,destination_words,tag);
 free(expanded);
 return result;
}
