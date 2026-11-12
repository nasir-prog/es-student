#include "device/device.h"

#include "hardware/structs/sysinfo.h"
#include "pico/unique_id.h"
#include "pico/version.h"

void device_get_info(device_info_t *info)
{
    const uint32_t chip_id = sysinfo_hw->chip_id;
    info->board = DEVICE_BOARD;
    info->sdk_version = PICO_SDK_VERSION_STRING;
    pico_get_unique_board_id_string(info->serial, sizeof(info->serial));
    info->manufacturer = (chip_id & SYSINFO_CHIP_ID_MANUFACTURER_BITS) >> SYSINFO_CHIP_ID_MANUFACTURER_LSB;
    info->part = (chip_id & SYSINFO_CHIP_ID_PART_BITS) >> SYSINFO_CHIP_ID_PART_LSB;
    info->revision = (chip_id & SYSINFO_CHIP_ID_REVISION_BITS) >> SYSINFO_CHIP_ID_REVISION_LSB;
}
