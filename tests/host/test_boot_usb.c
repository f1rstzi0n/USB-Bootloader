/* Boot 编译配置下的真实 USB 描述符，无 Flash/USB 硬件替身。 */
#include "test_harness.h"
#include "tusb.h"
#include "usb_identity.h"
#include "usb_descriptors.h"

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | (uint16_t)p[1] << 8); }
static void test_boot_configuration(void)
{
    const tusb_desc_device_t *dev = (const tusb_desc_device_t *)tud_descriptor_device_cb();
    const uint8_t *cfg = tud_descriptor_configuration_cb(0);
    const uint8_t endpoints[] = {0x01, 0x81};
    uint16_t total = le16(cfg + 2);
    unsigned interfaces = 0, eps = 0, hid_descriptors = 0;
    TEST_ASSERT_EQ_U(USB_VID, dev->idVendor);
    TEST_ASSERT_EQ_U(USB_PID_BOOT, dev->idProduct);
    TEST_ASSERT(dev->idProduct != USB_PID_APP);
    TEST_ASSERT_EQ_U(64, dev->bMaxPacketSize0);
    TEST_ASSERT_EQ_U(1, dev->bNumConfigurations);
    TEST_ASSERT_EQ_U(1, CFG_TUD_HID);
    TEST_ASSERT_EQ_U(0, CFG_TUD_MSC);
    TEST_ASSERT_EQ_U(1, cfg[4]);
    TEST_ASSERT_EQ_U(TUD_CONFIG_DESC_LEN + TUD_HID_INOUT_DESC_LEN, total);
    for (uint16_t off = 0; off < total;)
    {
        const uint8_t *d = cfg + off;
        TEST_ASSERT(d[0] >= 2 && off + d[0] <= total);
        if (d[0] < 2 || off + d[0] > total) break;
        if (d[1] == TUSB_DESC_INTERFACE)
        {
            TEST_ASSERT_EQ_U(0, d[2]);
            TEST_ASSERT_EQ_U(2, d[4]);
            TEST_ASSERT_EQ_U(TUSB_CLASS_HID, d[5]);
            TEST_ASSERT_EQ_U(0, d[6]);
            TEST_ASSERT_EQ_U(HID_ITF_PROTOCOL_NONE, d[7]);
            interfaces++;
        }
        else if (d[1] == HID_DESC_TYPE_HID) hid_descriptors++;
        else if (d[1] == TUSB_DESC_ENDPOINT)
        {
            TEST_ASSERT(eps < sizeof(endpoints));
            if (eps < sizeof(endpoints)) TEST_ASSERT_EQ_U(endpoints[eps], d[2]);
            TEST_ASSERT_EQ_U(TUSB_XFER_INTERRUPT, d[3]);
            TEST_ASSERT_EQ_U(64, le16(d + 4));
            TEST_ASSERT_EQ_U(1, d[6]);
            eps++;
        }
        off = (uint16_t)(off + d[0]);
    }
    TEST_ASSERT_EQ_U(1, interfaces);
    TEST_ASSERT_EQ_U(1, hid_descriptors);
    TEST_ASSERT_EQ_U(2, eps);
}
static void test_boot_report(void)
{
    const uint8_t *cfg = tud_descriptor_configuration_cb(0);
    const uint8_t *report = tud_hid_descriptor_report_cb(0);
    uint16_t total = le16(cfg + 2), report_len = 0;
    uint32_t size = 0, count = 0, in_bits = 0, out_bits = 0;
    for (uint16_t off = 0; off < total; off = (uint16_t)(off + cfg[off]))
    {
        TEST_ASSERT(cfg[off] >= 2 && off + cfg[off] <= total);
        if (cfg[off] < 2 || off + cfg[off] > total) break;
        if (cfg[off + 1] == HID_DESC_TYPE_HID) report_len = le16(cfg + off + 7);
    }
    TEST_ASSERT(report_len > 0);
    for (uint16_t off = 0; off < report_len;)
    {
        uint8_t key = report[off++];
        unsigned n = key & 3u;
        uint32_t value = 0;
        if (n == 3) n = 4;
        TEST_ASSERT(off + n <= report_len);
        if (off + n > report_len) break;
        for (unsigned i = 0; i < n; ++i) value |= (uint32_t)report[off++] << (8u * i);
        switch (key & 0xFCu)
        {
        case 0x74: size = value; break;
        case 0x94: count = value; break;
        case 0x80: in_bits += size * count; break;
        case 0x90: out_bits += size * count; break;
        case 0x84: TEST_ASSERT(false); break;
        default: break;
        }
    }
    TEST_ASSERT_EQ_U(512, in_bits);
    TEST_ASSERT_EQ_U(512, out_bits);
}
static void assert_serial(const char *expected)
{
    const uint16_t *descriptor = tud_descriptor_string_cb(3, 0x0409);
    size_t n = strlen(expected);
    TEST_ASSERT(descriptor != NULL);
    TEST_ASSERT_EQ_U((TUSB_DESC_STRING << 8) | (2u * n + 2u), descriptor[0]);
    for (size_t i = 0; i < n; ++i) TEST_ASSERT_EQ_U((uint8_t)expected[i], descriptor[i + 1]);
}
static void test_boot_serial(void)
{
    static const char uid[] = "00112233445566778899AABB";
    usb_descriptors_set_serial(uid);
    assert_serial(uid);
    usb_descriptors_set_serial(NULL);
    assert_serial(USB_SERIAL);
}
int main(void)
{
    TEST_RUN(test_boot_configuration);
    TEST_RUN(test_boot_report);
    TEST_RUN(test_boot_serial);
    TEST_MAIN_END();
}
