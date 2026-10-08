#include "device.h"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include "pico/version.h"
#include "pico/unique_id.h"
#include "hardware/regs/addressmap.h"
#include "hardware/regs/sysinfo.h"

struct info_t device_card = {
    .version = 0x00010100,
    .name = DEVICE_NAME,
    .revision = 2,
};

void device_info(void)
{
    char board_id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];
    pico_get_unique_board_id_string(board_id, sizeof(board_id));

    volatile uint32_t *chip_id =
        (uint32_t *)(SYSINFO_BASE + SYSINFO_CHIP_ID_OFFSET);

    uint32_t id = *chip_id;

    uint32_t manufacturer =
        (id & SYSINFO_CHIP_ID_MANUFACTURER_BITS)
        >> SYSINFO_CHIP_ID_MANUFACTURER_LSB;

    uint32_t part =
        (id & SYSINFO_CHIP_ID_PART_BITS)
        >> SYSINFO_CHIP_ID_PART_LSB;

    uint32_t revision =
        (id & SYSINFO_CHIP_ID_REVISION_BITS)
        >> SYSINFO_CHIP_ID_REVISION_LSB;

    printf("project: %s\n", DEVICE_PROJECT);
    printf("repo: %s\n", DEVICE_REPO);
    printf("board: %s\n", DEVICE_BOARD);
    printf("serial: %s\n", board_id);

    printf("chip: manufacturer 0x%03x, part 0x%04x, revision %u\n",
           (unsigned int)manufacturer,
           (unsigned int)part,
           (unsigned int)revision);
    printf("pico-sdk: %s\n", PICO_SDK_VERSION_STRING);
}

void dev_info(void)
{
    printf("struct          address     size offset value\n");
    printf("%-15s 0x%08x %5u\n", "device_card",
           (unsigned)(uintptr_t)&device_card, (unsigned)sizeof(device_card));
    printf("- %-13s 0x%08x %5u %6u 0x%08x\n", "version",
           (unsigned)(uintptr_t)&device_card.version,
           (unsigned)sizeof(device_card.version),
           (unsigned)offsetof(struct info_t, version), (unsigned)device_card.version);
    printf("- %-13s 0x%08x %5u %6u %s\n", "name",
           (unsigned)(uintptr_t)device_card.name,
           (unsigned)sizeof(device_card.name),
           (unsigned)offsetof(struct info_t, name), device_card.name);
    printf("- %-13s 0x%08x %5u %6u %u\n", "revision",
           (unsigned)(uintptr_t)&device_card.revision,
           (unsigned)sizeof(device_card.revision),
           (unsigned)offsetof(struct info_t, revision), (unsigned)device_card.revision);
    unsigned fields = sizeof(device_card.version) + sizeof(device_card.name)
                      + sizeof(device_card.revision);
    printf("fields %u, sizeof %u, padding %u\n", fields,
           (unsigned)sizeof(device_card), (unsigned)sizeof(device_card) - fields);
}
