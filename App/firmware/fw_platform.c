#include <string.h>
#include "main.h"
#include "board.h"
#include "usb_port.h"
#include "usb_io.h"
#include "usb_proto.h"
#include "usb_descriptors.h"
#include "tusb.h"
#include "fw_platform.h"
#include "fw_update.h"
#include "fw_public_key.h"


#define FW_BOOT_REQUEST 0xB007F446u
__attribute__((section(".boot_request"), used))
static volatile uint32_t s_boot_request[2];
static fw_update_t s_fw;
static bool s_planned_disconnect;

static bool flash_read(void *ctx, uint32_t address, uint8_t *out, uint32_t len)
{
    (void)ctx;
    if (!out || address < FW_HEADER_BASE || address > FW_FLASH_END ||
        len > FW_FLASH_END - address) return false;
    memcpy(out, (const void *)(uintptr_t)address, len);
    return true;
}
static bool flash_program(void *ctx, uint32_t address, const uint8_t *p, uint32_t len)
{
    bool ok = true;
    uint32_t off;
    (void)ctx;
    if (!p || !len || (address & 3u) || (len & 3u) ||
        address < FW_HEADER_BASE || address >= FW_FLASH_END ||
        len > FW_FLASH_END - address) return false;
    if (HAL_FLASH_Unlock() != HAL_OK) return false;
    for (off = 0; off < len; off += 4u) {
        uint32_t word;
        memcpy(&word, p + off, sizeof(word));
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address + off, word) != HAL_OK) {
            ok = false;
            break;
        }
        /* 单 word 编程后服务 USB，避免整块阻塞控制请求。 */
        usb_io_task();
        board_watchdog_refresh();
    }
    (void)HAL_FLASH_Lock();
    /* 编程后的回读必须来自 Flash，不能使用旧缓存。 */
    FLASH_FlushCaches();
    return ok;
}
static bool flash_erase_sector(void *ctx, uint8_t sector)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t error = 0;
    HAL_StatusTypeDef result;
    (void)ctx;
    if (sector < 4u || sector > 7u) return false;
    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Sector = sector;
    erase.NbSectors = 1;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    if (HAL_FLASH_Unlock() != HAL_OK) return false;
    board_watchdog_refresh();
    result = HAL_FLASHEx_Erase(&erase, &error);
    if (result == HAL_OK) board_watchdog_refresh();
    (void)HAL_FLASH_Lock();
    return result == HAL_OK;
}
static void update_init(void)
{
    uint32_t words[3] = { HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2() };
    uint8_t uid[12];
    fw_flash_io_t io = {0};
    memcpy(uid, words, sizeof(uid));
    io.read = flash_read;
    io.program = flash_program;
    io.erase_sector = flash_erase_sector;
    fw_update_init(&s_fw, &io, fw_public_key, uid);
}
static void boot_wait(uint32_t interval)
{
    uint32_t start = HAL_GetTick();
    do { usb_io_task(); board_watchdog_refresh(); } while ((uint32_t)(HAL_GetTick() - start) < interval);
}
static void boot_disconnect(void)
{
    uint32_t serial = usb_io_connect_request(false), actual_ms = 0;
    usb_proto_disconnected();
    do { usb_io_task(); board_watchdog_refresh(); }
    while (!usb_io_connect_applied(serial, &actual_ms) || (uint32_t)(HAL_GetTick() - actual_ms) < 250u);
}
static void reset_device(bool enter_boot)
{
    boot_disconnect();
    s_boot_request[0] = enter_boot ? FW_BOOT_REQUEST : 0u;
    s_boot_request[1] = enter_boot ? ~FW_BOOT_REQUEST : 0u;
    __DSB();
    NVIC_SystemReset();
}
static size_t boot_dispatch(const bvpv_frame_t *req, uint8_t *out, uint16_t cap)
{
    uint8_t payload[FW_INFO_SIZE];
    uint16_t len = 1;
    if (!req || !out || cap < FW_INFO_SIZE + BVPV_FRAME_OVERHEAD) return 0;
    if (req->flags != BVPV_PROTO_VERSION) payload[0] = 0x0Au;
    else fw_update_request(&s_fw, req->svc, req->payload, req->plen, payload, &len);
    return bvpv_frame_encode(out, req->svc, BVPV_PROTO_VERSION | BVPV_FLAGS_DIR_RESPONSE,
                             req->tid, payload, len);
}
static void boot_cancel(void) { fw_update_disconnected(&s_fw, s_planned_disconnect); }
static void boot_sent(uint8_t svc, bool delivered) { fw_update_tx_complete(&s_fw, svc, delivered); }
static uint8_t boot_reply_policy(const uint8_t *frame, size_t len)
{
    if (len < BVPV_FRAME_OVERHEAD + 1u || frame[BVPV_FRAME_HDR_LEN]) return 0;
    return frame[0] == FW_SVC_BEGIN || frame[0] == FW_SVC_FINISH || frame[0] == FW_SVC_RUN_APP
        ? USB_REPLY_KEEP_RX_CLOSED : 0;
}
static const usb_proto_backend_t boot_backend = {
    boot_dispatch, 0, 0, 0, boot_cancel, boot_sent, boot_reply_policy
};
__attribute__((noreturn))
static void jump_application(void)
{
    const uint32_t *vector = (const uint32_t *)(uintptr_t)FW_APP_BASE;
    uint32_t sp = vector[0], entry = vector[1];
    unsigned i;
    board_watchdog_refresh();
    (void)HAL_RCC_DeInit();
    __disable_irq();
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;
    for (i = 0; i < 8; ++i) {
        NVIC->ICER[i] = 0xFFFFFFFFu;
        NVIC->ICPR[i] = 0xFFFFFFFFu;
    }
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
    SCB->VTOR = FW_APP_BASE;
    __DSB();
    __ISB();
    __asm volatile("msr msp, %0\n"
                   "movs r2, #0\n"
                   "msr control, r2\n"
                   "isb\n"
                   "cpsie i\n"
                   "bx %1\n" :: "r"(sp), "r"(entry) : "r2", "memory");
    __builtin_unreachable();
}
void fw_platform_boot_start(void)
{
    bool requested = __HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) != RESET &&
                     __HAL_RCC_GET_FLAG(RCC_FLAG_PORRST) == RESET &&
                     __HAL_RCC_GET_FLAG(RCC_FLAG_BORRST) == RESET &&
                     s_boot_request[0] == FW_BOOT_REQUEST &&
                     s_boot_request[1] == ~FW_BOOT_REQUEST;
    s_boot_request[0] = s_boot_request[1] = 0;
    __HAL_RCC_CLEAR_RESET_FLAGS();
    update_init();
    if (!requested && fw_update_valid_application(&s_fw)) jump_application();
    usb_descriptors_set_serial(board_usb_serial());
    usb_proto_init_backend(&boot_backend);
}
void fw_platform_task(void)
{
    fw_update_action_t action = fw_update_take_action(&s_fw);
    if (action == FW_ACTION_RESET) reset_device(false);
    if (action == FW_ACTION_ERASE) {
        s_planned_disconnect = true;
        boot_disconnect();
        (void)fw_update_erase(&s_fw);
        usb_proto_init_backend(&boot_backend);
        boot_wait(250);
        s_planned_disconnect = false;
        usb_port_connect(true);
    }
}

void fw_validation_progress(void)
{
    board_watchdog_refresh();
    if (tud_inited()) usb_io_task();
}
