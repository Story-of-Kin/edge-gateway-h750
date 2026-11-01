/* Power-fail-aware, partition-bounded NOR records. Magic commits last. */
#include "gateway_store.h"
#include "gateway_protocol.h"
#include <string.h>
#define MAGIC 0x47574131u
static uint32_t get32(const uint8_t*p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void put32(uint8_t*p,uint32_t x){p[0]=(uint8_t)x;p[1]=(uint8_t)(x>>8);p[2]=(uint8_t)(x>>16);p[3]=(uint8_t)(x>>24);}
static uint16_t get16(const uint8_t*p){return (uint16_t)(p[0]|(p[1]<<8));}
static uint32_t address(const gw_store_t*s,uint32_t i){return s->base+i*GW_RECORD_SIZE;}
static int header(gw_store_t*s,uint32_t i,uint8_t*p)
{return s->io.read(s->io.user,address(s,i),p,20);}
static int valid_header(const uint8_t*p)
{return get32(p)==MAGIC&&get32(p+4)>0&&get16(p+12)>0&&get16(p+12)<=GW_RECORD_PAYLOAD;}
static int record(gw_store_t*s,uint32_t i,uint8_t*p)
{
    if(s->io.read(s->io.user,address(s,i),p,GW_RECORD_SIZE))return -1;
    if(!valid_header(p))return 0;
    return gw_crc16(p+20,get16(p+12))==get16(p+14)?1:0;
}
static int find_head(gw_store_t*s)
{
    uint8_t p[GW_RECORD_SIZE];uint32_t i,min=0xffffffffu;int found=0,r;
    for(i=0;i<s->size/GW_RECORD_SIZE;i++) {
        if(header(s,i,p))return -1;
        if(valid_header(p)&&get32(p+16)==0xffffffffu&&get32(p+4)<min){
            r=record(s,i,p);if(r<0)return -1;
            if(r==1){min=get32(p+4);s->head_slot=i;found=1;}
        }
    }
    return found;
}
int gw_store_open(gw_store_t*s,const gw_flash_t*io,uint32_t base,uint32_t size,int cache)
{
    uint8_t p[GW_RECORD_SIZE];uint32_t i,max=0,min=0xffffffffu;int r;
    if(!s||!io||!io->read||!io->write||!io->erase||base<GW_DATA_START||base>=GW_DATA_END||
       size<GW_SECTOR_SIZE||size>GW_DATA_END-base||base%GW_SECTOR_SIZE||size%GW_SECTOR_SIZE)return -1;
    memset(s,0,sizeof(*s));s->io=*io;s->base=base;s->size=size;s->cache=cache;
    for(i=0;i<size/GW_RECORD_SIZE;i++) {
        if(header(s,i,p))return -2;
        if(!valid_header(p))continue;
        r=record(s,i,p);if(r<0){s->next_sequence=0;return -2;}if(r!=1)continue;
        s->records++;
        if(get32(p+4)>max){max=get32(p+4);s->next_slot=(i+1)%(size/GW_RECORD_SIZE);}
        if(cache&&get32(p+16)==0xffffffffu){s->pending++;if(get32(p+4)<min){min=get32(p+4);s->head_slot=i;}}
    }
    s->next_sequence=max+1;return 0;
}
static int sector_reclaim(gw_store_t*s,uint32_t slot)
{
    uint8_t h[20];uint32_t j,used=0;
    for(j=0;j<GW_SECTOR_SIZE/GW_RECORD_SIZE;j++) {
        if(header(s,slot+j,h))return -1;
        if(valid_header(h)){
            if(s->cache&&get32(h+16)==0xffffffffu)return 1;
            used++;
        }
    }
    if(s->io.erase(s->io.user,address(s,slot)))return -1;
    s->records=s->records>=used?s->records-used:0;return 0;
}
int gw_store_append(gw_store_t*s,const char*text,size_t n,uint32_t stamp)
{
    uint8_t p[GW_RECORD_SIZE],check[GW_RECORD_SIZE],magic[4];uint32_t attempt,i=0,k,slots;int erased,r;
    if(!s||!text||n==0||n>GW_RECORD_PAYLOAD||!s->next_sequence)return -1;
    slots=s->size/GW_RECORD_SIZE;
    for(attempt=0;attempt<slots;attempt++) {
        i=s->next_slot;
        if(s->io.read(s->io.user,address(s,i),p,sizeof(p)))return -2;
        erased=1;for(k=0;k<sizeof(p);k++)if(p[k]!=255){erased=0;break;}
        if(!erased&&i%(GW_SECTOR_SIZE/GW_RECORD_SIZE)==0){r=sector_reclaim(s,i);if(r<0)return -2;if(!r)erased=1;}
        if(erased)break;
        s->next_slot=(i+1)%slots;
    }
    if(attempt==slots)return -3;
    memset(p,255,sizeof(p));put32(p+4,s->next_sequence);put32(p+8,stamp);
    p[12]=(uint8_t)n;p[13]=(uint8_t)(n>>8);r=gw_crc16((const uint8_t*)text,n);
    p[14]=(uint8_t)r;p[15]=(uint8_t)(r>>8);memcpy(p+20,text,n);
    if(s->io.write(s->io.user,address(s,i),p,sizeof(p)))return -2;
    if(s->io.read(s->io.user,address(s,i),check,sizeof(check))||memcmp(check,p,sizeof(p)))return -2;
    put32(magic,MAGIC);
    if(s->io.write(s->io.user,address(s,i),magic,4))return -2;
    if(record(s,i,check)!=1)return -2;
    if(s->cache){if(!s->pending)s->head_slot=i;s->pending++;}
    s->records++;s->next_sequence++;s->next_slot=(i+1)%slots;return 0;
}
int gw_store_peek(gw_store_t*s,char*text,size_t cap,uint32_t*seq)
{
    uint8_t p[GW_RECORD_SIZE];uint16_t n;
    if(!s||!s->cache||!text||!seq)return -1;
    if(!s->pending)return 0;
    if(record(s,s->head_slot,p)!=1||get32(p+16)!=0xffffffffu)return -2;
    n=get16(p+12);if(cap<=(size_t)n)return -1;
    memcpy(text,p+20,n);text[n]=0;*seq=get32(p+4);return n;
}
int gw_store_ack(gw_store_t*s,uint32_t seq)
{
    uint8_t p[GW_RECORD_SIZE],zero[4]={0};
    if(!s||!s->cache||!s->pending||record(s,s->head_slot,p)!=1||get32(p+4)!=seq)return -1;
    if(s->io.write(s->io.user,address(s,s->head_slot)+16,zero,4))return -2;
    if(header(s,s->head_slot,p)||get32(p+16)!=0)return -2;
    s->pending--;if(s->pending&&find_head(s)!=1)return -2;return 0;
}
int gw_store_recent(gw_store_t*s,uint32_t index,char*text,size_t cap,uint32_t*stamp)
{
    uint8_t p[GW_RECORD_SIZE];uint32_t k,slot,found=0,slots;uint16_t n;int r;
    if(!s||!text||!stamp)return -1;
    if(index>=s->records)return 0;
    slots=s->size/GW_RECORD_SIZE;slot=(s->next_slot+slots-1)%slots;
    for(k=0;k<slots;k++,slot=(slot+slots-1)%slots){
        r=record(s,slot,p);if(r<0)return -2;if(r!=1)continue;
        if(found++!=index)continue;
        n=get16(p+12);if(cap<=n)return -1;
        memcpy(text,p+20,n);text[n]=0;*stamp=get32(p+8);return n;
    }
    return 0;
}
