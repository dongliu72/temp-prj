/******************************************************************************
*  Copyright 2017, Netlink Communication Corp.
*  ---------------------------------------------------------------------------
*  Statement:
*  ----------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Netlink Communication Corp. (C) 2017
******************************************************************************/

/******************************************************************************
*  Filename:
*  ---------
*  hal_system_patch.c
*
*  Project:
*  --------
*  NL1000_A1 series
*
*  Description:
*  ------------
*  This include file defines the patch proto-types of system functions
*
*  Author:
*  -------
*  Chung-Chun Wang
******************************************************************************/

/***********************
Head Block of The File
***********************/
// Sec 0: Comment block of the file

// Sec 1: Include File
#include "hal_system.h"
#include "hal_system_patch.h"
#include "hal_wdt.h"
#include "hal_spi.h"
#include "hal_i2c.h"
#include "hal_dbg_uart.h"

// Sec 2: Constant Definitions, Imported Symbols, miscellaneous
#define AOS             ((S_Aos_Reg_t *) AOS_BASE)
#define SYS_REG         ((S_Sys_Reg_t *) SYS_BASE)

// 0x004
#define AOS_SLP_MODE_EN              (1<<0)

//0x020
#define AOS_RET_SF_VOL_POS           0
#define AOS_RET_SF_VOL_MSK           (0xF << AOS_RET_SF_VOL_POS)
#define AOS_RET_SF_VOL_0P55          (0x0 << AOS_RET_SF_VOL_POS)
#define AOS_RET_SF_VOL_0P67          (0x3 << AOS_RET_SF_VOL_POS)
#define AOS_RET_SF_VOL_0P86          (0x8 << AOS_RET_SF_VOL_POS)
#define AOS_RET_SF_VOL_1P20          (0xF << AOS_RET_SF_VOL_POS)

#define AOS_APS_CLK_EN_I2C_PCLK      (1<<5)
#define AOS_APS_CLK_EN_TMR_0_PCLK    (1<<6)
#define AOS_APS_CLK_EN_TMR_1_PCLK    (1<<7)
#define AOS_APS_CLK_EN_WDT_PCLK      (1<<8)
#define AOS_APS_CLK_EN_SPI_0_PCLK    (1<<10)
#define AOS_APS_CLK_EN_SPI_1_PCLK    (1<<11)
#define AOS_APS_CLK_EN_SPI_2_PCLK    (1<<12)
#define AOS_APS_CLK_EN_UART_0_PCLK   (1<<13)
#define AOS_APS_CLK_EN_UART_1_PCLK   (1<<14)
#define AOS_APS_CLK_EN_DBG_UART_PCLK (1<<15)
#define AOS_APS_CLK_EN_PWM_CLK       (1<<26)
#define AOS_APS_CLK_EN_JTAG_HCLK     (1<<28)
#define AOS_APS_CLK_EN_WDT_INTERNAL  (1<<30)
#define WDT_TIMEOUT_SECS            10
#define STRAP_NORMAL_MODE       0xA

/********************************************
Declaration of data structure
********************************************/
// Sec 3: structure, uniou, enum, linked list...
typedef struct
{
    volatile uint32_t RET_MUX;            // 0x000
    volatile uint32_t MODE_CTL;           // 0x004
    volatile uint32_t OSC_SEL;            // 0x008
    volatile uint32_t SLP_TIMER_CURR_L;   // 0x00C
    volatile uint32_t SLP_TIMER_CURR_H;   // 0x010
    volatile uint32_t SLP_TIMER_PRESET_L; // 0x014
    volatile uint32_t SLP_TIMER_PRESET_H; // 0x018
    volatile uint32_t PS_TIMER_PRESET;    // 0x01C
    volatile uint32_t RET_SF_VAL_CTL;     // 0x020
    volatile uint32_t PMU_SF_VAL_CTL;     // 0x024
    volatile uint32_t HPBG_CTL;           // 0x028
    volatile uint32_t LPBG_CTL;           // 0x02C
    volatile uint32_t BUCK_CTL;           // 0x030
    volatile uint32_t ON1_TIME;           // 0x034
    volatile uint32_t ON2_TIME;           // 0x038
    volatile uint32_t ON3_TIME;           // 0x03C
    volatile uint32_t ON4_TIME;           // 0x040
    volatile uint32_t ON5_TIME;           // 0x044
    volatile uint32_t ON6_TIME;           // 0x048
    volatile uint32_t ON7_TIME;           // 0x04C
    volatile uint32_t CPOR_N_ON_TIME;     // 0x050
    volatile uint32_t reserve_054;        // 0x054, reserved
    volatile uint32_t SPS_TIMER_PRESET;   // 0x058
    volatile uint32_t SON1_TIME;          // 0x05C
    volatile uint32_t SON2_TIME;          // 0x060
    volatile uint32_t SON3_TIME;          // 0x064
    volatile uint32_t SON4_TIME;          // 0x068
    volatile uint32_t SON5_TIME;          // 0x06C
    volatile uint32_t SON6_TIME;          // 0x070
    volatile uint32_t SON7_TIME;          // 0x074
    volatile uint32_t SCPOR_N_ON_TIME;    // 0x078
    volatile uint32_t PU_CTL;             // 0x07C
    volatile uint32_t OSC_CTL;            // 0x080
    volatile uint32_t PMS_SPARE;          // 0x084, HW reservd for debug
    volatile uint32_t ADC_CTL;            // 0x088
    volatile uint32_t LDO_CTL;            // 0x08C
    volatile uint32_t RG_PD_IE;           // 0x090
    volatile uint32_t RG_PD_PE;           // 0x094
    volatile uint32_t RG_PD_O_INV;        // 0x098
    volatile uint32_t RG_PD_DS;           // 0x09C
    volatile uint32_t RG_GPO;             // 0x0A0
    volatile uint32_t RG_PD_I_INV;        // 0x0A4
    volatile uint32_t RG_PDOV_MODE;       // 0x0A8
    volatile uint32_t RG_PD_DIR;          // 0x0AC
    volatile uint32_t RG_PD_OENP_INV;     // 0x0B0
    volatile uint32_t RG_PDOC_MODE;       // 0x0B4
    volatile uint32_t RG_GPI;             // 0x0B8
    volatile uint32_t reserve_0bc;        // 0x0BC, reserved
    volatile uint32_t RG_PDI_SRC_IO_A;    // 0x0C0
    volatile uint32_t RG_PDI_SRC_IO_B;    // 0x0C4
    volatile uint32_t RG_PDI_SRC_IO_C;    // 0x0C8
    volatile uint32_t RG_PDI_SRC_IO_D;    // 0x0CC
    volatile uint32_t RG_PTS_INMUX_A;     // 0x0D0
    volatile uint32_t RG_PTS_INMUX_B;     // 0x0D4
    volatile uint32_t RG_PTS_INMUX_C;     // 0x0D8
    volatile uint32_t RG_PTS_INMUX_D;     // 0x0DC
    volatile uint32_t RG_SRAM_IOS_EN;     // 0x0E0
    volatile uint32_t RG_SRAM_RET_OFF;    // 0x0E4
    volatile uint32_t RG_PHY_WR_SRAM;     // 0x0E8
    volatile uint32_t RG_PHY_RD_SRAM;     // 0x0EC
    volatile uint32_t CAL_CEN;            // 0x0F0
    volatile uint32_t CAL_STR;            // 0x0F4
    volatile uint32_t SDM_PT_SEL;         // 0x0F8
    volatile uint32_t SDM_CTL;            // 0x0FC
    volatile uint32_t R_STRAP_MODE_CTL;   // 0x100
    volatile uint32_t R_APS_SWRST;        // 0x104
    volatile uint32_t R_MSQ_SWRST;        // 0x108
    volatile uint32_t RG_SPARE;           // 0x10C
    volatile uint32_t RG_PTS_INMUX_E;     // 0x110
    volatile uint32_t RG_PTS_INMUX_F;     // 0x114
    volatile uint32_t RG_SRAM_RET_ACK;    // 0x118
    volatile uint32_t RG_MSQ_ROM_MAP;     // 0x11C
    volatile uint32_t RG_AOS_ID;          // 0x120
    volatile uint32_t RG_SPARE_1;         // 0x124
    volatile uint32_t RG_RSTS;            // 0x128
    volatile uint32_t RG_SPARE_2;         // 0x12C
    volatile uint32_t RG_SPARE_3;         // 0x130
    volatile uint32_t R_M3CLK_SEL;        // 0x134
    volatile uint32_t R_M0CLK_SEL;        // 0x138
    volatile uint32_t R_RFPHY_SEL;        // 0x13C
    volatile uint32_t R_SCRT_EN;          // 0x140
    volatile uint32_t reserve_144[21];    // 0x144 ~ 0x194, move to sys_reg
    volatile uint32_t R_CLK_MMFACTOR_CM3; // 0x198
} S_Aos_Reg_t;

typedef struct
{
    volatile uint32_t reserve_r0[3];         // 0x000 ~ 0x008, reserved
    volatile uint32_t R_SRAM_BYPASS;         // 0x00C
    volatile uint32_t R_SW_RESET_EN;         // 0x010
    volatile uint32_t R_SW_DBG_EN;           // 0x014
    volatile uint32_t R_BOOT_STATUS;         // 0x018
    volatile uint32_t R_CHIP_ID;             // 0x01C
    volatile uint32_t reserve_r1[12];        // 0x020 ~ 0x04C, reserved
    volatile uint32_t R_CM3_I_PATCH[128];    // 0x050 ~ 0x24C
    volatile uint32_t R_CM3_I_PATCH_ST;      // 0x250
    volatile uint32_t R_CM3_I_PATCH_EN[4];   // 0x254 ~ 0x260
    volatile uint32_t R_CM3_D_P_ADDR[4];     // 0x268 ~ 0x270
    volatile uint32_t R_CM3_D_PATCH_EN;      // 0x274
    volatile uint32_t R_CM3_D_PATCH_DATA[4]; // 0x278 ~ 0x284
    volatile uint32_t reserve_r2[6];         // 0x288 ~ 0x29C, reserved
    volatile uint32_t R_CM0_I_PATCH[128];    // 0x2A0 ~ 0x49C
    volatile uint32_t R_CM0_I_PATCH_ST;      // 0x4A0
    volatile uint32_t R_CM0_I_PATCH_EN[4];   // 0x4A4 ~ 0x4B0
    volatile uint32_t R_CM3_ORIG_ADD[4];     // 0x4B4 ~ 0x4C0
    volatile uint32_t R_CM3_TAG_ADD[4];      // 0x4C4 ~ 0x4D0
    volatile uint32_t R_CM3_RMP_MASK[4];     // 0x4D4 ~ 0x4E0
    volatile uint32_t R_CM0_ORIG_ADD[3];     // 0x4E4 ~ 0x4EC
    volatile uint32_t R_CM0_TAG_ADD[3];      // 0x4F0 ~ 0x4F8
    volatile uint32_t R_CM0_RMP_MASK[3];     // 0x4FC ~ 0x504
} S_Sys_Reg_t;

/********************************************
Declaration of Global Variables & Functions
********************************************/
// Sec 4: declaration of global  variable
T_Hal_Sys_DisableClock Hal_Sys_DisableClock;

// Sec 5: declaration of global function prototype
/* Power relative */

/* Sleep Mode relative */

/* Pin-Mux relative*/
RET_DATA T_Hal_SysPinMuxM3UartInit   Hal_SysPinMuxM3UartInit;
RET_DATA T_Hal_SysPinMuxM3UartSwitch Hal_SysPinMuxM3UartSwitch;

/* Ret RAM relative*/

/* Xtal fast starup relative */

/* SW reset relative */

/* Clock relative */
extern uint32_t Hal_Sys_ApsClkTreeSetup_impl(E_ApsClkTreeSrc_t eClkTreeSrc, uint8_t u8ClkDivEn, uint8_t u8PclkDivEn );
extern uint32_t Hal_Sys_MsqClkTreeSetup_impl(E_MsqClkTreeSrc_t eClkTreeSrc, uint8_t u8ClkDivEn );
extern void Hal_Sys_ApsClkChangeApply_impl(void);
/* Remap relative */

/* Miscellaneous */

/***************************************************
Declaration of static Global Variables &  Functions
***************************************************/
// Sec 6: declaration of static global  variable

// Sec 7: declaration of static function prototype

/***********
C Functions
***********/
// Sec 8: C Functions
void Hal_Sys_SleepInit_patch(void)
{
    // Set RetRAM voltage
    AOS->RET_SF_VAL_CTL  = AOS_RET_SF_VOL_0P86;

    // Need make rising pulse. So clean bit first
    AOS->MODE_CTL &= ~AOS_SLP_MODE_EN;

    // HW provide setting to make pulse
    AOS->PS_TIMER_PRESET = 0x001A; // PS 0b' 11 1111 1111
    AOS->ON1_TIME        = 0x9401; // ON_1 0b' 0 | 1010 01 | 00 0010 0000
    AOS->ON2_TIME        = 0x9C02; // ON_2 0b' 0 | 1010 11 | 00 0100 0000
    AOS->ON3_TIME        = 0xDC03; // ON_3 0b' 0 | 1111 11 | 00 0110 0000
    AOS->ON4_TIME        = 0xDC03; // ON_4 0b' 0 | 1111 11 | 00 1000 0000
    AOS->ON5_TIME        = 0xDC03; // ON_5 0b' 0 | 1111 11 | 00 1010 0000
    AOS->ON6_TIME        = 0xDC03; // ON_6 0b' 0 | 1101 11 | 00 1100 0000
    AOS->ON7_TIME        = 0x6C09; // ON_7 0b' 0 | 0010 11 | 11 1111 1110
    AOS->CPOR_N_ON_TIME  = 0x041A; // CPOR 0b' 0100 1111 1111 1110

    AOS->SPS_TIMER_PRESET = 0x0006; // SPS 0b' 00 0000 1011 0000
    AOS->SON1_TIME        = 0xDC01; // SON_1 0b' 1 | 1101 11 | 00 0010 0000
    AOS->SON2_TIME        = 0xDC02; // SON_2 0b' 1 | 1111 11 | 00 0100 0000
    AOS->SON3_TIME        = 0x9C03; // SON_3 0b' 0 | 1111 11 | 00 0100 0101
    AOS->SON4_TIME        = 0x9404; // SON_4 0b' 0 | 1111 11 | 00 0101 0000
    AOS->SON5_TIME        = 0x9005; // SON_5 0b' 0 | 1011 11 | 00 0110 0000
    AOS->SON6_TIME        = 0x9005; // SON_6 0b' 0 | 1011 01 | 00 1000 0000
    AOS->SON7_TIME        = 0x9005; // SON_7 0b' 0 | 1011 00 | 00 1010 0000
    AOS->SCPOR_N_ON_TIME  = 0x0006; //SCPOR 0b' 0000 0000 0100 0000
}

uint32_t Hal_Sys_ApsClkTreeSetup_patch(E_ApsClkTreeSrc_t eClkTreeSrc, uint8_t u8ClkDivEn, uint8_t u8PclkDivEn )
{
    if(eClkTreeSrc == ASP_CLKTREE_SRC_RC_BB)
    {
        // make sure RC clock enable, due to RF turn off RC
        *(volatile uint32_t *)0x40009048 |= (1 << 11) | (1 << 14);
        *(volatile uint32_t *)0x40009090 |= (1 << 13);
    }

    // Orignal code
    return Hal_Sys_ApsClkTreeSetup_impl(eClkTreeSrc, u8ClkDivEn, u8PclkDivEn);
}

uint32_t Hal_Sys_MsqClkTreeSetup_patch(E_MsqClkTreeSrc_t eClkTreeSrc, uint8_t u8ClkDivEn )
{
    if(eClkTreeSrc == MSQ_CLKTREE_SRC_RC)
    {
        // make sure RC clock enable, due to RF turn off RC
        *(volatile uint32_t *)0x40009048 |= (1 << 11) | (1 << 14);
        *(volatile uint32_t *)0x40009090 |= (1 << 13);
    }

    // Orignal code
    return Hal_Sys_MsqClkTreeSetup_impl( eClkTreeSrc, u8ClkDivEn );
}

/*************************************************************************
* FUNCTION:
*  Hal_SysPinMuxAppInit
*
* DESCRIPTION:
*   1. Pin-Mux initial for application stage
*   2. Related reg.: AOS 0x090 ~ 0x0DC
* CALLS
*
* PARAMETERS
*   None
* RETURNS
*   None
* GLOBALS AFFECTED
*
*************************************************************************/
void Hal_SysPinMuxAppInit_patch(void)
{
    volatile uint32_t tmp;

    Hal_SysPinMuxM3UartInit();

// SPI0 standard mode
    // IO12(CS), IO13(CLK), IO14(MOSI), IO15(MISO)
    // output source
    tmp = AOS->RG_PDI_SRC_IO_B;
    tmp &= ~(((uint32_t)0xF << 28) | (0xF << 24) | (0xF << 20) | (0xF << 16));
    tmp |= ((0x0 << 28) | (0x0 << 24) | (0x0 << 20) | (0x0 << 16));
    AOS->RG_PDI_SRC_IO_B = tmp;
    // input IO
    tmp = AOS->RG_PTS_INMUX_C;
    tmp &= ~((0xF << 4) | (0xF << 0));
    tmp |= ((0x0 << 4) | (0x0 << 0));
    AOS->RG_PTS_INMUX_C = tmp;

    // input Enable
    tmp = AOS->RG_PD_IE;
    tmp |= ((0x1 << 15) | (0x1 << 14) | (0x1 << 13) | (0x1 << 12));
    AOS->RG_PD_IE = tmp;

    // pull-up / pull-down
    tmp = AOS->RG_PD_PE;
    tmp &= ~((0x1 << 15) | (0x1 << 14) | (0x1 << 13) | (0x1 << 12));
    AOS->RG_PD_PE = tmp;

    // function pin
    tmp = AOS->RG_PDOC_MODE;
    tmp |= ((0x1 << 15) | (0x1 << 14) | (0x1 << 13) | (0x1 << 12));
    AOS->RG_PDOC_MODE = tmp;

    tmp = AOS->RG_PDOV_MODE;
    tmp |= ((0x1 << 15) | (0x1 << 14) | (0x1 << 13) | (0x1 << 12));
    AOS->RG_PDOV_MODE = tmp;

// MSQ_dbg_uart
    // IO16(RX), IO17(TX)
    // output source
    tmp = AOS->RG_PDI_SRC_IO_C;
    tmp &= ~((0xF << 4) | (0xF << 0));
    tmp |= ((0xB << 4) | (0xD << 0));
    AOS->RG_PDI_SRC_IO_C = tmp;
    // input IO
    tmp = AOS->RG_PTS_INMUX_A;
    tmp &= ~(0xF << 20);
    tmp |= (0x8 << 20);
    AOS->RG_PTS_INMUX_A = tmp;

    // input Enable
    tmp = AOS->RG_PD_IE;
    tmp |= ((0x1 << 17) | (0x1 << 16));
    AOS->RG_PD_IE = tmp;

    // pull-up / pull-down
    tmp = AOS->RG_PD_PE;
    tmp &= ~(0x1 << 17);
    tmp |= (0x1 << 16);
    AOS->RG_PD_PE = tmp;

    // function pin
    tmp = AOS->RG_PDOC_MODE;
    tmp |= ((0x1 << 17) | (0x1 << 16));
    AOS->RG_PDOC_MODE = tmp;

    tmp = AOS->RG_PDOV_MODE;
    tmp |= ((0x1 << 17) | (0x1 << 16));
    AOS->RG_PDOV_MODE = tmp;

// M0 SWD
    // IO18(CLK), IO19(DAT)
    // output source
    tmp = AOS->RG_PDI_SRC_IO_C;
    tmp &= ~((0xF << 12) | (0xF << 8));
    tmp |= ((0x9 << 12) | (0xD << 8));
    AOS->RG_PDI_SRC_IO_C = tmp;
    // input IO
    tmp = AOS->RG_PTS_INMUX_B;
    tmp &= ~((0xF << 24) | (0xF << 20));
    tmp |= ((0x9 << 24) | (0x9 << 20));
    AOS->RG_PTS_INMUX_B = tmp;

    // input Enable
    tmp = AOS->RG_PD_IE;
    tmp |= ((0x1 << 19) | (0x1 << 18));
    AOS->RG_PD_IE = tmp;

    // pull-up / pull-down
    tmp = AOS->RG_PD_PE;
    tmp &= ~((0x1 << 19) | (0x1 << 18));
    AOS->RG_PD_PE = tmp;

    // function pin
    tmp = AOS->RG_PDOC_MODE;
    tmp |= ((0x1 << 19) | (0x1 << 18));
    AOS->RG_PDOC_MODE = tmp;

    tmp = AOS->RG_PDOV_MODE;
    tmp |= ((0x1 << 19) | (0x1 << 18));
    AOS->RG_PDOV_MODE = tmp;

// M3 SWD
    // IO20(DAT), IO21(CLK)
    // output source
    tmp = AOS->RG_PDI_SRC_IO_C;
    tmp &= ~((0xF << 20) | (0xF << 16));
    tmp |= ((0xD << 20) | (0x8 << 16));
    AOS->RG_PDI_SRC_IO_C = tmp;
    // input IO
    tmp = AOS->RG_PTS_INMUX_B;
    tmp &= ~((0xF << 16) | (0xF << 12));
    tmp |= ((0xA << 16) | (0xA << 12));
    AOS->RG_PTS_INMUX_B = tmp;

    // input Enable
    tmp = AOS->RG_PD_IE;
    tmp |= ((0x1 << 21) | (0x1 << 20));
    AOS->RG_PD_IE = tmp;

    // pull-up / pull-down
    tmp = AOS->RG_PD_PE;
    tmp &= ~((0x1 << 21) | (0x1 << 20));
    AOS->RG_PD_PE = tmp;

    // function pin
    tmp = AOS->RG_PDOC_MODE;
    tmp |= ((0x1 << 21) | (0x1 << 20));
    AOS->RG_PDOC_MODE = tmp;

    tmp = AOS->RG_PDOV_MODE;
    tmp |= ((0x1 << 21) | (0x1 << 20));
    AOS->RG_PDOV_MODE = tmp;

    // M3 SWD enable
    Hal_Sys_SwDebugEn(0x1);
}

/*************************************************************************
* FUNCTION:
*  Hal_SysPinMuxDownloadInit
*
* DESCRIPTION:
*   1. Pin-Mux initial for download stage
*   2. Related reg.: AOS 0x090 ~ 0x0DC
* CALLS
*
* PARAMETERS
*   None
* RETURNS
*   None
* GLOBALS AFFECTED
*
*************************************************************************/
void Hal_SysPinMuxDownloadInit_patch(void)
{
    Hal_SysPinMuxM3UartSwitch();
    Hal_SysPinMuxSpiFlashInit();
}

/*************************************************************************
* FUNCTION:
*  Hal_SysPinMuxM3UartInit
*
* DESCRIPTION:
*   1. Pin-Mux for initial stage
*   2. Related reg.: AOS 0x090 ~ 0x0DC
* CALLS
*
* PARAMETERS
*   None
* RETURNS
*   None
* GLOBALS AFFECTED
*
*************************************************************************/
void Hal_SysPinMuxM3UartInit_impl(void)
{
    volatile uint32_t tmp;

// UART1
    // IO0(TX), IO1(RX), IO6(RTS), IO7(CTS)
    // output source
    tmp = AOS->RG_PDI_SRC_IO_A;
    tmp &= ~(((uint32_t)0xF << 28) | (0xF << 24) | (0xF << 4) | (0xF << 0));
    tmp |= (((uint32_t)0xD << 28) | (0x3 << 24) | (0xD << 4) | (0x4 << 0));
    AOS->RG_PDI_SRC_IO_A = tmp;
    // input IO
    tmp = AOS->RG_PTS_INMUX_A;
    tmp &= ~((0xF << 12) | (0xF << 8));
    tmp |= ((0x0 << 12) | (0x0 << 8));
    AOS->RG_PTS_INMUX_A = tmp;

    // input Enable
    tmp = AOS->RG_PD_IE;
    tmp |= ((0x1 << 7) | (0x1 << 6) | (0x1 << 1) | (0x1 << 0));
    AOS->RG_PD_IE = tmp;

    // pull-up / pull-down
    tmp = AOS->RG_PD_PE;
    tmp &= ~((0x1 << 6) | (0x1 << 0));
    tmp |= ((0x1 << 7) | (0x1 << 1));
    AOS->RG_PD_PE = tmp;

    // function pin
    tmp = AOS->RG_PDOC_MODE;
    tmp |= ((0x1 << 7) | (0x1 << 6) | (0x1 << 1) | (0x1 << 0));
    AOS->RG_PDOC_MODE = tmp;

    tmp = AOS->RG_PDOV_MODE;
    tmp |= ((0x1 << 7) | (0x1 << 6) | (0x1 << 1) | (0x1 << 0));
    AOS->RG_PDOV_MODE = tmp;

#if (SYS_PINMUX_TYPE == SYS_PINMUX_OPTION_1)
// APS_dbg_uart
    // IO8(TX), IO9(RX)
    // output source
    tmp = AOS->RG_PDI_SRC_IO_B;
    tmp &= ~((0xF << 4) | (0xF << 0));
    tmp |= ((0xD << 4) | (0xA << 0));
    AOS->RG_PDI_SRC_IO_B = tmp;
    // input IO
    tmp = AOS->RG_PTS_INMUX_A;
    tmp &= ~(0xF << 16);
    tmp |= (0x4 << 16);
    AOS->RG_PTS_INMUX_A = tmp;

    // input Enable
    tmp = AOS->RG_PD_IE;
    tmp |= ((0x1 << 9) | (0x1 << 8));
    AOS->RG_PD_IE = tmp;

    // pull-up / pull-down
    tmp = AOS->RG_PD_PE;
    tmp &= ~(0x1 << 8);
    tmp |= (0x1 << 9);
    AOS->RG_PD_PE = tmp;

    // function pin
    tmp = AOS->RG_PDOC_MODE;
    tmp |= ((0x1 << 9) | (0x1 << 8));
    AOS->RG_PDOC_MODE = tmp;

    tmp = AOS->RG_PDOV_MODE;
    tmp |= ((0x1 << 9) | (0x1 << 8));
    AOS->RG_PDOV_MODE = tmp;

#elif (SYS_PINMUX_TYPE == SYS_PINMUX_OPTION_2)
// APS_dbg_uart
    // IO4(TX), IO5(RX)
    // output source
    tmp = AOS->RG_PDI_SRC_IO_A;
    tmp &= ~((0xF << 20) | (0xF << 16));
    tmp |= ((0xD << 20) | (0xA << 16));
    AOS->RG_PDI_SRC_IO_A = tmp;
    // input IO
    tmp = AOS->RG_PTS_INMUX_A;
    tmp &= ~(0xF << 16);
    tmp |= (0x2 << 16);
    AOS->RG_PTS_INMUX_A = tmp;

    // input Enable
    tmp = AOS->RG_PD_IE;
    tmp |= ((0x1 << 5) | (0x1 << 4));
    AOS->RG_PD_IE = tmp;

    // pull-up / pull-down
    tmp = AOS->RG_PD_PE;
    tmp &= ~(0x1 << 4);
    tmp |= (0x1 << 5);
    AOS->RG_PD_PE = tmp;

    // function pin
    tmp = AOS->RG_PDOC_MODE;
    tmp |= ((0x1 << 5) | (0x1 << 4));
    AOS->RG_PDOC_MODE = tmp;

    tmp = AOS->RG_PDOV_MODE;
    tmp |= ((0x1 << 5) | (0x1 << 4));
    AOS->RG_PDOV_MODE = tmp;

// UART0
    // IO2(TX), IO3(RX), IO8(CTS), IO9(RTS)
    // output source
    // IO2, IO3
    tmp = AOS->RG_PDI_SRC_IO_A;
    tmp &= ~((0xF << 12) | (0xF << 8));
    tmp |= ((0xD << 12) | (0x3 << 8));
    AOS->RG_PDI_SRC_IO_A = tmp;
    // IO8, IO9
    tmp = AOS->RG_PDI_SRC_IO_B;
    tmp &= ~((0xF << 4) | (0xF << 0));
    tmp |= ((0x3 << 4) | (0xD << 0));
    AOS->RG_PDI_SRC_IO_B = tmp;
    // input IO
    tmp = AOS->RG_PTS_INMUX_A;
    tmp &= ~((0xF << 4) | (0xF << 0));
    tmp |= ((0x0 << 4) | (0x1 << 0));
    AOS->RG_PTS_INMUX_A = tmp;

    // input Enable
    tmp = AOS->RG_PD_IE;
    tmp |= ((0x1 << 9) | (0x1 << 8) | (0x1 << 3) | (0x1 << 2));
    AOS->RG_PD_IE = tmp;

    // pull-up / pull-down
    tmp = AOS->RG_PD_PE;
    tmp &= ~((0x1 << 9) | (0x1 << 2));
    tmp |= ((0x1 << 8) | (0x1 << 3));
    AOS->RG_PD_PE = tmp;

    // function pin
    tmp = AOS->RG_PDOC_MODE;
    tmp |= ((0x1 << 9) | (0x1 << 8) | (0x1 << 3) | (0x1 << 2));
    AOS->RG_PDOC_MODE = tmp;

    tmp = AOS->RG_PDOV_MODE;
    tmp |= ((0x1 << 9) | (0x1 << 8) | (0x1 << 3) | (0x1 << 2));
    AOS->RG_PDOV_MODE = tmp;

#endif
}

/*************************************************************************
* FUNCTION:
*  Hal_SysPinMuxM3UartSwitch
*
* DESCRIPTION:
*   1. Pin-Mux for download stage
*   2. Related reg.: AOS 0x090 ~ 0x0DC
* CALLS
*
* PARAMETERS
*   None
* RETURNS
*   None
* GLOBALS AFFECTED
*
*************************************************************************/
void Hal_SysPinMuxM3UartSwitch_impl(void)
{
    volatile uint32_t tmp;

// APS_dbg_uart
    // IO0(TX), IO1(RX)
    // output source
    tmp = AOS->RG_PDI_SRC_IO_A;
    tmp &= ~((0xF << 4) | (0xF << 0));
    tmp |= ((0xD << 4) | (0xA << 0));
    AOS->RG_PDI_SRC_IO_A = tmp;
    // input IO
    tmp = AOS->RG_PTS_INMUX_A;
    tmp &= ~(0xF << 16);
    tmp |= (0x0 << 16);
    AOS->RG_PTS_INMUX_A = tmp;

    // input Enable
    tmp = AOS->RG_PD_IE;
    tmp |= ((0x1 << 1) | (0x1 << 0));
    AOS->RG_PD_IE = tmp;

    // pull-up / pull-down
    tmp = AOS->RG_PD_PE;
    tmp &= ~(0x1 << 0);
    tmp |= (0x1 << 1);
    AOS->RG_PD_PE = tmp;

    // function pin
    tmp = AOS->RG_PDOC_MODE;
    tmp |= ((0x1 << 1) | (0x1 << 0));
    AOS->RG_PDOC_MODE = tmp;

    tmp = AOS->RG_PDOV_MODE;
    tmp |= ((0x1 << 1) | (0x1 << 0));
    AOS->RG_PDOV_MODE = tmp;

#if (SYS_PINMUX_TYPE == SYS_PINMUX_OPTION_1)
// UART1
    // IO6(RTS), IO7(CTS), IO8(TX), IO9(RX)
    // output source
    // IO6, IO7
    tmp = AOS->RG_PDI_SRC_IO_A;
    tmp &= ~(((uint32_t)0xF << 28) | (0xF << 24));
    tmp |= (((uint32_t)0xD << 28) | (0x3 << 24));
    AOS->RG_PDI_SRC_IO_A = tmp;
    // IO8, IO9
    tmp = AOS->RG_PDI_SRC_IO_B;
    tmp &= ~((0xF << 4) | (0xF << 0));
    tmp |= ((0xD << 4) | (0x4 << 0));
    AOS->RG_PDI_SRC_IO_B = tmp;
    // input IO
    tmp = AOS->RG_PTS_INMUX_A;
    tmp &= ~((0xF << 12) | (0xF << 8));
    tmp |= ((0x2 << 12) | (0x0 << 8));
    AOS->RG_PTS_INMUX_A = tmp;

    // input Enable
    tmp = AOS->RG_PD_IE;
    tmp |= ((0x1 << 9) | (0x1 << 8) | (0x1 << 7) | (0x1 << 6));
    AOS->RG_PD_IE = tmp;

    // pull-up / pull-down
    tmp = AOS->RG_PD_PE;
    tmp &= ~((0x1 << 8) | (0x1 << 6));
    tmp |= ((0x1 << 9) | (0x1 << 7));
    AOS->RG_PD_PE = tmp;

    // function pin
    tmp = AOS->RG_PDOC_MODE;
    tmp |= ((0x1 << 9) | (0x1 << 8) | (0x1 << 7) | (0x1 << 6));
    AOS->RG_PDOC_MODE = tmp;

    tmp = AOS->RG_PDOV_MODE;
    tmp |= ((0x1 << 9) | (0x1 << 8) | (0x1 << 7) | (0x1 << 6));
    AOS->RG_PDOV_MODE = tmp;

#elif (SYS_PINMUX_TYPE == SYS_PINMUX_OPTION_2)
// UART1
    // IO4(TX), IO5(RX), IO6(RTS), IO7(CTS)
    // output source
    tmp = AOS->RG_PDI_SRC_IO_A;
    tmp &= ~(((uint32_t)0xF << 28) | (0xF << 24) | (0xF << 20) | (0xF << 16));
    tmp |= (((uint32_t)0xD << 28) | (0x3 << 24) | (0xD << 20) | (0x3 << 16));
    AOS->RG_PDI_SRC_IO_A = tmp;
    // input IO
    tmp = AOS->RG_PTS_INMUX_A;
    tmp &= ~((0xF << 12) | (0xF << 8));
    tmp |= ((0x1 << 12) | (0x0 << 8));
    AOS->RG_PTS_INMUX_A = tmp;

    // input Enable
    tmp = AOS->RG_PD_IE;
    tmp |= ((0x1 << 7) | (0x1 << 6) | (0x1 << 5) | (0x1 << 4));
    AOS->RG_PD_IE = tmp;

    // pull-up / pull-down
    tmp = AOS->RG_PD_PE;
    tmp &= ~((0x1 << 6) | (0x1 << 4));
    tmp |= ((0x1 << 7) | (0x1 << 5));
    AOS->RG_PD_PE = tmp;

    // function pin
    tmp = AOS->RG_PDOC_MODE;
    tmp |= ((0x1 << 7) | (0x1 << 6) | (0x1 << 5) | (0x1 << 4));
    AOS->RG_PDOC_MODE = tmp;

    tmp = AOS->RG_PDOV_MODE;
    tmp |= ((0x1 << 7) | (0x1 << 6) | (0x1 << 5) | (0x1 << 4));
    AOS->RG_PDOV_MODE = tmp;

#endif
}

/*************************************************************************
* FUNCTION:
*  Hal_Sys_ApsClkChangeApply
*
* DESCRIPTION:
*   1. Update all system clock relative
* CALLS
*
* PARAMETERS
*   None
* RETURNS
*   None
* GLOBALS AFFECTED
*
*************************************************************************/
void Hal_Sys_ApsClkChangeApply_patch(void)
{
    // FreeRTOS, update system tick.
    SysTick->LOAD =( SystemCoreClockGet()/1000 ) - 1;

    if (AOS->R_M3CLK_SEL & AOS_APS_CLK_EN_DBG_UART_PCLK)
        Hal_DbgUart_BaudRateSet( Hal_DbgUart_BaudRateGet() );
    if (AOS->R_M3CLK_SEL & AOS_APS_CLK_EN_SPI_0_PCLK)
        Hal_Spi_BaudRateSet(SPI_IDX_0, Hal_Spi_BaudRateGet( SPI_IDX_0 ) );
    if (AOS->R_M3CLK_SEL & AOS_APS_CLK_EN_SPI_1_PCLK)
        Hal_Spi_BaudRateSet(SPI_IDX_1, Hal_Spi_BaudRateGet( SPI_IDX_1 ) );
    if (AOS->R_M3CLK_SEL & AOS_APS_CLK_EN_SPI_2_PCLK)
        Hal_Spi_BaudRateSet(SPI_IDX_2, Hal_Spi_BaudRateGet( SPI_IDX_2 ) );
    if (AOS->R_M3CLK_SEL & AOS_APS_CLK_EN_I2C_PCLK)
        Hal_I2c_SpeedSet( Hal_I2c_SpeedGet() );
    // WDT
    if (AOS->R_M3CLK_SEL & AOS_APS_CLK_EN_WDT_PCLK)
    Hal_Wdt_Feed(WDT_TIMEOUT_SECS * SystemCoreClockGet());
}
void Hal_Sys_DisableClock_impl(void)
{
    uint32_t u32DisClk;
    u32DisClk = AOS_APS_CLK_EN_I2C_PCLK |
                AOS_APS_CLK_EN_TMR_0_PCLK |
                AOS_APS_CLK_EN_TMR_1_PCLK |
                AOS_APS_CLK_EN_WDT_PCLK |
                AOS_APS_CLK_EN_SPI_0_PCLK |
                AOS_APS_CLK_EN_SPI_1_PCLK |
                AOS_APS_CLK_EN_SPI_2_PCLK |
                AOS_APS_CLK_EN_UART_0_PCLK |
                AOS_APS_CLK_EN_UART_1_PCLK |
                AOS_APS_CLK_EN_DBG_UART_PCLK |
                AOS_APS_CLK_EN_PWM_CLK |
                AOS_APS_CLK_EN_WDT_INTERNAL;
    if (Hal_Sys_StrapModeRead() == STRAP_NORMAL_MODE)
        u32DisClk |= AOS_APS_CLK_EN_JTAG_HCLK;
    AOS->R_M3CLK_SEL = AOS->R_M3CLK_SEL & ~u32DisClk;    
}
