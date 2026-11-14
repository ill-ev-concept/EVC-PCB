/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define BQ76952_ADDR            0x10    // TI 8-bit write address (7-bit 0x08 << 1). HAL uses this value.

#define BQ_CMD_SUBCMD_LOW       0x3E    // subcommand address register
#define BQ_CMD_XFER_BUFFER      0x40    // subcommand data buffer

#define BQ_SUBCMD_DEVICE_NUMBER 0x0001
#define BQ76952_EXPECTED_DEVNUM 0x7695  // BQ76942 would be 0x7694

/* --- FET Bring-up & Configuration Defines --- */
#define BQ_RAM_MFG_STATUS_INIT   0x9343
#define BQ_RAM_CHG_PUMP_CONTROL  0x9309
#define BQ_RAM_FET_OPTIONS       0x9308
#define BQ_RAM_VCELL_MODE        0x9302
#define BQ_RAM_ENABLED_PROT_A    0x9261
#define BQ_RAM_DA_CONFIG         0x929E

#define BQ_CMD_FET_STATUS        0x7F
#define BQ_CMD_SAFETY_STATUS_A   0x03
#define BQ_CMD_SAFETY_STATUS_B   0x05
#define BQ_CMD_SAFETY_STATUS_C   0x07
#define BQ_CMD_BATTERY_STATUS    0x12

#define BQ_SUBCMD_ALL_FETS_OFF   0x0095
#define BQ_SUBCMD_ALL_FETS_ON    0x0096
#define BQ_SUBCMD_DSG_PDSG_OFF   0x0093
#define BQ_SUBCMD_CHG_PCHG_OFF   0x0094
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void UART_Print(const char *msg)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
}

/*
 * Issue a subcommand and read back 2 bytes from the transfer buffer.
 * Subcommand is written little-endian to 0x3E/0x3F, result appears at 0x40.
 */
static uint32_t bq_last_err = 0;   // I2C error code captured before recovery clears it
static int bq_crc_bad = 0;         // set when the last read failed its CRC check

#define BQ_MAX_BLOCK 32            // max data bytes per block transfer

/* Re-initialise I2C1 after an error. The F103 I2C peripheral can get stuck
 * (BUSY flag) after a NACK, so reset it before retrying. */
static void BQ_I2C_Recover(void)
{
    HAL_I2C_DeInit(&hi2c1);
    HAL_Delay(2);
    HAL_I2C_Init(&hi2c1);
}

/* CRC-8, polynomial x^8+x^2+x+1 (0x07), initial value 0 (per BQ76952 datasheet) */
static uint8_t crc8_bq(const uint8_t *buf, uint8_t len)
{
    uint8_t crc = 0;

    while (len--)
    {
        crc ^= *buf++;
        for (int i = 0; i < 8; i++)
        {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

/*
 * CRC-framed block write starting at register 'reg'.
 * Wire format after the register address: [d0][crc0][d1][crc1]...
 *   crc0 covers {device write addr, reg, d0}; every later crc covers only its own data byte.
 */
static HAL_StatusTypeDef BQ_WriteBlock(uint8_t reg, const uint8_t *data, uint8_t len)
{
    uint8_t frame[2 * BQ_MAX_BLOCK];

    if (len == 0 || len > BQ_MAX_BLOCK)
    {
        return HAL_ERROR;
    }

    for (uint8_t i = 0; i < len; i++)
    {
        frame[2 * i] = data[i];

        if (i == 0)
        {
            uint8_t hdr[3] = { BQ76952_ADDR, reg, data[0] };
            frame[1] = crc8_bq(hdr, 3);
        }
        else
        {
            frame[2 * i + 1] = crc8_bq(&data[i], 1);
        }
    }

    return HAL_I2C_Mem_Write(&hi2c1, BQ76952_ADDR, reg,
                             I2C_MEMADD_SIZE_8BIT, frame, 2 * len, 100);
}

/*
 * CRC-checked block read starting at register 'reg'.
 * Wire format returned: [d0][crc0][d1][crc1]...
 *   crc0 covers {write addr, reg, read addr, d0}; every later crc covers only its own data byte.
 * Returns HAL_ERROR (and sets bq_crc_bad) if any CRC does not match.
 */
static HAL_StatusTypeDef BQ_ReadBlock(uint8_t reg, uint8_t *data, uint8_t len)
{
    uint8_t rx[2 * BQ_MAX_BLOCK];
    HAL_StatusTypeDef status;

    bq_crc_bad = 0;

    if (len == 0 || len > BQ_MAX_BLOCK)
    {
        return HAL_ERROR;
    }

    status = HAL_I2C_Mem_Read(&hi2c1, BQ76952_ADDR, reg,
                              I2C_MEMADD_SIZE_8BIT, rx, 2 * len, 100);
    if (status != HAL_OK)
    {
        return status;
    }

    for (uint8_t i = 0; i < len; i++)
    {
        uint8_t expected;

        if (i == 0)
        {
            uint8_t hdr[4] = { BQ76952_ADDR, reg, (uint8_t)(BQ76952_ADDR | 0x01), rx[0] };
            expected = crc8_bq(hdr, 4);
        }
        else
        {
            expected = crc8_bq(&rx[2 * i], 1);
        }

        if (expected != rx[2 * i + 1])
        {
            bq_crc_bad = 1;
            return HAL_ERROR;
        }

        data[i] = rx[2 * i];
    }

    return HAL_OK;
}

/* Returns 0 = OK, 1 = write failed, 2 = read failed, 3 = CRC mismatch on read (after 3 attempts) */
static int BQ_ReadSubcommandU16(uint16_t subcmd, uint16_t *value)
{
    uint8_t cmd[2] = { (uint8_t)(subcmd & 0xFF), (uint8_t)(subcmd >> 8) };
    uint8_t data[2] = {0};
    int stage = 0;

    for (int attempt = 0; attempt < 3; attempt++)
    {
        if (BQ_WriteBlock(BQ_CMD_SUBCMD_LOW, cmd, 2) != HAL_OK)
        {
            stage = 1;
            bq_last_err = HAL_I2C_GetError(&hi2c1);
            BQ_I2C_Recover();
            HAL_Delay(10);
            continue;
        }

        HAL_Delay(5);   // give the device time to fill the transfer buffer

        if (BQ_ReadBlock(BQ_CMD_XFER_BUFFER, data, 2) != HAL_OK)
        {
            if (bq_crc_bad)
            {
                stage = 3;
            }
            else
            {
                stage = 2;
                bq_last_err = HAL_I2C_GetError(&hi2c1);
                BQ_I2C_Recover();
            }
            HAL_Delay(10);
            continue;
        }

        *value = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
        return 0;
    }

    return stage;
}

#define BQ_CMD_STACK_VOLTAGE    0x34    // top-of-stack voltage, userV units (default 10 mV per count)
#define BQ_CMD_INT_TEMP         0x68    // internal die temperature, 0.1 K per count

/* CRC-checked direct command read of one 16-bit little-endian value, with retries */
static HAL_StatusTypeDef BQ_ReadDirectU16(uint8_t cmd, uint16_t *value)
{
    uint8_t data[2] = {0};

    for (int attempt = 0; attempt < 3; attempt++)
    {
        if (BQ_ReadBlock(cmd, data, 2) == HAL_OK)
        {
            *value = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
            return HAL_OK;
        }

        if (!bq_crc_bad)
        {
            bq_last_err = HAL_I2C_GetError(&hi2c1);
            BQ_I2C_Recover();
        }
        HAL_Delay(10);
    }

    return HAL_ERROR;
}

#define BQ_SUBCMD_SET_CFGUPDATE   0x0090
#define BQ_SUBCMD_EXIT_CFGUPDATE  0x0092
#define BQ_BATSTATUS_CFGUPDATE    0x0001    // Battery Status bit 0

/* Send a command-only subcommand (no data read back), CRC-framed */
static HAL_StatusTypeDef BQ_SendSubcommand(uint16_t subcmd)
{
    uint8_t cmd[2] = { (uint8_t)(subcmd & 0xFF), (uint8_t)(subcmd >> 8) };
    return BQ_WriteBlock(BQ_CMD_SUBCMD_LOW, cmd, 2);
}

/* Read 'len' bytes of RAM register space starting at 'addr' */
static HAL_StatusTypeDef BQ_RamRead(uint16_t addr, uint8_t *data, uint8_t len)
{
    uint8_t a[2] = { (uint8_t)(addr & 0xFF), (uint8_t)(addr >> 8) };

    if (BQ_WriteBlock(BQ_CMD_SUBCMD_LOW, a, 2) != HAL_OK)
    {
        return HAL_ERROR;
    }

    HAL_Delay(5);   // let the device fill the transfer buffer
    return BQ_ReadBlock(BQ_CMD_XFER_BUFFER, data, len);
}

/*
 * Write 'len' bytes to RAM register space starting at 'addr'. Must be in CONFIG_UPDATE mode.
 * Step 1: write [addr_lo][addr_hi][data...] to 0x3E (auto-increments into 0x40...)
 * Step 2: write [checksum][length] to 0x60/0x61, where
 *           checksum = ~(sum of addr bytes + data bytes)
 *           length   = data bytes + 4  (2 address bytes + data + 2 for checksum/length)
 */
static HAL_StatusTypeDef BQ_RamWrite(uint16_t addr, const uint8_t *data, uint8_t len)
{
    uint8_t block[BQ_MAX_BLOCK];
    uint8_t sum = 0;

    if (len == 0 || len > (BQ_MAX_BLOCK - 2))
    {
        return HAL_ERROR;
    }

    block[0] = (uint8_t)(addr & 0xFF);
    block[1] = (uint8_t)(addr >> 8);
    memcpy(&block[2], data, len);

    if (BQ_WriteBlock(BQ_CMD_SUBCMD_LOW, block, (uint8_t)(len + 2)) != HAL_OK)
    {
        return HAL_ERROR;
    }

    for (uint8_t i = 0; i < (uint8_t)(len + 2); i++)
    {
        sum += block[i];
    }

    uint8_t tail[2] = { (uint8_t)(~sum), (uint8_t)(len + 4) };

    if (BQ_WriteBlock(0x60, tail, 2) != HAL_OK)
    {
        return HAL_ERROR;
    }

    HAL_Delay(10);  // give the device time to commit the write
    return HAL_OK;
}

/* Poll Battery Status until CFGUPDATE bit matches 'wantSet'. Returns 0 on success. */
static int BQ_WaitCfgUpdate(int wantSet)
{
    uint16_t st = 0;

    for (int i = 0; i < 25; i++)
    {
        if (BQ_ReadDirectU16(BQ_CMD_BATTERY_STATUS, &st) == HAL_OK &&
            (((st & BQ_BATSTATUS_CFGUPDATE) != 0) == (wantSet != 0)))
        {
            return 0;
        }
        HAL_Delay(2);
    }
    return -1;
}

/* Read and print Battery Status, stack voltage and internal temperature */
static void BQ_PrintStatus(void)
{
    char msg[96];
    uint16_t raw = 0;

    // Battery Status (0x12)
    if (BQ_ReadDirectU16(BQ_CMD_BATTERY_STATUS, &raw) == HAL_OK)
    {
        snprintf(msg, sizeof(msg), "Battery Status = 0x%04X\r\n", raw);
    }
    else
    {
        snprintf(msg, sizeof(msg), "Battery Status read FAILED (%s)\r\n",
                 bq_crc_bad ? "CRC" : "I2C");
    }
    UART_Print(msg);

    // Stack voltage (0x34): default userV = 10 mV, so raw 4800 = 48.00 V
    if (BQ_ReadDirectU16(BQ_CMD_STACK_VOLTAGE, &raw) == HAL_OK)
    {
        snprintf(msg, sizeof(msg), "Stack voltage = %u.%02u V (raw %u)\r\n",
                 (unsigned)(raw / 100), (unsigned)(raw % 100), (unsigned)raw);
    }
    else
    {
        snprintf(msg, sizeof(msg), "Stack voltage read FAILED (%s)\r\n",
                 bq_crc_bad ? "CRC" : "I2C");
    }
    UART_Print(msg);

    // Internal temperature (0x68): 0.1 K units, so Celsius*10 = raw - 2732
    if (BQ_ReadDirectU16(BQ_CMD_INT_TEMP, &raw) == HAL_OK)
    {
        int tenthsC = (int)raw - 2732;
        int absT = (tenthsC < 0) ? -tenthsC : tenthsC;

        snprintf(msg, sizeof(msg), "Internal temp = %s%d.%d C (raw %u x0.1 K)\r\n",
                 (tenthsC < 0) ? "-" : "", absT / 10, absT % 10, (unsigned)raw);
    }
    else
    {
        snprintf(msg, sizeof(msg), "Internal temp read FAILED (%s)\r\n",
                 bq_crc_bad ? "CRC" : "I2C");
    }
    UART_Print(msg);
}

/* Read and print FET status */
static void BQ_PrintFetStatus(const char *when)
{
    char msg[128];
    uint8_t st = 0;

    if (BQ_ReadBlock(BQ_CMD_FET_STATUS, &st, 1) != HAL_OK) return;

    snprintf(msg, sizeof(msg),
             "  FET Status [%s] = 0x%02X  "
             "CHG=%d DSG=%d PCHG=%d PDSG=%d DCHG=%d DDSG=%d ALRT=%d\r\n",
             when, st,
             (st >> 0) & 1, (st >> 1) & 1, (st >> 2) & 1, (st >> 3) & 1,
             (st >> 4) & 1, (st >> 5) & 1, (st >> 6) & 1);
    UART_Print(msg);
}

/* Print Safety Status to see why FETs might be blocked */
static void BQ_PrintBlockers(const char *when)
{
    char msg[128];
    uint16_t a = 0, b = 0, c = 0, bat = 0;

    BQ_ReadDirectU16(BQ_CMD_SAFETY_STATUS_A, &a);
    BQ_ReadDirectU16(BQ_CMD_SAFETY_STATUS_B, &b);
    BQ_ReadDirectU16(BQ_CMD_SAFETY_STATUS_C, &c);
    BQ_ReadDirectU16(BQ_CMD_BATTERY_STATUS,  &bat);

    snprintf(msg, sizeof(msg),
             "  [%s] SafetyA=0x%04X SafetyB=0x%04X SafetyC=0x%04X BattStat=0x%04X\r\n",
             when, a, b, c, bat);
    UART_Print(msg);

    if (a & 0x80) UART_Print("    ! SCD\r\n");
    if (a & 0x40) UART_Print("    ! OCD2\r\n");
    if (a & 0x20) UART_Print("    ! OCD1\r\n");
    if (a & 0x10) UART_Print("    ! OCC\r\n");
    if (a & 0x08) UART_Print("    ! COV\r\n");
    if (a & 0x04) UART_Print("    ! CUV\r\n");
    if (b & 0x80) UART_Print("    ! OTF\r\n");
    if (b & 0x40) UART_Print("    ! OTINT\r\n");
    if (b & 0x20) UART_Print("    ! OTD\r\n");
    if (b & 0x10) UART_Print("    ! OTC\r\n");
    if (c & 0x80) UART_Print("    ! OCD3\r\n");
    if (c & 0x40) UART_Print("    ! SCDL\r\n");
    if (c & 0x20) UART_Print("    ! OCDL\r\n");
    if (c & 0x10) UART_Print("    ! COVL\r\n");
    if (bat & 0x0800) UART_Print("    ! Battery Status SS=1 (safety fault active)\r\n");
    if (bat & 0x1000) UART_Print("    ! Battery Status PF=1 (permanent fail)\r\n");
}

/* Send subcommand that returns no data, print result */
static int BQ_SendCmd(const char *name, uint16_t subcmd)
{
    char msg[96];

    if (BQ_SendSubcommand(subcmd) != HAL_OK) {
        snprintf(msg, sizeof(msg), "  %s send FAILED\r\n", name);
        UART_Print(msg);
        return -1;
    }
    snprintf(msg, sizeof(msg), "  %s sent OK (0x%04X)\r\n", name, subcmd);
    UART_Print(msg);
    return 0;
}

/* Unseal the BQ76952 so we can enter CONFIG_UPDATE */
static int BQ_Unseal(void)
{
    char msg[96];
    UART_Print("  Unsealing device...\r\n");

    // Default TI Unseal Keys: Step 1 (0x0414) and Step 2 (0x3672)
    // Sent little-endian to 0x3E/0x3F
    uint8_t key1[2] = {0x14, 0x04};
    uint8_t key2[2] = {0x72, 0x36};

    if (BQ_WriteBlock(BQ_CMD_SUBCMD_LOW, key1, 2) != HAL_OK) return -1;
    HAL_Delay(5);
    if (BQ_WriteBlock(BQ_CMD_SUBCMD_LOW, key2, 2) != HAL_OK) return -1;
    HAL_Delay(5);

    uint16_t bat_stat = 0;
    BQ_ReadDirectU16(BQ_CMD_BATTERY_STATUS, &bat_stat);
    uint8_t sec = (bat_stat >> 8) & 0x03;

    snprintf(msg, sizeof(msg), "  Security state = %d (0=Full, 1=Unsealed, 2=Sealed)\r\n", sec);
    UART_Print(msg);

    if (sec == 0 || sec == 1) return 0;
    return -1;
}

/* Full FET bring-up test */
static void BQ_FetBringupTest(void)
{
    char msg[128];
    uint8_t val;
    uint16_t val16;

    UART_Print("\r\n============================================\r\n");
    UART_Print("   FET / CHARGE PUMP BRING-UP TEST\r\n");
    UART_Print("============================================\r\n");

    BQ_PrintBlockers("pre");
    BQ_PrintFetStatus("pre");

    if (BQ_Unseal() != 0) {
        UART_Print("  Unseal FAILED. Cannot enter CONFIG_UPDATE. Aborting.\r\n");
        return;
    }

    if (BQ_SendCmd("SET_CFGUPDATE", BQ_SUBCMD_SET_CFGUPDATE) != 0) return;
    if (BQ_WaitCfgUpdate(1) != 0) {
        UART_Print("  CFGUPDATE bit never set, aborting\r\n");
        BQ_SendSubcommand(BQ_SUBCMD_EXIT_CFGUPDATE);
        return;
    }
    UART_Print("  Entered CONFIG_UPDATE\r\n");

    /* 1. Vcell Mode: 13S (bits 0..12 set) */
    val16 = 0x1FFF;
    if (BQ_RamWrite(BQ_RAM_VCELL_MODE, (uint8_t *)&val16, 2) == HAL_OK) {
        UART_Print("  Vcell Mode = 0x1FFF (13 cells) OK\r\n");
    } else {
        UART_Print("  Vcell Mode write FAILED\r\n");
    }

    /* 2. DA Config: 1 mA current units, 10 mV voltage units */
    val = 0x06;
    if (BQ_RamWrite(BQ_RAM_DA_CONFIG, &val, 1) == HAL_OK) {
        UART_Print("  DA Config = 0x06 OK\r\n");
    } else {
        UART_Print("  DA Config write FAILED\r\n");
    }

    /* 3. Enabled Protections A: only SCD + COV (disable CUV for bring-up) */
    val = 0x88;
    if (BQ_RamWrite(BQ_RAM_ENABLED_PROT_A, &val, 1) == HAL_OK) {
        UART_Print("  Enabled Protections A = 0x88 (SCD+COV only) OK\r\n");
    } else {
        UART_Print("  Enabled Protections A write FAILED\r\n");
    }

    /* 4. Mfg Status Init: set FET_EN (bit 4) */
    uint8_t mfg[2] = {0};
    if (BQ_RamRead(BQ_RAM_MFG_STATUS_INIT, mfg, 2) == HAL_OK) {
        snprintf(msg, sizeof(msg), "  Mfg Status Init before = 0x%02X%02X\r\n", mfg[1], mfg[0]);
        UART_Print(msg);
        mfg[0] |= 0x10;
        if (BQ_RamWrite(BQ_RAM_MFG_STATUS_INIT, mfg, 2) == HAL_OK) {
            snprintf(msg, sizeof(msg), "  Mfg Status Init after  = 0x%02X%02X\r\n", mfg[1], mfg[0]);
            UART_Print(msg);
        } else {
            UART_Print("  Mfg Status Init write FAILED\r\n");
        }
    } else {
        UART_Print("  Mfg Status Init read FAILED\r\n");
    }

    /* 5. Chg Pump Control: CPEN=1, LVEN=0 (11V) */
    val = 0x01;
    if (BQ_RamWrite(BQ_RAM_CHG_PUMP_CONTROL, &val, 1) == HAL_OK) {
        UART_Print("  Chg Pump Control = 0x01 (enabled, 11V) OK\r\n");
    } else {
        UART_Print("  Chg Pump Control write FAILED\r\n");
    }

    /* 6. FET Options: SFET=1 (series), FET_CTRL_EN=1, HOST_FET_EN=1 */
    val = 0x0D;
    if (BQ_RamWrite(BQ_RAM_FET_OPTIONS, &val, 1) == HAL_OK) {
        UART_Print("  FET Options = 0x0D (series, host ctrl) OK\r\n");
    } else {
        UART_Print("  FET Options write FAILED\r\n");
    }

    if (BQ_SendCmd("EXIT_CFGUPDATE", BQ_SUBCMD_EXIT_CFGUPDATE) != 0) return;
    if (BQ_WaitCfgUpdate(0) != 0) {
        UART_Print("  CFGUPDATE bit never cleared\r\n");
        return;
    }
    UART_Print("  Exited CONFIG_UPDATE\r\n");

    UART_Print("  Waiting 200 ms for charge pump startup...\r\n");
    HAL_Delay(200);

    BQ_PrintBlockers("post-cfg");
    BQ_PrintFetStatus("post-cfg");

    BQ_SendCmd("ALL_FETS_OFF", BQ_SUBCMD_ALL_FETS_OFF);
    HAL_Delay(50);
    BQ_PrintFetStatus("after-off");

    UART_Print("\r\n>>> Check with meter: V(BAT+ to PACK+) should be ~0.6V (body diode)\r\n");
    UART_Print(">>> Press ANY KEY to send ALL_FETS_ON...\r\n");

    __HAL_UART_CLEAR_OREFLAG(&huart1);
    uint8_t rx;
    HAL_UART_Receive(&huart1, &rx, 1, HAL_MAX_DELAY);

    snprintf(msg, sizeof(msg), "  Received key '%c' (0x%02X)\r\n", rx, rx);
    UART_Print(msg);

    BQ_SendCmd("ALL_FETS_ON", BQ_SUBCMD_ALL_FETS_ON);
    HAL_Delay(50);

    BQ_PrintBlockers("after-on");
    BQ_PrintFetStatus("after-on");

    UART_Print("\r\n>>> FETs should now be ON if no safety fault present.\r\n");
    UART_Print(">>> Expected: V(BAT+ to PACK+) ~0V, V(CHG-BAT+) ~11V, V(DSG-BAT+) ~11V\r\n");
    UART_Print(">>> Press ANY KEY to send ALL_FETS_OFF (cleanup)...\r\n");

    __HAL_UART_CLEAR_OREFLAG(&huart1);
    HAL_UART_Receive(&huart1, &rx, 1, HAL_MAX_DELAY);

    BQ_SendCmd("ALL_FETS_OFF", BQ_SUBCMD_ALL_FETS_OFF);
    HAL_Delay(50);
    BQ_PrintFetStatus("final-off");

    UART_Print("=== FET bring-up test done ===\r\n\r\n");
}

static void I2C_Scan(void)
{
    char msg[64];

    UART_Print("\r\nScanning I2C bus...\r\n");

    for (uint8_t addr7 = 1; addr7 < 127; addr7++)
    {
        uint16_t devAddr = addr7 << 1;

        if (HAL_I2C_IsDeviceReady(&hi2c1, devAddr, 2, 20) == HAL_OK)
        {
            snprintf(msg, sizeof(msg),
                     "Found I2C device: 7-bit=0x%02X, HAL addr=0x%02X\r\n",
                     addr7,
                     devAddr);
            UART_Print(msg);
        }
    }

    UART_Print("I2C scan done.\r\n");
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  UART_Print("\r\nBQ76952 bring-up test started\r\n");

  // The BQ76952 takes a few hundred ms to finish booting after power-up
  HAL_Delay(500);

  if (HAL_I2C_IsDeviceReady(&hi2c1, BQ76952_ADDR, 3, 100) == HAL_OK)
  {
      UART_Print("BQ76952 detected on I2C\r\n");
  }
  else
  {
      UART_Print("BQ76952 NOT detected on I2C\r\n");
  }

  I2C_Scan();

  // Run FET test (this will block waiting for UART keys)
  BQ_FetBringupTest();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint16_t devnum = 0;
    char msg[96];

    int rc = BQ_ReadSubcommandU16(BQ_SUBCMD_DEVICE_NUMBER, &devnum);

    if (rc == 0)
    {
        snprintf(msg, sizeof(msg), "Device number = 0x%04X %s\r\n",
                 devnum,
                 (devnum == BQ76952_EXPECTED_DEVNUM) ? "(OK, BQ76952)" : "(UNEXPECTED)");
        UART_Print(msg);
    }
    else
    {
        int addrAck = (HAL_I2C_IsDeviceReady(&hi2c1, BQ76952_ADDR, 2, 20) == HAL_OK);
        snprintf(msg, sizeof(msg),
                 "Device number FAILED at %s (I2C err=0x%08lX, address %s)\r\n",
                 (rc == 1) ? "write" : (rc == 2) ? "read" : "CRC check",
                 bq_last_err,
                 addrAck ? "ACKed" : "NACKed");
        UART_Print(msg);
    }

    BQ_PrintStatus();
    BQ_PrintFetStatus("loop");
    UART_Print("\r\n");

    HAL_Delay(1000);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

