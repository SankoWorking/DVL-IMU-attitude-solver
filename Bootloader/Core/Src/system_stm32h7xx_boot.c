#include "stm32h7xx.h"

uint32_t SystemCoreClock = 64000000;
uint32_t SystemD2Clock = 64000000;
const uint8_t D1CorePrescTable[16] = {0, 0, 0, 0, 1, 2, 3, 4, 1, 2, 3, 4, 6, 7, 8, 9};

void SystemInit(void)
{
#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
    SCB->CPACR |= ((3UL << (10U * 2U)) | (3UL << (11U * 2U)));
#endif
    SCB->VTOR = FLASH_BANK1_BASE;
}

void SystemCoreClockUpdate(void)
{
    SystemCoreClock = 400000000U;
    SystemD2Clock = 200000000U;
}

void ExitRun0Mode(void)
{
#if defined(USE_PWR_LDO_SUPPLY)
#if defined(SMPS)
    PWR->CR3 = (PWR->CR3 & ~PWR_CR3_SMPSEN) | PWR_CR3_LDOEN;
#else
    PWR->CR3 |= PWR_CR3_LDOEN;
#endif
    while ((PWR->CSR1 & PWR_CSR1_ACTVOSRDY) == 0U)
    {
    }
#endif
}
