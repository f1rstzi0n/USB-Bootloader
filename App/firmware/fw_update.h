#ifndef FW_UPDATE_H
#define FW_UPDATE_H
#include "fw_image.h"
typedef struct {
    fw_flash_io_t io;
    uint8_t public_key[64], uid[12];
    uint8_t active[FW_MANIFEST_SIZE], candidate[FW_MANIFEST_SIZE];
    uint8_t active_hash[32], session_hash[32];
    uint32_t next_offset;
    fw_update_state_t state;
    fw_update_action_t action;
    uint8_t last_error, reset_svc;
    bool valid_application, session, erase_authorized;
} fw_update_t;

/* 公钥为大端 X||Y；全零拒绝所有镜像。启动时验证已提交应用及完整摘要。 */
void fw_update_init(fw_update_t *, const fw_flash_io_t *, const uint8_t public_key[64],
                    const uint8_t uid[12]);
/* out 至少 FW_INFO_SIZE；同步执行 BLOCK/FINISH，BEGIN 不擦写。 */
void fw_update_request(fw_update_t *, uint8_t svc, const uint8_t *, uint16_t len,
                       uint8_t *out, uint16_t *out_len);
void fw_update_info(const fw_update_t *, uint8_t mode, uint8_t out[FW_INFO_SIZE]);
void fw_update_tx_complete(fw_update_t *, uint8_t svc, bool delivered);
fw_update_action_t fw_update_take_action(fw_update_t *);
/* 仅在 ERASE action 后由 board 软断开 USB 再调用；同步擦除扇区 4–7。 */
bool fw_update_erase(fw_update_t *);
/* planned=true 保留擦除会话；未完成的 BEGIN 在普通断连时撤销。 */
void fw_update_disconnected(fw_update_t *, bool planned);
bool fw_update_valid_application(const fw_update_t *);
#endif
