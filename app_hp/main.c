#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include <alif.h>
#include <RTE_Components.h>
#include <app_mem_regions.h>
#include <se_services_port.h>
#include <sys_clocks.h>
#include <drv_bkram.h>
#include <drv_mhu.h>
#include <soc_clk.h>
#include <lptimer.h>
#include <pinconf.h>
#include <uart.h>
#include <pm.h>

#if defined(RTE_CMSIS_Compiler_STDIN) || defined(RTE_CMSIS_Compiler_STDOUT)
#define _UART_BASE_(n) UART##n##_BASE
#define UART_BASE(n) _UART_BASE_(n)
#include "retarget_init.h"
#include "retarget_config.h"
#endif

#define MHU_VAL 0x1234
volatile uint32_t mhu_rx_value;

volatile uint32_t ms_ticks;
void SysTick_Handler (void) { ms_ticks++; }
void delay_ms (uint32_t msec) { msec += ms_ticks; while(ms_ticks < msec) __WFI(); }

static void uart_init();
static void uart_update();
static void uart_deinit();

void MHU_RTSS_S_TX_IRQHandler()
{
    MHU_SENDER_regs *MHU = (MHU_SENDER_regs *) RTSS_TX_MHU0_BASE;
    uint32_t int_st = MHU->INT_ST;
    MHU->INT_CLR = int_st;
}

void MHU_RTSS_S_RX_IRQHandler()
{
    MHU_RECEIVER_regs *MHU = (MHU_RECEIVER_regs *) RTSS_RX_MHU0_BASE;
    uint32_t int_st = MHU->INT_ST;

    uint32_t check_val;
    MHU_RECEIVER_Read(RTSS_RX_MHU0_BASE, 0, &check_val);
    MHU_RECEIVER_Clear(RTSS_RX_MHU0_BASE, 0, check_val);
    mhu_rx_value = check_val;

    MHU->INT_CLR = int_st;

    uint32_t count;
    bk_ram_rd(&count, BKRAM_INDEX_HP_RX_CNT);
    count++;
    bk_ram_wr(&count, BKRAM_INDEX_HP_RX_CNT);
}

static bool GetPendingIRQ()
{
    uint32_t wic_pending = 0;
    wic_pending |= NVIC->ISPR[0];
    wic_pending |= NVIC->ISPR[1];

    /* nothing to do if IRQs 0-63 are clear */
    if (wic_pending == 0) return false;

    /* For example: only MHU0 RX should wake the HP core */
    if (NVIC_GetPendingIRQ(41)) {
        return true;
    }

    return false;
}

static void PrintPendingIRQ()
{
    /* Note: IRQ lines are shared in this multicore system,
     * you may see pending IRQs not meant for this core. */
    for (uint32_t i = 0; i < 64; i++) {
        if (NVIC_GetPendingIRQ(i)) {
            printf("IRQ%" PRIu32 " is pending\r\n", i);
        }
    }
}

static void boot_from_por()
{
    /* get status of clock tree */
    CoreClockUpdate();
    SystBusClkUpdate();

    ms_ticks = 0;
    SysTick_Config(SystemCoreClock/1000);
    uart_init();

    printf("RTSS-HP un-expected boot\r\n\n");
}

static void boot_from_stop()
{
    /* get status of clock tree */
    CoreClockUpdate();
    SystBusClkUpdate();

    ms_ticks = 0;
    SysTick_Config(SystemCoreClock/1000);
    uart_init();

    uint32_t cycle_cnt;
    bk_ram_rd(&cycle_cnt, BKRAM_INDEX_HP_CYCLES);
    cycle_cnt++;
    bk_ram_wr(&cycle_cnt, BKRAM_INDEX_HP_CYCLES);
    printf("RTSS-HP resume count: %" PRIu32 "\r\n", cycle_cnt);

    PrintPendingIRQ();
    NVIC_EnableIRQ(41);
    NVIC_EnableIRQ(42);

    uint32_t count;
    bk_ram_rd(&count, BKRAM_INDEX_HP_RX_CNT);
    printf("MHU interrupt count: %" PRIu32 " (RX)\r\n\n", count);
}

static void enter_stop()
{
    uart_deinit();
    while(1) pm_core_enter_deep_sleep_request_subsys_off();
}

static void execute_while1_rtsshp()
{
    uint32_t active_ms;
    bk_ram_rd(&active_ms, BKRAM_INDEX_WHILE1);

    /* while(1) */
    active_ms += ms_ticks;
    while(ms_ticks < active_ms);

    MHU_SENDER_Set(RTSS_TX_MHU0_BASE, 0, MHU_VAL);
}

int main (void)
{
    bool wake_event = GetPendingIRQ();
    if (wake_event) {
        boot_from_stop();
        execute_while1_rtsshp();
        enter_stop();
    }
    else {
        boot_from_por();
        enter_stop();
    }
    return 0;
}

static void uart_init()
{
#if defined(RTE_CMSIS_Compiler_STDIN_Custom)
    stdin_init();
#endif
#if defined(RTE_CMSIS_Compiler_STDOUT_Custom)
    stdout_init();
#endif
}

/* call this function after making clock tree changes */
static void uart_update()
{
#if defined(RTE_CMSIS_Compiler_STDIN_Custom) || defined(RTE_CMSIS_Compiler_STDOUT_Custom)
    uart_set_baudrate((UART_Type*)UART_BASE(PRINTF_UART_CONSOLE), SystemAPBClock, PRINTF_UART_CONSOLE_BAUD_RATE);
#endif
}

static void uart_deinit()
{
#if defined(RTE_CMSIS_Compiler_STDIN_Custom)
    pinconf_set(PRINTF_UART_CONSOLE_RX_PORT_NUM, PRINTF_UART_CONSOLE_RX_PIN,
        0, PADCTRL_DRIVER_DISABLED_PULL_UP);
#endif
#if defined(RTE_CMSIS_Compiler_STDOUT_Custom)
    UART_Type *uart = (UART_Type*)UART_BASE(PRINTF_UART_CONSOLE);
    while (!(uart->UART_LSR & UART_LSR_TRANSMITTER_EMPTY));
    pinconf_set(PRINTF_UART_CONSOLE_TX_PORT_NUM, PRINTF_UART_CONSOLE_TX_PIN,
        0, PADCTRL_DRIVER_DISABLED_PULL_UP);
#endif
}
