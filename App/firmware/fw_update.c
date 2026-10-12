#include "fw_update.h"
#include <string.h>
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void put16(uint8_t *p, uint16_t n) { p[0] = (uint8_t)n; p[1] = (uint8_t)(n >> 8); }
static void put32(uint8_t *p, uint32_t n) {
    p[0] = (uint8_t)n; p[1] = (uint8_t)(n >> 8); p[2] = (uint8_t)(n >> 16); p[3] = (uint8_t)(n >> 24);
}

static bool read_flash(fw_update_t *f, uint32_t address, uint8_t *p, uint32_t n) {
    return f->io.read && address >= FW_HEADER_BASE && address <= FW_FLASH_END &&
        n <= FW_FLASH_END - address && f->io.read(f->io.ctx, address, p, n);
}
static bool program_flash(fw_update_t *f, uint32_t address, const uint8_t *p, uint32_t n) {
    uint8_t check[256];
    uint32_t offset;
    if (!f->io.program || (address & 3u) || (n & 3u) || !n || address < FW_HEADER_BASE ||
        address > FW_FLASH_END || n > FW_FLASH_END - address ||
        !f->io.program(f->io.ctx, address, p, n)) return false;
    for (offset = 0; offset < n; offset += sizeof(check)) {
        uint32_t count = n - offset;
        if (count > sizeof(check)) count = sizeof(check);
        if (!read_flash(f, address + offset, check, count) || memcmp(check, p + offset, count)) return false;
    }
    return true;
}
static uint8_t fail(fw_update_t *f, uint8_t error) {
    f->state = FW_FAILED;
    f->last_error = error;
    f->action = FW_ACTION_NONE;
    f->reset_svc = 0;
    return error;
}

void fw_update_init(fw_update_t *f, const fw_flash_io_t *io, const uint8_t key[64], const uint8_t uid[12]) {
    fw_image_t image;
    memset(f, 0, sizeof(*f));
    fw_image_init(&image, io, key, uid);
    f->io = image.io;
    memcpy(f->public_key, image.public_key, 64);
    memcpy(f->uid, image.uid, 12);
    memcpy(f->active, image.active, FW_MANIFEST_SIZE);
    memcpy(f->active_hash, image.active_hash, 32);
    f->last_error = image.last_error;
    f->valid_application = image.valid_application;
}
static uint8_t image_check(fw_update_t *f, const uint8_t *h) {
    fw_image_t image;
    memset(&image, 0, sizeof(image));
    image.io = f->io;
    return fw_image_check(&image, h);
}

void fw_update_info(const fw_update_t *f, uint8_t mode, uint8_t out[FW_INFO_SIZE]) {
    memset(out, 0, FW_INFO_SIZE);
    out[1] = mode;
    out[2] = mode == FW_MODE_APP ? FW_IDLE : (uint8_t)f->state;
    out[3] = f->valid_application ? 1u : 0u;
    out[4] = f->last_error;
    memcpy(out + 5, f->uid, sizeof(f->uid));
    memcpy(out + 17, FW_TARGET, sizeof(FW_TARGET));
    put16(out + 33, FW_BOOT_CONTRACT);
    put16(out + 35, FW_BLOCK_SIZE);
    put32(out + 37, FW_APP_BASE);
    put32(out + 41, FW_APP_CAPACITY);
    if (mode != FW_MODE_APP) {
        put32(out + 45, f->next_offset);
        if (f->session) memcpy(out + 81, f->session_hash, 32);
    }
    if (f->valid_application) {
        memcpy(out + 49, f->active_hash, 32);
        memcpy(out + 113, f->active + 32, 32);
        memcpy(out + 145, f->active + 28, 4);
    }
}

void fw_update_request(fw_update_t *f, uint8_t svc, const uint8_t *p, uint16_t len,
                       uint8_t *out, uint16_t *out_len) {
    uint8_t error = 0;
    *out_len = 1;
    if (len && !p) { out[0] = 0x08u; return; }
    if (svc == FW_SVC_INFO) {
        if (len) { out[0] = 0x08u; return; }
        fw_update_info(f, FW_MODE_BOOT, out);
        *out_len = FW_INFO_SIZE;
        return;
    }
    if (f->reset_svc || f->state == FW_ERASING || f->state == FW_VERIFYING) {
        out[0] = 0x06u;
        return;
    }
    switch (svc) {
    case FW_SVC_BEGIN:
        if (f->state == FW_AWAIT_ERASE) { error = 0x06u; break; }
        if (len != FW_MANIFEST_SIZE) { error = 0x08u; break; }
        error = fw_manifest_verify(p, f->public_key);
        if (error) break;
        if (!fw_sha256(p, FW_MANIFEST_SIGNED_SIZE, f->session_hash)) { error = FW_ERR_SIGNATURE; break; }
        memcpy(f->candidate, p, sizeof(f->candidate));
        f->next_offset = 0;
        f->session = true;
        f->state = FW_AWAIT_ERASE;
        f->action = FW_ACTION_NONE;
        f->last_error = 0;
        break;
    case FW_SVC_BLOCK: {
        uint8_t data[FW_BLOCK_SIZE];
        uint32_t offset, n, padded, remaining;
        if (f->state != FW_RECEIVING || !f->session) { error = 0x0Au; break; }
        if (len < 5u || len > FW_BLOCK_SIZE + 4u) { error = 0x08u; break; }
        offset = get32(p);
        n = len - 4u;
        if (offset != f->next_offset || offset >= get32(f->candidate + 28)) { error = 0x03u; break; }
        remaining = get32(f->candidate + 28) - offset;
        if (n != (remaining > FW_BLOCK_SIZE ? FW_BLOCK_SIZE : remaining)) { error = 0x08u; break; }
        padded = (n + 3u) & ~3u;
        memcpy(data, p + 4, n);
        memset(data + n, 0xFF, padded - n);
        if (!program_flash(f, FW_APP_BASE + offset, data, padded)) { error = fail(f, FW_ERR_FLASH); break; }
        f->next_offset += n;
        put32(out + 1, f->next_offset);
        *out_len = 5;
        break;
    }
    case FW_SVC_FINISH: {
        uint8_t commit[4];
        if (len) { error = 0x08u; break; }
        if (f->state != FW_RECEIVING || !f->session || f->next_offset != get32(f->candidate + 28)) {
            error = 0x0Au; break;
        }
        f->state = FW_VERIFYING;
        error = image_check(f, f->candidate);
        if (error) { fail(f, error); break; }
        if (!program_flash(f, FW_HEADER_BASE, f->candidate, FW_MANIFEST_SIZE)) {
            error = fail(f, FW_ERR_FLASH); break;
        }
        put32(commit, FW_COMMIT_VALUE);
        if (!program_flash(f, FW_COMMIT_ADDRESS, commit, sizeof(commit))) {
            error = fail(f, FW_ERR_FLASH); break;
        }
        memcpy(f->active, f->candidate, sizeof(f->active));
        memcpy(f->active_hash, f->session_hash, sizeof(f->active_hash));
        f->valid_application = true;
        f->state = FW_COMMITTED;
        f->last_error = 0;
        f->reset_svc = FW_SVC_FINISH;
        break;
    }
    case FW_SVC_RUN_APP:
        if (len) { error = 0x08u; break; }
        if (!f->valid_application || f->state == FW_AWAIT_ERASE) { error = 0x0Au; break; }
        f->reset_svc = FW_SVC_RUN_APP;
        break;
    default: error = 0x09u; break;
    }
    out[0] = error;
}

void fw_update_tx_complete(fw_update_t *f, uint8_t svc, bool delivered) {
    if (svc == FW_SVC_BEGIN && f->state == FW_AWAIT_ERASE) {
        if (delivered) { f->action = FW_ACTION_ERASE; f->erase_authorized = true; }
        else fw_update_disconnected(f, false);
    } else if (svc == f->reset_svc && f->reset_svc) {
        f->action = delivered ? FW_ACTION_RESET : FW_ACTION_NONE;
        f->reset_svc = 0;
    }
}
fw_update_action_t fw_update_take_action(fw_update_t *f) {
    fw_update_action_t result = f->action;
    f->action = FW_ACTION_NONE;
    return result;
}
bool fw_update_erase(fw_update_t *f) {
    uint8_t sector;
    if (f->state != FW_AWAIT_ERASE || !f->session || !f->erase_authorized) return false;
    f->erase_authorized = false;
    f->state = FW_ERASING;
    f->valid_application = false;
    memset(f->active, 0, sizeof(f->active));
    memset(f->active_hash, 0, sizeof(f->active_hash));
    for (sector = 4; sector <= 7; ++sector) {
        if (!f->io.erase_sector || !f->io.erase_sector(f->io.ctx, sector)) {
            fail(f, FW_ERR_FLASH);
            return false;
        }
    }
    f->state = FW_RECEIVING;
    return true;
}
void fw_update_disconnected(fw_update_t *f, bool planned) {
    if (!planned && f->state == FW_AWAIT_ERASE) {
        f->state = FW_IDLE;
        f->erase_authorized = false;
        f->session = false;
        f->next_offset = 0;
        memset(f->candidate, 0, sizeof(f->candidate));
        memset(f->session_hash, 0, sizeof(f->session_hash));
        f->action = FW_ACTION_NONE;
    }
    if (!planned) f->reset_svc = 0;
}
bool fw_update_valid_application(const fw_update_t *f) { return f->valid_application; }

