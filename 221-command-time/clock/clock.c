#include "clock.h"

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/regs/clocks.h"
#include "log.h"

const uint32_t CLK_SYS_LOW_KHZ = 62500;

static void row(const char *name, uint32_t set_khz, uint32_t measured_khz)
{
    printf("%-8s %9u %12u\n", name, (unsigned)set_khz, (unsigned)measured_khz);
}

void clk_info(void)
{
    printf("signal     set_khz measured_khz\n");
    row("clk_ref", clock_get_hz(clk_ref) / 1000,
        frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_REF));
    row("clk_sys", clock_get_hz(clk_sys) / 1000,
        frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS));
    row("clk_peri", clock_get_hz(clk_peri) / 1000,
        frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_PERI));
    row("clk_usb", clock_get_hz(clk_usb) / 1000,
        frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_USB));
    row("clk_adc", clock_get_hz(clk_adc) / 1000,
        frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_ADC));
    printf("%-8s %9s %12u\n", "rosc", "-",
           (unsigned)frequency_count_khz(CLOCKS_FC0_SRC_VALUE_ROSC_CLKSRC));
}

void uptime(void)
{
    printf("uptime: %llu ms\n", (unsigned long long)(time_us_64() / 1000));
}

// В SDK 2.3.0 set_sys_clock_khz() вызывает set_sys_clock_pll() в
// src/rp2_common/hardware_clocks/clocks.c. При стандартном
// PICO_CLOCK_ADJUST_PERI_CLOCK_WITH_SYS_CLOCK = 0 clk_peri переходит
// на PLL USB (48 МГц) и остаётся там после восстановления clk_sys.
static void clk_sys_set(uint32_t khz)
{
    if (set_sys_clock_khz(khz, false))
    {
        LOG_INF("clk_sys %u kHz\n", (unsigned)khz);
    }
    else
    {
        LOG_ERR("clk_sys %u kHz is not set\n", (unsigned)khz);
    }
}

void clk_sys_low(void)
{
    clk_sys_set(CLK_SYS_LOW_KHZ);
}

void clk_sys_default(void)
{
    clk_sys_set(SYS_CLK_KHZ);
}
