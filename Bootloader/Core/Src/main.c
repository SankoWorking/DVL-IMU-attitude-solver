#include "main.h"

#include "boot_cdc_if.h"
#include "boot_flash.h"
#include "boot_msc_storage.h"
#include "boot_shared.h"
#include "usb_device.h"

static void MPU_Config(void);
static uint32_t BootRequest_Consume(void);
static void BootRequest_Clear(void);
static void Boot_JumpToDfuBootloader(void);

#define STM32H7_SYSTEM_MEMORY  0x1FF09800UL
#define NVIC_REGISTER_COUNT    8U

int main(void)
{
    uint32_t boot_request;

    HAL_Init();
    MPU_Config();
    SystemClock_Config();
    PeriphCommonClock_Config();

    boot_request = BootRequest_Consume();
    if (boot_request == DFU_REQUEST_MAGIC)
    {
        Boot_JumpToDfuBootloader();
    }

    if ((boot_request != BOOT_REQUEST_MAGIC) && (BootFlash_IsAppValid() != 0U))
    {
        BootFlash_JumpToApp();
    }

    MX_USB_DEVICE_Init();
    Boot_CDC_Log("H7 Luxiaoban bootloader ready\r\n");
    Boot_CDC_Log("Drop H743.hex to the USB disk to update app at 0x08040000\r\n");

    for (;;)
    {
        if (BootMsc_IsInstallRequested() != 0U)
        {
            BootFlashStatus status;

            Boot_CDC_Log("Installing HEX from USB disk...\r\n");
            status = BootFlash_InstallHexImage(BootMsc_GetDiskData(), BootMsc_GetDiskSize());
            BootMsc_ClearInstallRequest();

            if (status == BOOT_FLASH_OK)
            {
                Boot_CDC_Log("Install OK, resetting to app\r\n");
                BootRequest_Clear();
                HAL_Delay(100U);
                MX_USB_DEVICE_DeInit();
                HAL_Delay(300U);
                NVIC_SystemReset();
            }
            else
            {
                Boot_CDC_Log("Install failed\r\n");
            }
        }

        HAL_Delay(20U);
    }
}

static uint32_t BootRequest_Consume(void)
{
    uint32_t request;

    __HAL_RCC_RTC_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    request = RTC->BKP0R;
    RTC->BKP0R = 0U;

    return request;
}

static void BootRequest_Clear(void)
{
    __HAL_RCC_RTC_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    RTC->BKP0R = 0U;
}

static void Boot_JumpToDfuBootloader(void)
{
    typedef void (*BootloaderEntry_t)(void);
    uint32_t boot_stack;
    uint32_t boot_entry;
    uint32_t i;

    boot_stack = *(__IO uint32_t *)STM32H7_SYSTEM_MEMORY;
    boot_entry = *(__IO uint32_t *)(STM32H7_SYSTEM_MEMORY + 4U);

    __disable_irq();
    HAL_DeInit();
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;
    SCB->ICSR = SCB_ICSR_PENDSVCLR_Msk | SCB_ICSR_PENDSTCLR_Msk;
    HAL_MPU_Disable();

    for (i = 0U; i < NVIC_REGISTER_COUNT; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFUL;
        NVIC->ICPR[i] = 0xFFFFFFFFUL;
    }

    SCB->VTOR = STM32H7_SYSTEM_MEMORY;
    __set_CONTROL(0U);
    __set_MSP(boot_stack);
    __DSB();
    __ISB();
    __enable_irq();
    ((BootloaderEntry_t)boot_entry)();

    while (1)
    {
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY))
    {
    }

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_HSI48;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 5;
    RCC_OscInitStruct.PLL.PLLN = 160;
    RCC_OscInitStruct.PLL.PLLP = 2;
    RCC_OscInitStruct.PLL.PLLQ = 2;
    RCC_OscInitStruct.PLL.PLLR = 2;
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
    RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    RCC_OscInitStruct.PLL.PLLFRACN = 0;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                                  RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
}

void PeriphCommonClock_Config(void)
{
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_CKPER | RCC_PERIPHCLK_USB;
    PeriphClkInitStruct.CkperClockSelection = RCC_CLKPSOURCE_HSE;
    PeriphClkInitStruct.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    HAL_PWREx_EnableUSBVoltageDetector();
}

static void MPU_Config(void)
{
    MPU_Region_InitTypeDef MPU_InitStruct = {0};

    HAL_MPU_Disable();

    MPU_InitStruct.Enable = MPU_REGION_ENABLE;
    MPU_InitStruct.Number = MPU_REGION_NUMBER0;
    MPU_InitStruct.BaseAddress = 0x0;
    MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
    MPU_InitStruct.SubRegionDisable = 0x87;
    MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
    MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
    MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}
