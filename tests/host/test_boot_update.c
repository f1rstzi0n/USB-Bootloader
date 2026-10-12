#include "fw_update.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static uint8_t flash[0x80000], key[64], uid[12] = {1,2,3,4,5,6,7,8,9,10,11,12};
static uint8_t valid[256 + 2051], bad_vectors[sizeof(valid)], response[FW_INFO_SIZE];
static fw_update_t update;
static unsigned operations, cut_at, cut_bytes, erases, programs;
static uint32_t last_address;
static bool dead, corrupt_program, read_failure;
static uint16_t response_len;

static uint32_t get32(const uint8_t *p) { return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static void put32(uint8_t *p, uint32_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24); }
static void load(const char *name, uint8_t *p, size_t n) {
    char path[1024]; FILE *file;
    CHECK(snprintf(path, sizeof(path), "%s/%s", FW_FIXTURE_DIR, name) > 0);
    file=fopen(path,"rb"); CHECK(file);
    CHECK(fread(p,1,n,file)==n); CHECK(fgetc(file)==EOF); fclose(file);
}
static bool read_io(void *ctx, uint32_t a, uint8_t *p, uint32_t n) {
    (void)ctx;
    CHECK(a >= FW_HEADER_BASE && a <= FW_FLASH_END && n <= FW_FLASH_END-a);
    if (dead || read_failure) return false;
    memcpy(p,flash+a-0x08000000u,n); return true;
}
static bool program_io(void *ctx, uint32_t a, const uint8_t *p, uint32_t n) {
    unsigned i;
    (void)ctx;
    CHECK(a >= FW_HEADER_BASE && a <= FW_FLASH_END && n <= FW_FLASH_END-a);
    CHECK((a&3u)==0 && (n&3u)==0 && n);
    operations++; programs++; last_address=a;
    if (dead) return false;
    if (operations==cut_at) { if (n>cut_bytes) n=cut_bytes; dead=true; }
    for (i=0;i<n;i++) { CHECK((flash[a-0x08000000u+i]&p[i])==p[i]); flash[a-0x08000000u+i]&=p[i]; }
    if (corrupt_program && n) flash[a-0x08000000u]^=1u;
    return !dead;
}
static bool erase_io(void *ctx, uint8_t sector) {
    uint32_t begin, n;
    (void)ctx;
    CHECK(sector>=4 && sector<=7);
    begin=sector==4 ? 0x10000u : 0x20000u+(sector-5u)*0x20000u;
    n=sector==4 ? 0x10000u : 0x20000u;
    operations++; erases++;
    if (dead) return false;
    if (operations==cut_at) { if (n>cut_bytes) n=cut_bytes; dead=true; }
    memset(flash+begin,255,n); return !dead;
}
static const fw_flash_io_t io={0,read_io,program_io,erase_io};
static void fresh(void) {
    memset(flash,255,sizeof(flash)); memset(flash,0xA5,0x10000);
    operations=erases=programs=cut_at=cut_bytes=0; dead=corrupt_program=read_failure=false;
    fw_update_init(&update,&io,key,uid);
}
static void request(uint8_t svc, const uint8_t *p, uint16_t n, uint8_t expected) {
    memset(response,0xEE,sizeof(response)); response_len=0;
    fw_update_request(&update,svc,p,n,response,&response_len);
    CHECK(response_len && response[0]==expected);
}
static bool begin(const uint8_t *package) {
    request(FW_SVC_BEGIN,package,256,0); CHECK(update.state==FW_AWAIT_ERASE);
    CHECK(!fw_update_erase(&update)); CHECK(!erases);
    fw_update_tx_complete(&update,FW_SVC_BEGIN,true);
    CHECK(fw_update_take_action(&update)==FW_ACTION_ERASE);
    fw_update_disconnected(&update,true);
    return fw_update_erase(&update);
}
static bool transfer(const uint8_t *package) {
    uint8_t block[FW_BLOCK_SIZE+4]; uint32_t length=get32(package+28),offset;
    for (offset=0;offset<length;offset+=FW_BLOCK_SIZE) {
        uint16_t n=(uint16_t)(length-offset>FW_BLOCK_SIZE ? FW_BLOCK_SIZE : length-offset);
        put32(block,offset); memcpy(block+4,package+256+offset,n);
        fw_update_request(&update,FW_SVC_BLOCK,block,n+4,response,&response_len);
        if (response[0]) return false;
        CHECK(response_len==5 && get32(response+1)==offset+n);
    }
    return true;
}
static bool finish(void) {
    fw_update_request(&update,FW_SVC_FINISH,0,0,response,&response_len);
    return response[0]==0;
}
static void check_boot_intact(void) { unsigned i; for(i=0;i<0x10000;i++) CHECK(flash[i]==0xA5); }
static void test_normal(void) {
    uint8_t hash[32]; fw_update_t booted;
    fresh(); request(FW_SVC_INFO,0,0,0); CHECK(response_len==149 && response[1]==2 && !response[3]);
    CHECK(!memcmp(response+5,uid,12)); CHECK(get32(response+37)==FW_APP_BASE && get32(response+41)==FW_APP_CAPACITY);
    CHECK(begin(valid)); CHECK(erases==4 && update.state==FW_RECEIVING);
    fw_update_disconnected(&update,false); CHECK(update.state==FW_RECEIVING && update.session);
    CHECK(transfer(valid)); CHECK(flash[FW_APP_BASE-0x08000000u+2051]==0xFF);
    CHECK(finish()); CHECK(programs==5 && last_address==FW_COMMIT_ADDRESS);
    CHECK(fw_update_valid_application(&update) && update.state==FW_COMMITTED);
    fw_update_disconnected(&update,false); CHECK(update.state==FW_COMMITTED && update.valid_application);
    CHECK(fw_update_take_action(&update)==FW_ACTION_NONE);
    fw_update_tx_complete(&update,FW_SVC_FINISH,false);
    CHECK(fw_update_take_action(&update)==FW_ACTION_NONE);
    request(FW_SVC_INFO,0,0,0); CHECK(response[3]==1 && response[2]==FW_COMMITTED);
    CHECK(fw_sha256(valid,192,hash)); CHECK(!memcmp(response+49,hash,32));
    request(FW_SVC_RUN_APP,0,0,0); fw_update_tx_complete(&update,FW_SVC_RUN_APP,true);
    CHECK(fw_update_take_action(&update)==FW_ACTION_RESET);
    fw_update_init(&booted,&io,key,uid); CHECK(fw_update_valid_application(&booted));
    fw_update_info(&update,FW_MODE_APP,response); CHECK(response[2]==FW_IDLE && !get32(response+45));
    memset(hash,0,32); CHECK(!memcmp(response+81,hash,32));
    check_boot_intact();
    /* 合法包重复刷写不受版本或摘要去重限制。 */
    fw_update_init(&update,&io,key,uid); erases=0; CHECK(begin(valid)); CHECK(transfer(valid)); CHECK(finish());
    check_boot_intact();
}
static void test_preflight(void) {
    uint8_t header[256],wrong_key[64]={0}, sha[32];
    fresh(); CHECK(fw_manifest_verify(valid,key)==0); CHECK(fw_manifest_verify(valid,wrong_key)==FW_ERR_SIGNATURE);
    memcpy(header,valid,256); header[192]^=1; request(FW_SVC_BEGIN,header,256,FW_ERR_SIGNATURE);
    memcpy(header,valid,256); header[8]='X'; request(FW_SVC_BEGIN,header,256,FW_ERR_TARGET);
    memcpy(header,valid,256); put32(header+24,0x08000000); request(FW_SVC_BEGIN,header,256,0x03);
    memcpy(header,valid,256); put32(header+28,FW_APP_CAPACITY+1); request(FW_SVC_BEGIN,header,256,0x03);
    memcpy(header,valid,256); header[100]=1; request(FW_SVC_BEGIN,header,256,0x08);
    memcpy(header,valid,256); header[40]='X'; request(FW_SVC_BEGIN,header,256,FW_ERR_SIGNATURE);
    request(FW_SVC_BEGIN,valid,255,0x08); CHECK(!erases && !programs);
    CHECK(fw_sha256((const uint8_t *)"abc",3,sha));
    CHECK(sha[0]==0xBA && sha[31]==0xAD);
    request(FW_SVC_BEGIN,valid,256,0); fw_update_tx_complete(&update,FW_SVC_BEGIN,false);
    CHECK(update.state==FW_IDLE && !update.session && !fw_update_erase(&update));
    CHECK(fw_update_take_action(&update)==FW_ACTION_NONE && !erases);
    request(FW_SVC_BEGIN,valid,256,0); fw_update_disconnected(&update,false);
    CHECK(update.state==FW_IDLE && !update.session && !erases);
    request(FW_SVC_RUN_APP,0,0,0x0A);
}
static void test_blocks_and_corruption(void) {
    uint8_t block[FW_BLOCK_SIZE+4]; fw_update_t booted;
    fresh(); CHECK(begin(valid)); put32(block,1); memcpy(block+4,valid+256,1024);
    request(FW_SVC_BLOCK,block,sizeof(block),0x03); CHECK(!programs && !update.next_offset);
    put32(block,0); request(FW_SVC_BLOCK,block,1027,0x08); CHECK(!programs);
    request(FW_SVC_FINISH,0,0,0x0A);
    CHECK(transfer(valid)); flash[FW_APP_BASE-0x08000000u+100]^=1;
    request(FW_SVC_FINISH,0,0,FW_ERR_DIGEST); CHECK(update.state==FW_FAILED && programs==3);
    fw_update_init(&booted,&io,key,uid); CHECK(!fw_update_valid_application(&booted));
    fresh(); CHECK(begin(bad_vectors)); CHECK(transfer(bad_vectors));
    request(FW_SVC_FINISH,0,0,0x03); CHECK(programs==3 && update.state==FW_FAILED);
    fresh(); CHECK(begin(valid)); corrupt_program=true;
    request(FW_SVC_BLOCK,block,sizeof(block),FW_ERR_FLASH); CHECK(update.state==FW_FAILED && !update.next_offset);
    check_boot_intact();
}
static void test_power_cuts(void) {
    unsigned cut,bytes;
    for(cut=1;cut<=9;cut++) for(bytes=0;bytes<=4;bytes++) {
        fw_update_t booted; bool complete;
        fresh(); cut_at=cut; cut_bytes=bytes;
        complete=begin(valid);
        if (complete) complete=transfer(valid);
        if (complete) complete=finish();
        CHECK(!complete && dead);
        dead=false;
        fw_update_init(&booted,&io,key,uid);
        CHECK(booted.state==FW_IDLE && !booted.session && !booted.next_offset);
        if (cut==9 && bytes==4) CHECK(fw_update_valid_application(&booted));
        else CHECK(!fw_update_valid_application(&booted));
        check_boot_intact();
        /* 丢弃RAM会话，重启后从BEGIN整包重刷。 */
        update=booted; cut_at=operations=erases=programs=0;
        CHECK(begin(valid)); CHECK(transfer(valid)); CHECK(finish());
        fw_update_init(&booted,&io,key,uid); CHECK(fw_update_valid_application(&booted));
    }
}
static void test_boot_validation(void) {
    fw_update_t booted; uint8_t zeros[64]={0};
    fresh(); CHECK(begin(valid)); CHECK(transfer(valid)); CHECK(finish());
    fw_update_init(&booted,&io,zeros,uid); CHECK(!fw_update_valid_application(&booted));
    flash[FW_APP_BASE-0x08000000u+100]^=1;
    fw_update_init(&booted,&io,key,uid); CHECK(!fw_update_valid_application(&booted));
    flash[FW_APP_BASE-0x08000000u+100]^=1;
    flash[FW_HEADER_BASE-0x08000000u+192]^=1;
    fw_update_init(&booted,&io,key,uid); CHECK(!fw_update_valid_application(&booted));
}
static void test_capacity(void) {
    uint8_t *package=malloc(256u+FW_APP_CAPACITY+1u); CHECK(package);
    load("oversize.afw",package,256u+FW_APP_CAPACITY+1u); fresh();
    request(FW_SVC_BEGIN,package,256,0x03); CHECK(!programs && !erases);
    load("capacity.afw",package,256u+FW_APP_CAPACITY); CHECK(fw_manifest_verify(package,key)==0);
    CHECK(begin(package)); CHECK(transfer(package)); CHECK(finish());
    CHECK(update.next_offset==FW_APP_CAPACITY); check_boot_intact(); free(package);
}
int main(void) {
    load("public-key.bin",key,sizeof(key)); load("valid.afw",valid,sizeof(valid)); load("bad-vectors.afw",bad_vectors,sizeof(bad_vectors));
    test_normal(); test_preflight(); test_blocks_and_corruption(); test_power_cuts(); test_boot_validation(); test_capacity();
    puts("boot update: signatures, fixed layout, completed-IN gate, power cuts and recovery passed"); return 0;
}


void fw_validation_progress(void) {}
