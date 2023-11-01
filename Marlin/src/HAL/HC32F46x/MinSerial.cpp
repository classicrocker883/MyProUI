#include "../platforms.h"

#ifdef TARGET_HC32F46x

#include "../../inc/MarlinConfigPre.h"

#if ENABLED(POSTMORTEM_DEBUGGING)

#ifndef CORE_DISABLE_FAULT_HANDLER
#error "POSTMORTEM_DEBUGGING requires CORE_DISABLE_FAULT_HANDLER to be set."
#else

#include "../shared/MinSerial.h"
#include <drivers/usart/usart_sync.h>
#include <drivers/panic/fault_handlers.h>

static void TXBegin()
{
#if !WITHIN(SERIAL_PORT, 1, 3)
#warning "Using POSTMORTEM_DEBUGGING requires a physical U(S)ART hardware in case of severe error."
#warning "Disabling the severe error reporting feature currently because the used serial port is not a HW port."
#else

    // prepare usart_sync configuration
    const stc_usart_uart_init_t usart_config = {
        .enClkMode = UsartIntClkCkNoOutput,
        .enClkDiv = UsartClkDiv_1,
        .enDataLength = UsartDataBits8,
        .enDirection = UsartDataLsbFirst,
        .enStopBit = UsartOneStopBit,
        .enParity = UsartParityNone,
        .enSampleMode = UsartSampleBit8,
        .enDetectMode = UsartStartBitFallEdge,
        .enHwFlow = UsartRtsEnable,
    };

    // initializes usart_sync driver
#define __USART_SYNC_INIT(port_no, baud, config) \
    usart_sync_init(M4_USART##port_no, \
        BOARD_USART##port_no##_TX_PIN, \
        baud, \
        config);
#define USART_SYNC_INIT(port_no, baud, config) __USART_SYNC_INIT(port_no, baud, config)

    // this will reset the baudrate to what is defined in Configuration.h, 
    // ignoring any changes made with e.g. M575.
    // keeping the dynamic baudrate would require re-calculating the baudrate 
    // using the register values, which is a pain...

    // TODO: retain dynamic baudrate in MinSerial init
    // -> see USART_SetBaudrate(), needs to be inverted
    USART_SYNC_INIT(SERIAL_PORT, BAUDRATE, &usart_config);
#endif
}

static void TX(char c)
{
#if WITHIN(SERIAL_PORT, 1, 3)
#define __USART_SYNC_PUTC(port_no, ch) \
    usart_sync_putc(M4_USART##port_no, ch);
#define USART_SYNC_PUTC(port_no, ch) __USART_SYNC_PUTC(port_no, ch)

    USART_SYNC_PUTC(SERIAL_PORT, c);
#endif
}


void fault_handlers_init()
{
    // enable cpu traps:
    // - divide by zero
    // - unaligned access
    SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk; //| SCB_CCR_UNALIGN_TRP_Msk;
}

void install_min_serial()
{
    HAL_min_serial_init = &TXBegin;
    HAL_min_serial_out = &TX;
}

extern "C"
{
    __attribute__((naked)) void JumpHandler_ASM()
    {
        __asm__ __volatile__(
            "b CommonHandler_ASM\n");
    }
    void __attribute__((naked, alias("JumpHandler_ASM"), nothrow)) HardFault_Handler();
    void __attribute__((naked, alias("JumpHandler_ASM"), nothrow)) BusFault_Handler();
    void __attribute__((naked, alias("JumpHandler_ASM"), nothrow)) UsageFault_Handler();
    void __attribute__((naked, alias("JumpHandler_ASM"), nothrow)) MemManage_Handler();
    void __attribute__((naked, alias("JumpHandler_ASM"), nothrow)) NMI_Handler();
}

#endif // CORE_DISABLE_FAULT_HANDLER
#endif // POSTMORTEM_DEBUGGING
#endif // TARGET_HC32F46x