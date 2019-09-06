/******************************************************************************
 * @file     main.c
 * @brief    Write files in SD card to SPI/NAND/eMMC
 *
 * @copyright (C) 2018 Nuvoton Technology Corp. All rights reserved.
*****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nuc980.h"
#include "sys.h"
#include "sdh.h"
#include "ff.h"
#include "diskio.h"
#include "fmi.h"
#include "writer.h"
#include "sdglue2.h"
#include "spinandflash.h"
#include "filesystem.h"

extern int spiInit();
int spiEraseAll(void);
int spiCheckBusy(void);
int spiEraseSector(UINT32 addr, UINT32 secCount);
int spiWrite(UINT32 addr, UINT32 len, UINT8 *buf);
extern int ProcessINI(char *fileName);

#define WDT_RSTCNT    outpw(REG_WDT_RSTCNT, 0x5aa5)
#define BUFF_SIZE      (512*1024) // CWWeng 2018.11.13 (64*1024)
#define EMMC_BLOCK_SIZE (64*1024)

DWORD acc_size;                         /* Work register for fs command */
WORD acc_files, acc_dirs;
FILINFO Finfo;

char Line[256];                         /* Console input buffer */
#if _USE_LFN
char Lfname[512];
#endif

__align(32) BYTE Buff_Pool[BUFF_SIZE] ;       /* Working buffer */
BYTE  *Buff;

__align(32) BYTE Block_Buff_Pool[512*1024] ;       /* For NAND/SPI NAND block */
BYTE  *Block_Buff;

//CWWeng 2018.11.15 add for NAND
extern UINT32 g_uIsUserConfig; //nand  //CWWeng 2018.11.15 copy from NuWriter FW parse.c
FW_NAND_IMAGE_T nandImage;   //CWWeng 2018.11.15 copy from NuWriter FW parse.c
FW_NAND_IMAGE_T *pNandImage; //CWWeng 2018.11.15 copy from NuWriter FW parse.c

//CWWeng 2018.11.19 add for SPINAND
extern SPINAND_INFO_T SNInfo, *pSN;

//CWWeng 2018.11.21 add for SPI 4 byte address
#define SPI_BLOCK_SIZE (64*1024)
#define SPI_FLASH_SIZE (16*1024*1024)  //4Byte Address Mode
extern volatile unsigned char Enable4ByteFlag;

//CWWeng 2018.11.9 add
char *pENV;
static const unsigned long crc32_table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419,
    0x706af48f, 0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4,
    0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07,
    0x90bf1d91, 0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
    0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856,
    0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4,
    0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3,
    0x45df5c75, 0xdcd60dcf, 0xabd13d59, 0x26d930ac, 0x51de003a,
    0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599,
    0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190,
    0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f,
    0x9fbfe4a5, 0xe8b8d433, 0x7807c9a2, 0x0f00f934, 0x9609a88e,
    0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed,
    0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3,
    0xfbd44c65, 0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
    0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a,
    0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5,
    0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa, 0xbe0b1010,
    0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17,
    0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6,
    0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615,
    0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
    0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1, 0xf00f9344,
    0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a,
    0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
    0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1,
    0xa6bc5767, 0x3fb506dd, 0x48b2364b, 0xd80d2bda, 0xaf0a1b4c,
    0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef,
    0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe,
    0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31,
    0x2cd99e8b, 0x5bdeae1d, 0x9b64c2b0, 0xec63f226, 0x756aa39c,
    0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b,
    0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1,
    0x18b74777, 0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
    0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45, 0xa00ae278,
    0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7,
    0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc, 0x40df0b66,
    0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605,
    0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8,
    0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b,
    0x2d02ef8d
};

INFO_T info;
extern INI_INFO_T Ini_Writer;

/* eMMC */
extern FW_MMC_IMAGE_T mmcImage;
extern FW_MMC_IMAGE_T *pmmcImage;
extern void GetMMCImage(void);

//CWWeng 2018.11.9 : It's a null function, in NuWriter it's for USB send status
//for easy to maintain the firmware between SD writer with NuWriter, add a null one here
void SendAck(UINT32 status);
void SendAck(UINT32 status)
{
}
unsigned char *pImageList;
unsigned char imageList[512];

/* Set ETimer0 : unit is micro-second */
void SetTimer(unsigned int count)
{
    /* Set timer0 */
    outpw(REG_CLK_PCLKEN0, inpw(REG_CLK_PCLKEN0) | 0x100);  /* enable timer engine clock */
    outpw(REG_ETMR0_ISR, 0x1);
    outpw(REG_ETMR0_CMPR, count);       /* set timer init counter value */
    outpw(REG_ETMR0_PRECNT, 0xB);
    outpw(REG_ETMR0_CTL, 0x01);         /* one-shot mode, prescale = 12 */
}

void DelayMicrosecond(unsigned int count)
{
    SetTimer(count);
    /* wait timeout */
    while(1) {
        if (inpw(REG_ETMR0_ISR) & 0x1) {
            outpw(REG_ETMR0_ISR, 0x1);
            break;
        }
    }
}

BYTE SDH_Drv; // select SD0

void  dump_buff_hex(uint8_t *pucBuff, int nBytes)
{
    int     nIdx, i;

    nIdx = 0;
    while (nBytes > 0) {
        printf("0x%04X  ", nIdx);
        for (i = 0; i < 16; i++)
            printf("%02x ", pucBuff[nIdx + i]);
        printf("  ");
        for (i = 0; i < 16; i++) {
            if ((pucBuff[nIdx + i] >= 0x20) && (pucBuff[nIdx + i] < 127))
                printf("%c", pucBuff[nIdx + i]);
            else
                printf(".");
            nBytes--;
        }
        nIdx += 16;
        printf("\n");
    }
    printf("\n");
}


/*--------------------------------------------------------------------------*/
/* Monitor                                                                  */

/*----------------------------------------------*/
/* Get a value of the string                    */
/*----------------------------------------------*/
/*  "123 -5   0x3ff 0b1111 0377  w "
        ^                           1st call returns 123 and next ptr
           ^                        2nd call returns -5 and next ptr
                   ^                3rd call returns 1023 and next ptr
                          ^         4th call returns 15 and next ptr
                               ^    5th call returns 255 and next ptr
                                  ^ 6th call fails and returns 0
*/

int xatoi (         /* 0:Failed, 1:Successful */
    TCHAR **str,    /* Pointer to pointer to the string */
    long *res       /* Pointer to a variable to store the value */
)
{
    unsigned long val;
    unsigned char r, s = 0;
    TCHAR c;


    *res = 0;
    while ((c = **str) == ' ') (*str)++;    /* Skip leading spaces */

    if (c == '-') {     /* negative? */
        s = 1;
        c = *(++(*str));
    }

    if (c == '0') {
        c = *(++(*str));
        switch (c) {
        case 'x':       /* hexadecimal */
            r = 16;
            c = *(++(*str));
            break;
        case 'b':       /* binary */
            r = 2;
            c = *(++(*str));
            break;
        default:
            if (c <= ' ') return 1; /* single zero */
            if (c < '0' || c > '9') return 0;   /* invalid char */
            r = 8;      /* octal */
        }
    } else {
        if (c < '0' || c > '9') return 0;   /* EOL or invalid char */
        r = 10;         /* decimal */
    }

    val = 0;
    while (c > ' ') {
        if (c >= 'a') c -= 0x20;
        c -= '0';
        if (c >= 17) {
            c -= 7;
            if (c <= 9) return 0;   /* invalid char */
        }
        if (c >= r) return 0;       /* invalid char for current radix */
        val = val * r + c;
        c = *(++(*str));
    }
    if (s) val = 0 - val;           /* apply sign if needed */

    *res = val;
    return 1;
}


/*----------------------------------------------*/
/* Dump a block of byte array                   */

void put_dump (
    const unsigned char* buff,  /* Pointer to the byte array to be dumped */
    unsigned long addr,         /* Heading address value */
    int cnt                     /* Number of bytes to be dumped */
)
{
    int i;


    printf("%08x ", (unsigned int)addr);

    for (i = 0; i < cnt; i++)
        printf(" %02x", buff[i]);

    printf(" ");
    for (i = 0; i < cnt; i++)
        putchar((TCHAR)((buff[i] >= ' ' && buff[i] <= '~') ? buff[i] : '.'));

    printf("\n");
}


/*--------------------------------------------------------------------------*/
/* Monitor                                                                  */
/*--------------------------------------------------------------------------*/

static
FRESULT scan_files (
    char* path      /* Pointer to the path name working buffer */
)
{
    DIR dirs;
    FRESULT res;
    BYTE i;
    char *fn;


    if ((res = f_opendir(&dirs, path)) == FR_OK) {
        i = strlen(path);
        while (((res = f_readdir(&dirs, &Finfo)) == FR_OK) && Finfo.fname[0]) {
            if (_FS_RPATH && Finfo.fname[0] == '.') continue;
#if _USE_LFN
            //fn = *Finfo.lfname ? Finfo.lfname : Finfo.fname; //CWWeng 2018.11.21
            fn = Finfo.fname; //CWWeng 2018.11.21 : long file name
#else
            fn = Finfo.fname;
#endif
            if (Finfo.fattrib & AM_DIR) {
                acc_dirs++;
                *(path+i) = '/';
                strcpy(path+i+1, fn);
                res = scan_files(path);
                *(path+i) = '\0';
                if (res != FR_OK) break;
            } else {
                /*              printf("%s/%s\n", path, fn); */
                acc_files++;
                acc_size += Finfo.fsize;
            }
        }
    }

    return res;
}



void put_rc (FRESULT rc)
{
    const TCHAR *p =
        _T("OK\0DISK_ERR\0INT_ERR\0NOT_READY\0NO_FILE\0NO_PATH\0INVALID_NAME\0")
        _T("DENIED\0EXIST\0INVALID_OBJECT\0WRITE_PROTECTED\0INVALID_DRIVE\0")
        _T("NOT_ENABLED\0NO_FILE_SYSTEM\0MKFS_ABORTED\0TIMEOUT\0LOCKED\0")
        _T("NOT_ENOUGH_CORE\0TOO_MANY_OPEN_FILES\0");
    //FRESULT i;
    uint32_t i;
    for (i = 0; (i != (UINT)rc) && *p; i++) {
        while(*p++) ;
    }
    printf(_T("rc=%d FR_%s\n"), (UINT)rc, p);
}

/*----------------------------------------------*/
/* Get a line from the input                    */
/*----------------------------------------------*/

void get_line (char *buff, int len)
{
    TCHAR c;
    int idx = 0;



    for (;;) {
        c = getchar();
        putchar(c);
        if (c == '\r') break;
        if ((c == '\b') && idx) idx--;
        if ((c >= ' ') && (idx < len - 1)) buff[idx++] = c;
    }
    buff[idx] = 0;

    putchar('\n');

}

void SDH0_IRQHandler(void)
{
    unsigned int volatile isr;
    unsigned int volatile ier;

    // FMI data abort interrupt
    if (SDH0->GINTSTS & SDH_GINTSTS_DTAIF_Msk) {
        /* ResetAllEngine() */
        SDH0->GCTL |= SDH_GCTL_GCTLRST_Msk;
    }

    //----- SD interrupt status
    isr = SDH0->INTSTS;
    if (isr & SDH_INTSTS_BLKDIF_Msk) {
        // block down
        g_u8SDDataReadyFlag = TRUE;
        SDH0->INTSTS = SDH_INTSTS_BLKDIF_Msk;
    }

    if (isr & SDH_INTSTS_CDIF_Msk) {   // card detect
        //----- SD interrupt status
        // it is work to delay 50 times for SDH_CLK = 200KHz
        {
            int volatile i;         // delay 30 fail, 50 OK
            for (i=0; i<0x500; i++);  // delay to make sure got updated value from REG_SDISR.
            isr = SDH0->INTSTS;
        }

        if (isr & SDH_INTSTS_CDSTS_Msk) {
            printf("\n***** card remove !\n");
            SD0.IsCardInsert = FALSE;   // SDISR_CD_Card = 1 means card remove for GPIO mode
            memset(&SD0, 0, sizeof(SDH_INFO_T));
        }
        else {
            printf("***** card insert !\n");
            SDH_Open(SDH0, CardDetect_From_GPIO);
            SDH_Probe(SDH0);
        }

        SDH0->INTSTS = SDH_INTSTS_CDIF_Msk;
    }

    // CRC error interrupt
    if (isr & SDH_INTSTS_CRCIF_Msk)
    {
        if (!(isr & SDH_INTSTS_CRC16_Msk)) {
            //printf("***** ISR sdioIntHandler(): CRC_16 error !\n");
            // handle CRC error
        }
        else if (!(isr & SDH_INTSTS_CRC7_Msk)) {
            if (!g_u8R3Flag) {
                //printf("***** ISR sdioIntHandler(): CRC_7 error !\n");
                // handle CRC error
            }
        }
        SDH0->INTSTS = SDH_INTSTS_CRCIF_Msk;      // clear interrupt flag
    }

    if (isr & SDH_INTSTS_DITOIF_Msk) {
        printf("***** ISR: data in timeout !\n");
        SDH0->INTSTS |= SDH_INTSTS_DITOIF_Msk;
    }

    // Response in timeout interrupt
    if (isr & SDH_INTSTS_RTOIF_Msk) {
        printf("***** ISR: response in timeout !\n");
        SDH0->INTSTS |= SDH_INTSTS_RTOIF_Msk;
    }
}

void SDH_IRQHandler(void)
{
    unsigned int volatile isr;
    unsigned int volatile ier;

    // FMI data abort interrupt
    if (SDH1->GINTSTS & SDH_GINTSTS_DTAIF_Msk) {
        /* ResetAllEngine() */
        SDH1->GCTL |= SDH_GCTL_GCTLRST_Msk;
    }

    //----- SD interrupt status
    isr = SDH1->INTSTS;
    if (isr & SDH_INTSTS_BLKDIF_Msk) {
        // block down
        g_u8SDDataReadyFlag = TRUE;
        SDH1->INTSTS = SDH_INTSTS_BLKDIF_Msk;
    }

    if (isr & SDH_INTSTS_CDIF_Msk) { // card detect
        //----- SD interrupt status
        // it is work to delay 50 times for SDH_CLK = 200KHz
        {
            int volatile i;         // delay 30 fail, 50 OK
            for (i=0; i<0x500; i++);  // delay to make sure got updated value from REG_SDISR.
            isr = SDH1->INTSTS;
        }

        if (isr & SDH_INTSTS_CDSTS_Msk) {
            printf("\n***** card remove !\n");
            SD1.IsCardInsert = FALSE;   // SDISR_CD_Card = 1 means card remove for GPIO mode
            memset(&SD1, 0, sizeof(SDH_INFO_T));
        } else {
            printf("***** card insert !\n");
            SDH_Open(SDH1, CardDetect_From_GPIO);
            SDH_Probe(SDH1);
        }

        SDH1->INTSTS = SDH_INTSTS_CDIF_Msk;
    }

    // CRC error interrupt
    if (isr & SDH_INTSTS_CRCIF_Msk) {
        if (!(isr & SDH_INTSTS_CRC16_Msk)) {
            //printf("***** ISR sdioIntHandler(): CRC_16 error !\n");
            // handle CRC error
        } else if (!(isr & SDH_INTSTS_CRC7_Msk)) {
            if (!g_u8R3Flag) {
                //printf("***** ISR sdioIntHandler(): CRC_7 error !\n");
                // handle CRC error
            }
        }
        SDH1->INTSTS = SDH_INTSTS_CRCIF_Msk;      // clear interrupt flag
    }

    if (isr & SDH_INTSTS_DITOIF_Msk) {
        printf("***** ISR: data in timeout !\n");
        SDH1->INTSTS |= SDH_INTSTS_DITOIF_Msk;
    }

    // Response in timeout interrupt
    if (isr & SDH_INTSTS_RTOIF_Msk) {
        printf("***** ISR: response in timeout !\n");
        SDH1->INTSTS |= SDH_INTSTS_RTOIF_Msk;
    }
}

uint32_t ETimer1_cnt = 0;

void ETMR1_IRQHandler(void)
{
    ETimer1_cnt++;
    //printf("ETimer1_cnt = %d\n",ETimer1_cnt);
    // clear timer interrupt flag
    ETIMER_ClearIntFlag(1);
    if (ETimer1_cnt >= 120) {
        printf("Time-out\n");
        ETIMER_Stop(1);
        while(1) {
            WDT_RSTCNT;
        }
    }
}

void ETimer1_Init(void)
{
    // Enable ETIMER1 engine clock
    outpw(REG_CLK_PCLKEN0, inpw(REG_CLK_PCLKEN0) | (1 << 9));

    // Set timer frequency to 1HZ
    ETIMER_Open(1, ETIMER_PERIODIC_MODE, 1);

    // Enable timer interrupt
    ETIMER_EnableInt(1);
    sysInstallISR(IRQ_LEVEL_1, IRQ_TIMER1, (PVOID)ETMR1_IRQHandler);
    sysSetLocalInterrupt(ENABLE_IRQ);
    sysEnableInterrupt(IRQ_TIMER1);
}


void SYS_Init(void)
{
    /* enable SDH */
    outpw(REG_CLK_HCLKEN, inpw(REG_CLK_HCLKEN) | 0x40000000);

    outpw(REG_CLK_HCLKEN,  inpw(REG_CLK_HCLKEN)  | (1<<20)); //Enable FMI clock
    outpw(REG_CLK_HCLKEN,  inpw(REG_CLK_HCLKEN)  | (1<<22)); //Enable SD0 clock
    outpw(REG_CLK_HCLKEN,  inpw(REG_CLK_HCLKEN)  | (1<<21)); //Enable NAND clock
    outpw(REG_CLK_HCLKEN,  inpw(REG_CLK_HCLKEN)  | (1<<30)); //Enable SD1 clock

    /* enable QSPI0 */
    outpw(REG_CLK_PCLKEN1, inpw(REG_CLK_PCLKEN1) | 0x10);

    /* enable WDT clock */
    outpw(REG_CLK_PCLKEN0, inpw(REG_CLK_PCLKEN0) | 0x1);

    /* select multi-function-pin */
    if (((inpw(REG_SYS_PWRON) & 0x00000300) == 0x300)) {
        /* SD Port 0 -> PC5~10,PC12 */
        outpw(REG_SYS_GPC_MFPL, (inpw(REG_SYS_GPC_MFPL)& ~0xFFF00000) | 0x66600000);
        outpw(REG_SYS_GPC_MFPH, (inpw(REG_SYS_GPC_MFPH)& ~0x000F0FFF) | 0x00060666);
    } else {
        /* SD Port 1 -> PF0~6 */
        outpw(REG_SYS_GPF_MFPL, (inpw(REG_SYS_GPF_MFPL)& ~0x0FFFFFFF) | 0x02222222);
    }
    //SD_Drv = 0;
}

/*---------------------------------------------------------*/
/* User Provided RTC Function for FatFs module             */
/*---------------------------------------------------------*/
/* This is a real time clock service to be called from     */
/* FatFs module. Any valid time must be returned even if   */
/* the system does not support an RTC.                     */
/* This function is not required in read-only cfg.         */

unsigned long get_fattime (void)
{
    unsigned long tmr;

    tmr=0x00000;

    return tmr;
}

void UART_Init()
{
    /* enable UART0 clock */
    outpw(REG_CLK_PCLKEN0, inpw(REG_CLK_PCLKEN0) | 0x10000);

    /* GPF11, GPF12 */
    outpw(REG_SYS_GPF_MFPH, (inpw(REG_SYS_GPF_MFPH) & 0xfff00fff) | 0x11000);   // UART0 multi-function

    /* UART0 line configuration for (115200,n,8,1) */
    outpw(REG_UART0_LCR, inpw(REG_UART0_LCR) | 0x07);
    outpw(REG_UART0_BAUD, 0x30000066); /* 12MHz reference clock input, 115200 */
}

static FIL file1, file2;        /* File objects */

DIR dir;                /* Directory object */
UINT s1, s2, s3, s4, cnt;
FRESULT result;
char *ptr;
FATFS   *fs;              /* Pointer to file system object */
TCHAR   sd_path[] = { '0', ':', 0 };    /* SD drive started from 0 */
FILINFO Finfo;
long    p1, p2, p3;
static FIL file1, file2;        /* File objects */

BYTE DDR_Line[80][23];
UINT32 DDRInitAddr[80];
UINT32 DDRInitData[80];
UINT32 DDRInitCount = 0;


static BYTE my_atoi(char src)
{
    BYTE c = 0;
    if ((src >= '0') && (src <= '9'))
        c = src - '0';
    else if ((src >= 'a') && (src <= 'f'))
        c = 0xa + src - 'a';
    else if ((src >= 'A') && (src <= 'F'))
        c = 0xA + src - 'A';
    return (c);
}

static void _parsing_DDRInit(int FileLen)
{
    int src_c = 0;
    int dest_c = 0;
    int LineNo = 0;
    int IsAddr = 1;

    DDRInitCount = 0;

    while (src_c < FileLen) {
        //sysprintf("[%d][%x][%x]\n",src_c,Buff[src_c], Buff[src_c+1]);
        if ((Buff[src_c] == 0x30) && (Buff[src_c+1] == 0x78)) {//skip 0x
            src_c += 2;
            continue;
        }
        if ((Buff[src_c] == 0xD) && (Buff[src_c+1] == 0xA)) {//Line end
            if ((Buff[src_c+2] == 0x30) && (Buff[src_c+3] == 0x78)) {
                src_c += 2;
                LineNo++;
                dest_c = 0;
                IsAddr ^= 1;
                DDRInitCount++;
                continue;
            }
        }

        if (Buff[src_c] == 0x3D) { //"="
            src_c++;
            IsAddr ^= 1;
            continue;
        }

        if (IsAddr)
            DDRInitAddr[LineNo] = DDRInitAddr[LineNo] * 16 + my_atoi(Buff[src_c]);
        else
            DDRInitData[LineNo] = DDRInitData[LineNo] * 16 + my_atoi(Buff[src_c]);

        DDR_Line[LineNo][dest_c] = Buff[src_c];
        //sysprintf("c=[%d],Buff[c]=[%x],LineNo=%d,Line[%d][%d]=%x\n",src_c,Buff[src_c],LineNo,LineNo,dest_c,Line[LineNo][dest_c]);
        src_c++;
        dest_c++;
    }

    DDRInitCount += 1; //The last line

    printf("Total initail data count = %d\n",DDRInitCount);
}

static unsigned int CalculateCRC32(unsigned char * buf,unsigned int len)
{
    unsigned int i;
    unsigned int crc32;
    unsigned char* byteBuf;

    crc32 = 0 ^ 0xFFFFFFFF;
    byteBuf = (unsigned char *) buf;
    for (i=0; i < len; i++) {
        crc32 = (crc32 >> 8) ^ crc32_table[ (crc32 ^ byteBuf[i]) & 0xFF ];
    }
    return ( crc32 ^ 0xFFFFFFFF );
}

static void _parsing_EnvTxt(int FileLen, char *pEnv)
{
    int src_c =0;
    int dest_c = 4; /* The first four bytes are checksum */

    while (src_c < FileLen) {
        if (Buff[src_c] == 0) //EOF
            break;

        if ((Buff[src_c] == 0xD) && (Buff[src_c+1] == 0xA)) {//Line end
            pEnv[dest_c] = 0;
            src_c += 2;
            dest_c++;
        } else {
            pEnv[dest_c] = Buff[src_c];
            src_c++;
            dest_c++;
        }
    }

    *(unsigned int *)pEnv = CalculateCRC32((unsigned char *)(pEnv + 4), 0x10000 - 4);

    if (0) {
        int k;
        printf("ENV:\n");
        for (k=0; k<0x10000; k++) {
            printf("0x%x ",pEnv[k]);
            if (k && ((k+1)%16 == 0))
                printf("\n");
        }
        printf("\n");
    }

}

static void _parsing_SPINAND_EnvTxt(int FileLen, char *pEnv)
{
    int src_c =0;
    int dest_c = 4; /* The first four bytes are checksum */

    while (src_c < FileLen) {
        if (Buff[src_c] == 0) //EOF
            break;

        if ((Buff[src_c] == 0xD) && (Buff[src_c+1] == 0xA)) {//Line end
            pEnv[dest_c] = 0;
            src_c += 2;
            dest_c++;
        } else {
            pEnv[dest_c] = Buff[src_c];
            src_c++;
            dest_c++;
        }
    }

    *(unsigned int *)pEnv = CalculateCRC32((unsigned char *)(pEnv + 4), 0x20000 - 4);

    if (0) {
        int k;
        printf("ENV:\n");
        for (k=0; k<0x20000; k++) {
            printf("0x%x ",pEnv[k]);
            if (k && ((k+1)%16 == 0))
                printf("\n");
        }
        printf("\n");
    }

}

static void Form_BootCode_Header(unsigned int *header_size)
{
    FRESULT res;
    int i;

    memset((void*)Buff, 0xFF, BUFF_SIZE);
    printf("open DDR ini file [%s]\n", Ini_Writer.DDR.FileName);
    res = f_open(&file1, Ini_Writer.DDR.FileName, FA_OPEN_EXISTING | FA_READ);
    if (res)
        printf("res = %d\n",res);
    else
        printf("f_open [%s] ok\n", Ini_Writer.DDR.FileName);

    printf("read DDR ini file [%s]\n", Ini_Writer.DDR.FileName);
    res = f_read(&file1, Buff, BUFF_SIZE, &s1);
    if (res || s1 == 0) {
        printf("res = %d,read size = %d\n",res,s1);
        printf("Error while read file\n");
        while(1) {   /* error or eof */
            WDT_RSTCNT;
        }
    }

    _parsing_DDRInit(s1);

    //Form boot code header
    *(unsigned int*)(Buff + 0) = 0x4E565420; //Boot code marker
    *(unsigned int*)(Buff + 4) = Ini_Writer.Loader.address; //2nd word - Execution address
    *(unsigned int*)(Buff + 8) = Ini_Writer.Loader_size;  //3rd word - Image size
    *(unsigned int*)(Buff + 12) = 0xFFFFFFFF;

    if (Ini_Writer.UserDef_SPI.PageSizeDefined)
        *(unsigned int*)(Buff + 16) = ((Ini_Writer.UserDef_SPI.SpareArea) << 16)|(Ini_Writer.UserDef_SPI.PageSize);//0x00400800;
    else
        *(unsigned int*)(Buff + 16) = (info.SPINand_SpareArea << 16)|(info.SPINand_PageSize);//0x00400800;

    if (Ini_Writer.UserDef_SPI.QuadCmdDefined) {
        *(unsigned int*)(Buff + 20) = ((Ini_Writer.UserDef_SPI.QuadReadCmd)
                                       |(Ini_Writer.UserDef_SPI.ReadStatusCmd << 8)
                                       |(Ini_Writer.UserDef_SPI.WriteStatusCmd << 16)
                                       |(Ini_Writer.UserDef_SPI.StatusValue << 24)); //0x023135EB
        *(unsigned int*)(Buff + 24) = (Ini_Writer.UserDef_SPI.DummyByte) | 0xFFFFFF00;
    } else {
        *(unsigned int*)(Buff + 20) = ((info.SPINand_QuadReadCmd)
                                       |(info.SPINand_ReadStatusCmd << 8)
                                       |(info.SPINand_WriteStatusCmd << 16)
                                       |(info.SPINand_StatusValue << 24)); //0x023135EB
        *(unsigned int*)(Buff + 24) = (info.SPINand_dummybyte) | 0xFFFFFF00;
    }
    *(unsigned int*)(Buff + 28) = 0xFFFFFFFF;

    *(unsigned int*)(Buff + 32) = 0xAA55AA55;
    *(unsigned int*)(Buff + 32 + 4) = DDRInitCount;

    printf("Copy DDR init data to Buff\n");
    for (i=0; i<DDRInitCount; i++) {
        *(unsigned int*)(Buff + 32 + 8 + (8*i)) = DDRInitAddr[i];
        *(unsigned int*)(Buff + 32 + 8 + (8*i) + 4) = DDRInitData[i];
    }

    //U-Boot should be put in 16 byte align
    if ((DDRInitCount%2) == 0) {
        for (i=0; i<8; i++)
            Buff[32 + 8 + DDRInitCount*8 + i] = 0xff; //Set dumy byte to 0xff
    }

    if (DDRInitCount%2)
        *header_size = 32 + 8 + (DDRInitCount*8);
    else {
        *header_size = 32 + 8 + (DDRInitCount*8) + 8;
    }

    f_close(&file1);
}

int32_t main(void)
{
    char        *ptr, *ptr2;
    long        p1, p2, p3;
    BYTE        *buf;
    FATFS       *fs;              /* Pointer to file system object */
    TCHAR       sd_path[] = { '0', ':', 0 };    /* SD drive started from 0 */
    FRESULT     res;

    DIR dir;                /* Directory object */
    UINT s1, s2, cnt;
    DWORD ofs = 0, sect = 0;
    UINT ImgNo,ImageCnt;

    *(volatile unsigned int *)(CLK_BA+0x18) |= (1<<16); /* Enable UART0 */
    UART_Init();
    printf("\n");
    printf("===============================================\n");
    printf("          SD Writer                            \n");
    printf("===============================================\n");

    sysDisableCache();
    sysFlushCache(I_D_CACHE);
    sysEnableCache(CACHE_WRITE_BACK);

    Buff = (BYTE *)((UINT32)&Buff_Pool[0] | 0x80000000);   /* use non-cache buffer */
    Block_Buff = (BYTE *)((UINT32)&Block_Buff_Pool[0] | 0x80000000);   /* use non-cache buffer */

    SYS_Init();
    WDT_RSTCNT;
    outpw(REG_WDT_CTL, (inpw(REG_WDT_CTL) & ~(0xf << 8))|(0x8<<8));// timeout 2^20 * (12M/512) = 44 sec

    ETimer1_Init();
    fmiHWInit();

    if (((inpw(REG_SYS_PWRON) & 0x00000300) == 0x300)) {
        /* Use SD0 */
        printf("Reading SD card in SD port 0 .....\n\n");
        sd_path[0] = '0';
        sysInstallISR(IRQ_LEVEL_1, IRQ_FMI, (PVOID)SDH0_IRQHandler);
        /* enable CPSR I bit */
        sysSetLocalInterrupt(ENABLE_IRQ);
        sysEnableInterrupt(IRQ_FMI);

        SDH_Open_Disk(SDH0, CardDetect_From_GPIO);
        f_chdrive(sd_path);          /* set default path */
        SDH_CardDetection(SDH0);
    } else {
        /* Use SD1 */
        printf("Reading SD card in SD port 1 .....\n\n");
        sd_path[0] = '1';
        sysInstallISR(IRQ_LEVEL_1, IRQ_SDH, (PVOID)SDH_IRQHandler);
        /* enable CPSR I bit */
        sysSetLocalInterrupt(ENABLE_IRQ);
        sysEnableInterrupt(IRQ_SDH);

        SDH_Open_Disk(SDH1, CardDetect_From_GPIO);
        f_chdrive(sd_path);          /* set default path */
        SDH_CardDetection(SDH1);
    }

    while (*ptr == ' ') ptr++;
    res = f_opendir(&dir, ptr);
    if (res) {
        put_rc(res);
        printf("res = %d\n",res);
    }
    p1 = s1 = s2 = 0;
    for(;;) {
        WDT_RSTCNT;
        res = f_readdir(&dir, &Finfo);
        if ((res != FR_OK) || !Finfo.fname[0]) break;
        if (Finfo.fattrib & AM_DIR) {
            s2++;
        } else {
            s1++;
            p1 += Finfo.fsize;
        }
        printf("%c%c%c%c%c %d/%02d/%02d %02d:%02d    %9d  %s",
               (Finfo.fattrib & AM_DIR) ? 'D' : '-',
               (Finfo.fattrib & AM_RDO) ? 'R' : '-',
               (Finfo.fattrib & AM_HID) ? 'H' : '-',
               (Finfo.fattrib & AM_SYS) ? 'S' : '-',
               (Finfo.fattrib & AM_ARC) ? 'A' : '-',
               (Finfo.fdate >> 9) + 1980, (Finfo.fdate >> 5) & 15, Finfo.fdate & 31,
               (Finfo.ftime >> 11), (Finfo.ftime >> 5) & 63, (unsigned int)Finfo.fsize, Finfo.fname);
#if _USE_LFN
        for (p2 = strlen(Finfo.fname); p2 < 14; p2++)
            printf(" ");
        printf("%s\n", Lfname);
#else
        printf("\n");
#endif
        if (Finfo.fattrib & AM_DIR) {

        }
    }
    printf("%4d File(s),%10d bytes total\n%4d Dir(s)", s1, (int)p1, s2);
    if (f_getfree(ptr, (DWORD*)&p1, &fs) == FR_OK)
        printf(", %10d bytes free\n", (int)p1 * fs->csize * 512);



    printf("Opening \"%s\"", "config");
    res = f_open(&file1, "config", FA_OPEN_EXISTING | FA_READ);
    printf("\n");
    if (res) {
        put_rc(res);
        printf("[config] res = %d\n",res);
    }

    p1 = 0;
    for (;;) {
        WDT_RSTCNT;
        res = f_read(&file1, Buff, BUFF_SIZE, &s1);
        if (res || s1 == 0) break;   /* error or eof */
        printf("Buff = %s\n",Buff);
    }
    f_close(&file1);

    ProcessINI("config");
    //ProcessINI("SPI_NAND/SPI_NAND.cfg");

    WDT_RSTCNT;
    res = f_opendir(&dir, ptr);
    if (res) {
        put_rc(res);
        printf("res = %d\n",res);
    }
    ImageCnt = 0;
    for(;;) {
        int n,i;
        res = f_readdir(&dir, &Finfo);
        if ((res != FR_OK) || !Finfo.fname[0]) break;
        if (!(n = strcasecmp((const char*)Finfo.fname, (const char*)Ini_Writer.Loader.FileName))) {
            printf("Ini_Writer.Loader.FileName = [%s], Finfo.fname = [%s]\n",Ini_Writer.Loader.FileName,Finfo.fname);
            printf("Loader [%s] file size is %d\n",Finfo.fname, (unsigned int)Finfo.fsize);
            Ini_Writer.Loader_size = Finfo.fsize;
        }
        for (i = 0; i < MAX_USER_IMAGE; i++) {
            if (!(n = strcasecmp((const char*)Finfo.fname, (const char*)Ini_Writer.UserImage[i].FileName))) {
                printf("Ini_Writer.UserImage[%d].FileName = [%s], Finfo.fname = [%s]\n",i,Ini_Writer.UserImage[i].FileName,Finfo.fname);
                printf("Data%d [%s] file size is %d\n",i,Finfo.fname, (unsigned int)Finfo.fsize);
                Ini_Writer.UserImage[i].DataSize = Finfo.fsize;
                ImageCnt++;
            }
        }
    }

    if (Ini_Writer.Type == TYPE_SPI_NOR) {
        printf("Write Type is SPI NOR flash\n");

        spiInit();

        if (Ini_Writer.Erase.user_choice == 1) {
            WDT_RSTCNT;
            printf("EraseAll = %d\n",Ini_Writer.Erase.EraseAll);
            if (Ini_Writer.Erase.EraseAll == 1) { /* Erase whole chip */
                printf("Please wait for chip erase.....\n");
                spiEraseAll();
                spiCheckBusy();
                printf("Chip erase... done\n");
            } else { /* Erase partial */
                printf("EraseStart = %d, EraseLength = %d\n",Ini_Writer.Erase.EraseStart,Ini_Writer.Erase.EraseLength);
                spiEraseSector(Ini_Writer.Erase.EraseStart * SPI_BLOCK_SIZE, Ini_Writer.Erase.EraseLength);
                printf("Erase... done\n");
            }
        }

        if (Ini_Writer.Loader.user_choice == 1) {
            int i, offset=0, blockCount, len;
            unsigned int header_size;

            WDT_RSTCNT;
            printf("open [%s]\n", Ini_Writer.Loader.FileName);
            res = f_open(&file2, Ini_Writer.Loader.FileName, FA_OPEN_EXISTING | FA_READ);
            if (res)
                printf("result = %d\n",res);
            else
                printf("f_open [%s] ok\n", Ini_Writer.Loader.FileName);

            //Burn Loader to SPI flash
            printf("Write [%s] to SPI flash ... start\n",Ini_Writer.Loader.FileName);

            //Write the first block
            Form_BootCode_Header(&header_size);

            res = f_read(&file2, Buff + header_size, SPI_BLOCK_SIZE - header_size, &s2);
            if (res || s2 == 0) {
                printf("res = %d,read size = %d\n",res,s2);
                while(1) {   /* error or eof */
                    WDT_RSTCNT;
                }
            }
            printf("Burn_SPI  offset=0x%x(%d)\n", offset, offset);
            spiEraseSector(offset, 1);
            spiWrite(offset, SPI_BLOCK_SIZE, (UINT8 *)Buff);
            offset += SPI_BLOCK_SIZE;

            len = Ini_Writer.Loader_size - (SPI_BLOCK_SIZE - header_size);
            if (len > 0) {
                //Write following blocks
                blockCount = len / SPI_BLOCK_SIZE;
                printf("Burn_SPI: offset=0x%x  len=%d   blockCount= %d\n", offset, len, blockCount);

                for (i=0; i<blockCount; i++) {
                    WDT_RSTCNT;
                    printf("Burn_SPI  offset=0x%x(%d)\n", offset, offset);
                    Enable4ByteFlag = 0;
                    // 4Byte Address Mode (>16MByte)
                    if((offset + SPI_BLOCK_SIZE) > SPI_FLASH_SIZE)
                        Enable4ByteFlag = 1;
                    if(len > SPI_FLASH_SIZE)
                        Enable4ByteFlag = 1;

                    if(Enable4ByteFlag)
                        printf("Enable4ByteFlag %d:  offset=0x%08x(%d)\n", Enable4ByteFlag, offset, offset);

                    memset((void*)Buff, 0xFF, SPI_BLOCK_SIZE);
                    res = f_read(&file2, Buff, SPI_BLOCK_SIZE, &s2);
                    if (res || s2 == 0) {
                        printf("res = %d,read size = %d\n",res,s2);
                        while(1) {   /* error or eof */
                            WDT_RSTCNT;
                        }
                    }
                    ETimer1_cnt = 0;
                    ETIMER_Start(1);
                    spiEraseSector(offset, 1);
                    spiWrite(offset, SPI_BLOCK_SIZE, (UINT8 *)Buff);
                    ETIMER_Stop(1);
                    WDT_RSTCNT;

                    offset += SPI_BLOCK_SIZE;

                    //ack status
                    //tmplen += SPI_BLOCK_SIZE;
                    //SendAck((tmplen * 95) / len);
                }
                if ((len % (SPI_BLOCK_SIZE)) != 0) {
                    printf("Burn_SPI  offset=0x%x(%d)\n", offset, offset);
                    // 4Byte Address Mode (>16MByte)
                    Enable4ByteFlag = 0;
                    if((offset + SPI_BLOCK_SIZE) > SPI_FLASH_SIZE)
                        Enable4ByteFlag = 1;
                    if(offset > SPI_FLASH_SIZE)
                        Enable4ByteFlag = 1;

                    if(Enable4ByteFlag)
                        printf("Enable4ByteFlag %d:  offset=0x%08x(%d)\n", Enable4ByteFlag, offset, offset);

                    len = len - blockCount*SPI_BLOCK_SIZE;
                    memset((void*)Buff, 0xFF, SPI_BLOCK_SIZE);
                    res = f_read(&file2, Buff, len, &s2);
                    if (res || s2 == 0) {
                        printf("res = %d,read size = %d\n",res,s2);
                        while(1) {   /* error or eof */
                            WDT_RSTCNT;
                        }
                    }
                    ETimer1_cnt = 0;
                    ETIMER_Start(1);
                    spiEraseSector(offset, 1);
                    spiWrite(offset, len, (UINT8 *)Buff);
                    ETIMER_Stop(1);
                }
            }
            f_close(&file2);
            printf("Write [%s] to SPI flash ... done\n",Ini_Writer.Loader.FileName);
        }

        if (Ini_Writer.UserImage[0].user_choice == 1) {
            for (ImgNo = 0; ImgNo < ImageCnt; ImgNo++) {
                WDT_RSTCNT;
                printf("open [%s]\n", Ini_Writer.UserImage[ImgNo].FileName);
                res = f_open(&file2, Ini_Writer.UserImage[ImgNo].FileName, FA_OPEN_EXISTING | FA_READ);
                if (res)
                    printf("result = %d\n",res);
                else
                    printf("f_open [%s] ok\n", Ini_Writer.UserImage[ImgNo].FileName);

                //Burn to SPI flash
                spiInit();
                printf("Write [%s] to SPI flash offset [0x%x] ... start\n", Ini_Writer.UserImage[ImgNo].FileName,Ini_Writer.UserImage[ImgNo].address);
                if (1) {
                    int i, offset=0, blockCount;
                    unsigned int len;
                    len = Ini_Writer.UserImage[ImgNo].DataSize;
                    blockCount = len / SPI_BLOCK_SIZE;
                    offset = Ini_Writer.UserImage[ImgNo].address;
                    printf("Burn_SPI: offset=0x%x  len=%d   blockCount= %d\n", offset, Ini_Writer.UserImage[ImgNo].DataSize, blockCount);

                    for (i=0; i<blockCount; i++) {
                        WDT_RSTCNT;
                        printf("Burn_SPI  offset=0x%x(%d)\n", offset, offset);
                        Enable4ByteFlag = 0;
                        // 4Byte Address Mode (>16MByte)
                        if((offset + SPI_BLOCK_SIZE) > SPI_FLASH_SIZE)
                            Enable4ByteFlag = 1;
                        if(Ini_Writer.UserImage[ImgNo].DataSize > SPI_FLASH_SIZE)
                            Enable4ByteFlag = 1;

                        if(Enable4ByteFlag)
                            printf("Enable4ByteFlag %d:  offset=0x%08x(%d)\n", Enable4ByteFlag, offset, offset);

                        memset((void*)Buff, 0xFF, SPI_BLOCK_SIZE);
                        res = f_read(&file2, Buff, SPI_BLOCK_SIZE, &s2);
                        if (res || s2 == 0) {
                            printf("res = %d,read size = %d\n",res,s2);
                            while(1) {   /* error or eof */
                                WDT_RSTCNT;
                            }
                        }

                        ETimer1_cnt = 0;
                        ETIMER_Start(1);
                        spiEraseSector(offset, 1);
                        spiWrite(offset, SPI_BLOCK_SIZE, (UINT8 *)Buff);
                        ETIMER_Stop(1);
                        WDT_RSTCNT;

                        offset += SPI_BLOCK_SIZE;

                        //ack status
                        //tmplen += SPI_BLOCK_SIZE;
                        //SendAck((tmplen * 95) / len);
                    }
                    if ((len % (SPI_BLOCK_SIZE)) != 0) {
                        WDT_RSTCNT;
                        printf("Burn_SPI  offset=0x%x(%d)\n", offset, offset);
                        // 4Byte Address Mode (>16MByte)
                        Enable4ByteFlag = 0;
                        if((offset + SPI_BLOCK_SIZE) > SPI_FLASH_SIZE)
                            Enable4ByteFlag = 1;
                        if(offset > SPI_FLASH_SIZE)
                            Enable4ByteFlag = 1;

                        if(Enable4ByteFlag)
                            printf("Enable4ByteFlag %d:  offset=0x%08x(%d)\n", Enable4ByteFlag, offset, offset);

                        len = len - blockCount*SPI_BLOCK_SIZE;
                        memset((void*)Buff, 0xFF, SPI_BLOCK_SIZE);
                        res = f_read(&file2, Buff, len, &s2);
                        if (res || s2 == 0) {
                            printf("res = %d,read size = %d\n",res,s2);
                            while(1) {   /* error or eof */
                                WDT_RSTCNT;
                            }
                        }
                        ETimer1_cnt = 0;
                        ETIMER_Start(1);
                        spiEraseSector(offset, 1);
                        spiWrite(offset, len, (UINT8 *)Buff);
                        ETIMER_Stop(1);
                        WDT_RSTCNT;
                    }
                }

                f_close(&file2);
                printf("Write [%s] to SPI flash ... done\n",Ini_Writer.UserImage[ImgNo].FileName);
            }
        }

        if (Ini_Writer.Env.user_choice == 1) {
            WDT_RSTCNT;
            printf("open [%s]\n",Ini_Writer.Env.FileName);
            result = f_open(&file2, Ini_Writer.Env.FileName, FA_OPEN_EXISTING | FA_READ);
            if (result)
                printf("result = %d\n",result);
            else
                printf("f_open [%s] ok\n",Ini_Writer.Env.FileName);

            printf("read [%s]\n",Ini_Writer.Env.FileName);
            result = f_read(&file2, Buff, SPI_BLOCK_SIZE, &s2);
            if (result || s2 == 0) {
                printf("result = %d,read size = %d\n",result,s2);
                //break;   /* error or eof */
            } else
                printf("[%s] size = %d\n",Ini_Writer.Env.FileName,s2);

            pENV = (char*)malloc(0x10000);
            if (!pENV) {
                printf("malloc fail [%s][%d]\n",__FUNCTION__,__LINE__);
                while(1) {
                    WDT_RSTCNT;
                }
            }
            memset((void*)pENV,0,0x10000);
            _parsing_EnvTxt(s2,pENV);
            f_close(&file2);

            //Burn to SPI flash
            printf("Write Environment variable to SPI flash offset [%x] ... start\n", Ini_Writer.Env.address);
            ETimer1_cnt = 0;
            ETIMER_Start(1);
            spiEraseSector(Ini_Writer.Env.address, 1);
            spiWrite(Ini_Writer.Env.address, SPI_BLOCK_SIZE, (UINT8 *)pENV);
            ETIMER_Stop(1);
            WDT_RSTCNT;
            printf("Write Environment variable to SPI flash ... done\n");
        }
    }
    if (Ini_Writer.Type == TYPE_SPI_NAND) {
        printf("Write Type is SPI NAND\n");
        /* Initial SPI NAND */
        spiNANDInit();

        if (Ini_Writer.Erase.user_choice == 1) {
            uint8_t volatile SR;
            uint32_t volatile blockNum, PA_Num;
            printf("EraseAll = %d\n",Ini_Writer.Erase.EraseAll);
            if (Ini_Writer.Erase.EraseAll == 1) { /* Erase whole chip */
                for (blockNum=0; blockNum < pSN->SPINand_BlockPerFlash; blockNum++) {
                    PA_Num = blockNum*pSN->SPINand_PagePerBlock;
                    if(spiNAND_bad_block_check(PA_Num) == 1) {
                        printf("bad_block:%d\n", blockNum);
                        continue;
                    } else {
                        spiNAND_BlockErase( (PA_Num>>8)&0xFF, PA_Num&0xFF);
                        SR = spiNAND_Check_Program_Erase_Fail_Flag();
                        if (SR != 0) {
                            spiNANDMarkBadBlock(PA_Num);
                            printf("Error erase status! bad_block:%d\n", blockNum);
                        } else {
                            printf("BlockErase %d Done\n", blockNum);
                        }
                    }
                }
            } else { /* Erase partial */
                printf("EraseStart = %d, EraseLength = %d\n",Ini_Writer.Erase.EraseStart,Ini_Writer.Erase.EraseLength);
                for(blockNum=Ini_Writer.Erase.EraseStart; blockNum < (Ini_Writer.Erase.EraseStart+Ini_Writer.Erase.EraseLength); blockNum++) {
                    PA_Num = blockNum*pSN->SPINand_PagePerBlock;
                    if (spiNAND_bad_block_check(PA_Num) == 1) {
                        printf("bad_block:%d\n", blockNum);
                        continue;
                    }
                    spiNAND_BlockErase( (PA_Num>>8)&0xFF, PA_Num&0xFF);
                    SR = spiNAND_Check_Program_Erase_Fail_Flag();
                    if (SR != 0) {
                        printf("Error erase status! bad_block:%d\n", blockNum);
                        spiNANDMarkBadBlock(PA_Num);
                    } else {
                        printf("BlockErase %d Done\n", blockNum);
                    }
                }
            }
        }

        if (Ini_Writer.Loader.user_choice == 1) {
            unsigned int i,write_len,blkindx,end_blk,page,page_count;
            unsigned char status;
            unsigned char *addr;
            int offset=0, blockCount, len;
            unsigned int header_size;

            WDT_RSTCNT;
            printf("open [%s]\n", Ini_Writer.Loader.FileName);
            res = f_open(&file2, Ini_Writer.Loader.FileName, FA_OPEN_EXISTING | FA_READ);
            if (res)
                printf("result = %d\n",res);
            else
                printf("f_open [%s] ok\n", Ini_Writer.Loader.FileName);

            //Burn to SPI NAND flash
            printf("Write SPL [%s] to SPI NAND flash ... start\n",Ini_Writer.Loader.FileName);

            //Write the first block
            Form_BootCode_Header(&header_size);

            write_len = MIN(((pSN->SPINand_PagePerBlock)*(pSN->SPINand_PageSize)), Ini_Writer.Loader_size);
            res = f_read(&file2, Buff + header_size, write_len, &s2);
            if (res) {
                printf("res = %d,read size = %d\n",res,s2);
                while(1) {   /* error or eof */
                    WDT_RSTCNT;
                }
            }

            page_count = write_len/pSN->SPINand_PageSize;
            if (write_len % pSN->SPINand_PageSize)
                page_count++;

            // erase block
            end_blk = 4;
            for(blkindx=0; blkindx<end_blk; blkindx++) {
_retry_spinand_1:
                if (blkindx > pSN->SPINand_BlockPerFlash) {
                    printf("Write out of SPI NAND flash! blkindx[%d], BlockPerFlash = %d\n", blkindx, pSN->SPINand_BlockPerFlash);
                    while(1) {
                        WDT_RSTCNT;
                    }
                }
                WDT_RSTCNT;
                addr = (unsigned char*)Buff;
                page = pSN->SPINand_PagePerBlock * (blkindx);
                printf("blkindx = %d   page = %d   page_count=%d\n", blkindx, page, page_count);
                if(spiNAND_bad_block_check(page) == 1) {
                    printf("bad block = %d\n", blkindx);
                    blkindx++;
                    end_blk++;
                    goto _retry_spinand_1;
                } else {
                    spiNAND_BlockErase(((page>>8)&0xFF), (page&0xFF)); // block erase
                    status = spiNAND_Check_Program_Erase_Fail_Flag();
                    if (status != 0) {
                        printf("Error erase status! spiNANDMarkBadBlock blockNum = %d\n", blkindx);
                        spiNANDMarkBadBlock(blkindx*pSN->SPINand_PagePerBlock);
                        blkindx++;
                        end_blk++;
                        goto _retry_spinand_1;
                    }
                }

                // write block
                for (i=0; i<page_count; i++) {
                    //printf("page+i=%d\n",page+i);
                    WDT_RSTCNT;
                    ETimer1_cnt = 0;
                    ETIMER_Start(1);
                    spiNAND_Pageprogram_Pattern(0, 0, (uint8_t*)addr, pSN->SPINand_PageSize);
                    spiNAND_Program_Excute((((page+i)>>8)&0xFF), (page+i)&0xFF);
                    ETIMER_Stop(1);
                    WDT_RSTCNT;
                    status = (spiNAND_StatusRegister(3) & 0x0C)>>2;
                    if (status != 0) {
                        spiNANDMarkBadBlock(blkindx*pSN->SPINand_PagePerBlock);
                        printf("Error write status! Bad block[%d]!\n",blkindx);
                        blkindx++;
                        end_blk++;
                        goto _retry_spinand_1;
                    }
                    addr += pSN->SPINand_PageSize;
                    //pagetmp++;
                }
            }
            f_close(&file2);
            printf("Write [%s] to SPI NAND flash ... done\n",Ini_Writer.Loader.FileName);
        }

        if (Ini_Writer.UserImage[0].user_choice == 1) {
            unsigned int i,blkindx,startblk,endblk,page,blk_count,page_count,remain_size,len;
            unsigned char status;
            unsigned int addr;

            for (ImgNo = 0; ImgNo < ImageCnt; ImgNo++) {
                WDT_RSTCNT;
                printf("open [%s]\n", Ini_Writer.UserImage[ImgNo].FileName);
                res = f_open(&file2, Ini_Writer.UserImage[ImgNo].FileName, FA_OPEN_EXISTING | FA_READ);
                if (res)
                    printf("result = %d\n",res);
                else
                    printf("f_open [%s] ok\n", Ini_Writer.UserImage[ImgNo].FileName);


                //Burn to SPI NAND flash
                printf("Write [%s] to SPI NAND flash offset [0x%x] ... start\n", Ini_Writer.UserImage[ImgNo].FileName,Ini_Writer.UserImage[ImgNo].address);

                blk_count = Ini_Writer.UserImage[ImgNo].DataSize/(pSN->SPINand_PagePerBlock * pSN->SPINand_PageSize);
                printf("Img[%d] size = %d, blk_count = %d\n",ImgNo,Ini_Writer.UserImage[ImgNo].DataSize,blk_count);

                startblk = Ini_Writer.UserImage[ImgNo].address/((pSN->SPINand_PagePerBlock) * (pSN->SPINand_PageSize));
                endblk = startblk + (Ini_Writer.UserImage[ImgNo].DataSize/((pSN->SPINand_PagePerBlock)*(pSN->SPINand_PageSize)));
                printf("startblk = %d, endblk = %d\n",startblk,endblk);

                page_count = pSN->SPINand_PagePerBlock;

                // erase block
                for(blkindx = startblk; blkindx <= endblk; blkindx++) {
                    len = MIN(((pSN->SPINand_PagePerBlock)*(pSN->SPINand_PageSize)), Ini_Writer.UserImage[ImgNo].DataSize);
                    memset((void*)Buff, 0xFF, BUFF_SIZE);
                    res = f_read(&file2, Buff, len, &s2);
                    //printf("len = %d, s2 = %d\n",len,s2);
                    if (res) {
                        printf("res = %d,read size = %d\n",res,s2);
                        while(1) {   /* error or eof */
                            WDT_RSTCNT;
                        }
                    }
_retry_spinand_2:
                    if (blkindx > pSN->SPINand_BlockPerFlash) {
                        printf("Write out of SPI NAND flash! blkindx[%d], BlockPerFlash = %d\n", blkindx, pSN->SPINand_BlockPerFlash);
                        while(1) {
                            WDT_RSTCNT;
                        }
                    }
                    WDT_RSTCNT;
                    page = pSN->SPINand_PagePerBlock * (blkindx);
                    addr = (unsigned int)Buff;
                    printf("blkindx = %d   page = %d   page_count=%d\n", blkindx, page, page_count);
                    if(spiNAND_bad_block_check(page) == 1) {
                        printf("bad block = %d\n", blkindx);
                        blkindx++;
                        endblk++;
                        goto _retry_spinand_2;
                    } else {
                        spiNAND_BlockErase(((page>>8)&0xFF), (page&0xFF)); // block erase
                        status = spiNAND_Check_Program_Erase_Fail_Flag();
                        if (status != 0) {
                            printf("Error erase status! spiNANDMarkBadBlock blockNum = %d\n", blkindx);
                            spiNANDMarkBadBlock(blkindx*pSN->SPINand_PagePerBlock);
                            blkindx++;
                            endblk++;
                            goto _retry_spinand_2;
                        }
                    }
                    // write block
                    for (i=0; i<pSN->SPINand_PagePerBlock; i++) {
                        //printf("page+i=%d\n",page+i);
                        WDT_RSTCNT;

                        ETimer1_cnt = 0;
                        ETIMER_Start(1);
                        spiNAND_Pageprogram_Pattern(0, 0, (uint8_t*)addr, pSN->SPINand_PageSize);
                        spiNAND_Program_Excute((((page+i)>>8)&0xFF), (page+i)&0xFF);
                        ETIMER_Stop(1);
                        WDT_RSTCNT;
                        status = (spiNAND_StatusRegister(3) & 0x0C)>>2;
                        if (status != 0) {
                            spiNANDMarkBadBlock(blkindx*pSN->SPINand_PagePerBlock);
                            printf("Error write status! Bad block[%d]!\n",blkindx);
                            blkindx++;
                            endblk++;
                            goto _retry_spinand_2;
                        }
                        addr += pSN->SPINand_PageSize;
                    }
                }
                f_close(&file2);
                printf("Write [%s] to SPI NAND flash ... done\n",Ini_Writer.UserImage[ImgNo].FileName);
            }
        }

        if (Ini_Writer.Env.user_choice == 1) {
            unsigned int i,blkindx,page,page_count;
            unsigned char status;
            unsigned int addr;

            WDT_RSTCNT;
            printf("open [%s]\n",Ini_Writer.Env.FileName);
            result = f_open(&file2, Ini_Writer.Env.FileName, FA_OPEN_EXISTING | FA_READ);
            if (result)
                printf("result = %d\n",result);
            else
                printf("f_open [%s] ok\n",Ini_Writer.Env.FileName);

            printf("read [%s]\n",Ini_Writer.Env.FileName);
            result = f_read(&file2, Buff, BUFF_SIZE, &s2);
            if (result || s2 == 0) {
                printf("result = %d,read size = %d\n",result,s2);
                //break;   /* error or eof */
            } else
                printf("[%s] size = %d\n",Ini_Writer.Env.FileName,s2);

            pENV = (char*)malloc(0x20000);
            if (!pENV) {
                printf("malloc fail [%s][%d]\n",__FUNCTION__,__LINE__);
                while(1) {
                    WDT_RSTCNT;
                }
            }
            memset((void*)pENV,0,0x20000);
            _parsing_SPINAND_EnvTxt(s2,pENV);
            f_close(&file2);

            //Burn to SPI NAND flash
            printf("Write Environment variable to SPI NAND flash offset [0x%x] ... start\n", Ini_Writer.Env.address);


            page_count = pSN->SPINand_PagePerBlock;
            blkindx=Ini_Writer.Env.address/(pSN->SPINand_PagePerBlock * pSN->SPINand_PageSize);

_retry_spinand_3:
            if (blkindx > pSN->SPINand_BlockPerFlash) {
                printf("Write out of SPI NAND flash! blkindx[%d], BlockPerFlash = %d\n", blkindx, pSN->SPINand_BlockPerFlash);
                while(1) {
                    WDT_RSTCNT;
                }
            }
            // erase block
            WDT_RSTCNT;
            addr = (unsigned int)pENV;
            page = pSN->SPINand_PagePerBlock * (blkindx);
            printf("blkindx = %d   page = %d   page_count=%d\n", blkindx, page, page_count);
            if(spiNAND_bad_block_check(page) == 1) {
                printf("bad block = %d\n", blkindx);
                blkindx++;
                goto _retry_spinand_3;
            } else {
                spiNAND_BlockErase(((page>>8)&0xFF), (page&0xFF)); // block erase
                status = spiNAND_Check_Program_Erase_Fail_Flag();
                if (status != 0) {
                    printf("Error erase status! spiNANDMarkBadBlock blockNum = %d\n", blkindx);
                    spiNANDMarkBadBlock(blkindx*pSN->SPINand_PagePerBlock);
                    blkindx++;
                    goto _retry_spinand_3;
                }
            }
            // write block
            for (i=0; i<page_count; i++) {
                //printf("page+i=%d\n",page+i);
                WDT_RSTCNT;
                ETimer1_cnt = 0;
                ETIMER_Start(1);
                spiNAND_Pageprogram_Pattern(0, 0, (uint8_t*)addr, pSN->SPINand_PageSize);
                spiNAND_Program_Excute((((page+i)>>8)&0xFF), (page+i)&0xFF);
                ETIMER_Stop(1);
                WDT_RSTCNT;
                status = (spiNAND_StatusRegister(3) & 0x0C)>>2;
                if (status != 0) {
                    spiNANDMarkBadBlock(blkindx*pSN->SPINand_PagePerBlock);
                    printf("Error write status! Bad block[%d]!\n",blkindx);
                    blkindx++;
                    goto _retry_spinand_3;
                }
                addr += pSN->SPINand_PageSize;
            }
            printf("Write Environment variable to SPI NAND flash ... done\n");
        }
    }
    if (Ini_Writer.Type == TYPE_NAND) {
        uint32_t chip_size;

        printf("Write Type is NAND flash\n");
        /* initial NAND */
        fmiNandInit();
        printf("g_uIsUserConfig=%d\n",g_uIsUserConfig);
        printf("UXmodem_NAND BlockPerFlash=%d\n",pSM->uBlockPerFlash);
        printf("UXmodem_NAND PagePerBlock=%d\n",pSM->uPagePerBlock);
        printf("UXmodem_NAND PageSize=%d\n",pSM->uPageSize);
        memset((char *)&nandImage, 0, sizeof(FW_NAND_IMAGE_T));
        pNandImage = (FW_NAND_IMAGE_T *)&nandImage;

        chip_size = (pSM->uBlockPerFlash)*(pSM->uPagePerBlock)*(pSM->uPageSize);
        printf("Chip size = 0x%x\n",chip_size);

        if (Ini_Writer.Erase.user_choice == 1) {
            int bad_block;
            printf("EraseAll = %d\n",Ini_Writer.Erase.EraseAll);
            if (Ini_Writer.Erase.EraseAll == 1) { /* Erase whole chip */
                bad_block = fmiSM_ChipErase(0);
                if (bad_block < 0) {
                    printf("ERROR: %d bad block\n", bad_block); // storage error
                } else {
                    printf("total %d bad block\n", bad_block);
                }
            } else { /* Erase partial */
                printf("EraseStart = %d, EraseLength = %d\n",Ini_Writer.Erase.EraseStart,Ini_Writer.Erase.EraseLength);
                bad_block = fmiSM_Erase(0,Ini_Writer.Erase.EraseStart,Ini_Writer.Erase.EraseLength);
                printf("total %d bad block\n", bad_block);
            }
        }

        if (Ini_Writer.Loader.user_choice == 1) {
            unsigned int header_size;
            int blkindx,page,status,i,page_count,end_blk;

            //Burn Loader to NAND flash
            WDT_RSTCNT;
            printf("Write [%s] to NAND flash ... start\n",Ini_Writer.Loader.FileName);
            Form_BootCode_Header(&header_size);
            page_count = (Ini_Writer.Loader_size + header_size)/pSM->uPageSize;
            end_blk = 4;
            for(blkindx=0; blkindx<end_blk; blkindx++) {
                unsigned int len,addr;
                printf("open SPL [%s]\n", Ini_Writer.Loader.FileName);
                res = f_open(&file2, Ini_Writer.Loader.FileName, FA_OPEN_EXISTING | FA_READ);
                if (res)
                    printf("result = %d\n",res);
                else
                    printf("f_open SPL [%s] ok\n", Ini_Writer.Loader.FileName);

                len = MIN(((pSM->uPagePerBlock)*(pSM->uPageSize)), Ini_Writer.Loader_size);
                res = f_read(&file2, Buff + header_size, len, &s2);
                if (res) {
                    printf("res = %d,read size = %d\n",res,s2);
                    while(1) {   /* error or eof */
                        WDT_RSTCNT;
                    }
                }
_retry_1:
                if (blkindx > pSM->uBlockPerFlash) {
                    printf("Write out of NAND flash!\n");
                    while(1) {
                        WDT_RSTCNT;
                    }
                }
                page = pSM->uPagePerBlock * (blkindx);
                addr = (unsigned int)Buff;
                printf("Erase block [%d]\n",blkindx);
                status = fmiSM_BlockErase(pSM, blkindx);
                if (status != 0) {
                    fmiMarkBadBlock(pSM, blkindx);
                    printf("Bad block [%d]\n",blkindx);
                    blkindx++;
                    end_blk++;
                    goto _retry_1;
                }

                // write block
                for (i=0; i<page_count; i++) {
                    //printf("write page [%d]\n",page+i);
                    ETimer1_cnt = 0;
                    ETIMER_Start(1);
                    status = fmiSM_Write_large_page(page+i, 0, addr);
                    ETIMER_Stop(1);
                    if (status != 0) {
                        fmiMarkBadBlock(pSM, blkindx);
                        printf("Bad block [%d]\n",blkindx);
                        blkindx++;
                        end_blk++;
                        goto _retry_1;
                    }
                    addr += pSM->uPageSize;
                }
                //Write remaining data that is less than a page
                printf("write remaining [%d] byte to page[%d]\n",(Ini_Writer.Loader_size + header_size) % pSM->uPageSize,page_count);
                if ((Ini_Writer.Loader_size + header_size) % pSM->uPageSize) {
                    ETimer1_cnt = 0;
                    ETIMER_Start(1);
                    status = fmiSM_Write_large_page(page+page_count, 0, addr);
                    ETIMER_Stop(1);
                    if (status != 0) {
                        fmiMarkBadBlock(pSM, blkindx);
                        printf("Bad block [%d]\n",blkindx);
                        blkindx++;
                        end_blk++;
                        goto _retry_1;
                    }
                }
                f_close(&file2);
            }
            printf("Write [%s] to NAND flash ... done\n",Ini_Writer.Loader.FileName);
        }

        if (Ini_Writer.UserImage[0].user_choice == 1) {
            unsigned int addr;
            int blkindx,startblk,endblk,page,status,i;
            for (ImgNo = 0; ImgNo < ImageCnt; ImgNo++) {
                WDT_RSTCNT;
                printf("open [%s]\n", Ini_Writer.UserImage[ImgNo].FileName);
                res = f_open(&file2, Ini_Writer.UserImage[ImgNo].FileName, FA_OPEN_EXISTING | FA_READ);
                if (res)
                    printf("result = %d\n",res);
                else
                    printf("f_open [%s] ok\n", Ini_Writer.UserImage[ImgNo].FileName);

                //Burn to NAND flash
                printf("Write [%s] size [%d] to NAND flash offset [0x%x] ... start\n", Ini_Writer.UserImage[ImgNo].FileName, Ini_Writer.UserImage[ImgNo].DataSize, Ini_Writer.UserImage[ImgNo].address);

                startblk = Ini_Writer.UserImage[ImgNo].address/((pSM->uPagePerBlock)*(pSM->uPageSize));
                endblk = startblk + (Ini_Writer.UserImage[ImgNo].DataSize/((pSM->uPagePerBlock)*(pSM->uPageSize)));
                printf("startblk[%d], endblk[%d]\n",startblk,endblk);
                for(blkindx = startblk; blkindx <= endblk; blkindx++) {
                    unsigned int len;
                    WDT_RSTCNT;
                    len = MIN(((pSM->uPagePerBlock)*(pSM->uPageSize)), Ini_Writer.UserImage[ImgNo].DataSize);
                    memset((void*)Block_Buff, 0xFF, 512*1024);
                    res = f_read(&file2, Block_Buff, len, &s2);
                    if (res) {
                        printf("res = %d,read size = %d\n",res,s2);
                        while(1) {   /* error or eof */
                            WDT_RSTCNT;
                        }
                    }
_retry_2:

                    if (blkindx > pSM->uBlockPerFlash) {
                        printf("Write out of NAND flash!\n");
                        while(1) {
                            WDT_RSTCNT;
                        }
                    }
                    page = pSM->uPagePerBlock * (blkindx);
                    printf("Erase block [%d]\n",blkindx);
                    status = fmiSM_BlockErase(pSM, blkindx);
                    if (status != 0) {
                        fmiMarkBadBlock(pSM, blkindx);
                        printf("Bad block [%d]\n",blkindx);
                        blkindx++;
                        endblk++;
                        goto _retry_2;
                    }

                    // write block
                    // read BUFF_SIZE one time
                    page = blkindx*(pSM->uPagePerBlock);
                    addr = (unsigned int)Block_Buff;
                    for (i=0; i<pSM->uPagePerBlock; i++) {
                        //printf("write page[%d]\n",page+i);
                        ETimer1_cnt = 0;
                        ETIMER_Start(1);
                        status = fmiSM_Write_large_page(page+i, 0, addr);
                        ETIMER_Stop(1);
                        WDT_RSTCNT;
                        if (status != 0) {
                            fmiMarkBadBlock(pSM, blkindx);
                            printf("Bad block [%d]\n",blkindx);
                            blkindx++;
                            endblk++;
                            goto _retry_2;
                        }
                        addr += pSM->uPageSize;
                        //pagetmp++;
                    }
                }

                f_close(&file2);
                printf("Write [%s] to NAND flash ... done\n", Ini_Writer.UserImage[ImgNo].FileName);
            }
        }

        if (Ini_Writer.Env.user_choice == 1) {
            unsigned int addr;
            int blkindx,page,status,i,page_count;

            WDT_RSTCNT;
            printf("open [%s]\n",Ini_Writer.Env.FileName);
            result = f_open(&file2, Ini_Writer.Env.FileName, FA_OPEN_EXISTING | FA_READ);
            if (result)
                printf("result = %d\n",result);
            else
                printf("f_open [%s] ok\n",Ini_Writer.Env.FileName);

            printf("read [%s]\n",Ini_Writer.Env.FileName);
            result = f_read(&file2, Buff, BUFF_SIZE, &s2);
            if (result || s2 == 0) {
                printf("result = %d,read size = %d\n",result,s2);
                //break;   /* error or eof */
            } else
                printf("[%s] size = %d\n",Ini_Writer.Env.FileName,s2);

            pENV = (char*)malloc(0x10000);
            if (!pENV) {
                printf("malloc fail [%s][%d]\n",__FUNCTION__,__LINE__);
                while(1) {
                    WDT_RSTCNT;
                }
            }
            memset((void*)pENV,0,0x10000);
            _parsing_EnvTxt(s2,pENV);
            f_close(&file2);

            //Burn to NAND flash
            printf("Write Environment variable to NAND flash offset [%x] ... start\n", Ini_Writer.Env.address);
            addr = (unsigned int)pENV;
            page_count = pSM->uPagePerBlock;
            //blkindx = Ini_Writer.Env.address/(Ini_Writer.Env.DataSize/((pSM->uPagePerBlock)*(pSM->uPageSize)));
            blkindx = Ini_Writer.Env.address/((pSM->uPagePerBlock)*(pSM->uPageSize));
_retry_3:
            if (blkindx > pSM->uBlockPerFlash) {
                printf("Write out of NAND flash!\n");
                while(1) {
                    WDT_RSTCNT;
                }
            }
            page = pSM->uPagePerBlock * (blkindx);
            printf("Erase block [%d]\n",blkindx);
            status = fmiSM_BlockErase(pSM, blkindx);
            if (status != 0) {
                fmiMarkBadBlock(pSM, blkindx);
                blkindx++;
                goto _retry_3;
            }

            // write block
            for (i=0; i<page_count; i++) {
                //printf("write page [%d]\n",page+i);
                ETimer1_cnt = 0;
                ETIMER_Start(1);
                status = fmiSM_Write_large_page(page+i, 0, addr);
                ETIMER_Stop(1);
                if (status != 0) {
                    fmiMarkBadBlock(pSM, blkindx);
                    printf("Bad block [%d]\n",blkindx);
                    blkindx++;
                    goto _retry_3;
                }
                addr += pSM->uPageSize;
                //pagetmp++;
            }
            printf("Write Environment variable to NAND flash ... done\n");
        }
    }
    if (Ini_Writer.Type == TYPE_EMMC) {
        int eMMCBlockSize, len;

        printf("Write Type is EMMC\n");
        /* initial eMMC */
        eMMCBlockSize=fmiInitSDDevice();
        if(eMMCBlockSize>0) {
            info.EMMC_uReserved=GetMMCReserveSpace();
            printf("eMMC_uReserved =%d ...\n",info.EMMC_uReserved);
            info.EMMC_uBlock=eMMCBlockSize;
        }

        printf("eMMCBlockSize=0x%08x(%d) \n",eMMCBlockSize, eMMCBlockSize);

        if (Ini_Writer.EMMC_Format.user_choice == 1) {
            PMBR pmbr;
            unsigned int *ptr;
            UCHAR _fmi_ucBuffer[512];
            ptr = (unsigned int *)((UINT32)_fmi_ucBuffer | 0x80000000);

            printf("Format eMMC !!!\n");
            printf("Reserved space: [%d] sector\n",Ini_Writer.EMMC_Format.ReservedSpace);
            printf("Partition Number: [%d]\n",Ini_Writer.EMMC_Format.PartitionNum);
            printf("Partition 1 size: [%d] MB\n",Ini_Writer.EMMC_Format.Partition1Size);
            printf("Partition 2 size: [%d] MB\n",Ini_Writer.EMMC_Format.Partition2Size);
            printf("Partition 3 size: [%d] MB\n",Ini_Writer.EMMC_Format.Partition3Size);
            printf("Partition 4 size: [%d] MB\n",Ini_Writer.EMMC_Format.Partition4Size);

            memset((char *)&mmcImage, 0, sizeof(FW_MMC_IMAGE_T));
            pmmcImage = (FW_MMC_IMAGE_T*)&mmcImage;
            pmmcImage->ReserveSize = Ini_Writer.EMMC_Format.ReservedSpace;
            pmmcImage->PartitionNum = Ini_Writer.EMMC_Format.PartitionNum;
            pmmcImage->Partition1Size = Ini_Writer.EMMC_Format.Partition1Size;
            pmmcImage->Partition2Size = Ini_Writer.EMMC_Format.Partition2Size;
            pmmcImage->Partition3Size = Ini_Writer.EMMC_Format.Partition3Size;
            pmmcImage->Partition4Size = Ini_Writer.EMMC_Format.Partition4Size;

            if(eMMCBlockSize>0) {
                pmbr=create_mbr(eMMCBlockSize, pmmcImage);
                switch(pmmcImage->PartitionNum) {
                case 1:
                    ETimer1_cnt = 0;
                    FormatFat32(pmbr,0);
                    break;
                case 2: {
                    ETimer1_cnt = 0;
                    FormatFat32(pmbr,0);
                    ETimer1_cnt = 0;
                    FormatFat32(pmbr,1);
                }
                break;
                case 3: {
                    ETimer1_cnt = 0;
                    FormatFat32(pmbr,0);
                    ETimer1_cnt = 0;
                    FormatFat32(pmbr,1);
                    ETimer1_cnt = 0;
                    FormatFat32(pmbr,2);
                }
                break;
                case 4: {
                    FormatFat32(pmbr,0);
                    FormatFat32(pmbr,1);
                    FormatFat32(pmbr,2);
                    FormatFat32(pmbr,3);
                }
                break;
                }

                fmiSD_Read(MMC_INFO_SECTOR,1,(UINT32)ptr);
                *(ptr+125)=0x11223344;
                *(ptr+126)=pmmcImage->ReserveSize;
                *(ptr+127)=0x44332211;
                fmiSD_Write(MMC_INFO_SECTOR,1,(UINT32)ptr);
            }
        }
        if (Ini_Writer.Loader.user_choice == 1) {
            unsigned int header_size;
            int offset = 0x400;

            WDT_RSTCNT;
            printf("open [%s]\n", Ini_Writer.Loader.FileName);
            res = f_open(&file2, Ini_Writer.Loader.FileName, FA_OPEN_EXISTING | FA_READ);
            if (res)
                printf("result = %d\n",res);
            else
                printf("f_open [%s] ok\n", Ini_Writer.Loader.FileName);

            //Burn Loader to eMMC
            printf("Write [%s] to eMMC ... start\n",Ini_Writer.Loader.FileName);
            Form_BootCode_Header(&header_size);

            res = f_read(&file2, Buff + header_size, (EMMC_BLOCK_SIZE - header_size), &s2);
            if (res || s2 == 0) {
                printf("res = %d,read size = %d\n",res,s2);
                while(1) {   /* error or eof */
                    WDT_RSTCNT;
                }
            }
            fmiSD_Write((0x400/SD_SECTOR),(s2 + header_size)/SD_SECTOR,(UINT32)Buff);
            offset += EMMC_BLOCK_SIZE;
            //Write remain part of loader
            len = Ini_Writer.Loader_size - (EMMC_BLOCK_SIZE - header_size);
            if (len > 0) {
                int read_idx, read_count;
                read_count = len / EMMC_BLOCK_SIZE;
                for (read_idx = 0; read_idx < read_count; read_idx++) {
                    WDT_RSTCNT;
                    res = f_read(&file2, Buff, EMMC_BLOCK_SIZE, &s2);
                    if (res || s2 == 0) {
                        printf("res = %d,read size = %d\n",res,s2);
                        while(1) {   /* error or eof */
                            WDT_RSTCNT;
                        }
                    }
                    ETimer1_cnt = 0;
                    ETIMER_Start(1);
                    fmiSD_Write((offset/SD_SECTOR),EMMC_BLOCK_SIZE/SD_SECTOR,(UINT32)Buff);
                    ETIMER_Stop(1);
                    WDT_RSTCNT;
                    offset += EMMC_BLOCK_SIZE;
                }
                //Write the last part of loader that is less than EMMC_BLOCK_SIZE
                if (len % EMMC_BLOCK_SIZE) {
                    res = f_read(&file2, Buff, len - (read_count * EMMC_BLOCK_SIZE), &s2);
                    if (res || s2 == 0) {
                        printf("res = %d,read size = %d\n",res,s2);
                        while(1) {   /* error or eof */
                            WDT_RSTCNT;
                        }
                    }
                    ETimer1_cnt = 0;
                    ETIMER_Start(1);
                    fmiSD_Write((offset/SD_SECTOR),s2/SD_SECTOR,(UINT32)Buff);
                    ETIMER_Stop(1);
                    WDT_RSTCNT;
                }
            }
        }

        printf("Write [%s] to eMMC ... done\n",Ini_Writer.Loader.FileName);
        f_close(&file2);

        if (Ini_Writer.UserImage[0].user_choice == 1) {
            for (ImgNo = 0; ImgNo < ImageCnt; ImgNo++) {
                WDT_RSTCNT;
                printf("open [%s]\n", Ini_Writer.UserImage[ImgNo].FileName);
                res = f_open(&file2, Ini_Writer.UserImage[ImgNo].FileName, FA_OPEN_EXISTING | FA_READ);
                if (res)
                    printf("result = %d\n",res);
                else
                    printf("f_open [%s] ok\n", Ini_Writer.UserImage[ImgNo].FileName);

                if (1) {
                    int i, offset=0, read_count, read_idx;
                    unsigned int len;
                    len = Ini_Writer.UserImage[ImgNo].DataSize;
                    read_count = len / EMMC_BLOCK_SIZE;
                    offset = Ini_Writer.UserImage[ImgNo].address;

                    for (read_idx = 0; read_idx < read_count; read_idx++) {
                        WDT_RSTCNT;
                        res = f_read(&file2, Buff, EMMC_BLOCK_SIZE, &s2);
                        if (res || s2 == 0) {
                            printf("res = %d,read size = %d\n",res,s2);
                            while(1) {   /* error or eof */
                                WDT_RSTCNT;
                            }
                        }
                        fmiSD_Write((offset/SD_SECTOR),(EMMC_BLOCK_SIZE)/SD_SECTOR,(UINT32)Buff);
                        offset += EMMC_BLOCK_SIZE;
                    }
                    if (len % EMMC_BLOCK_SIZE) {
                        WDT_RSTCNT;
                        res = f_read(&file2, Buff, len - (read_count*EMMC_BLOCK_SIZE), &s2);
                        if (res || s2 == 0) {
                            printf("res = %d,read size = %d\n",res,s2);
                            while(1) {   /* error or eof */
                                WDT_RSTCNT;
                            }
                        }
                        ETimer1_cnt = 0;
                        ETIMER_Start(1);
                        fmiSD_Write((offset/SD_SECTOR),(s2)/SD_SECTOR,(UINT32)Buff);
                        ETIMER_Stop(1);
                    }
                }
                printf("Write [%s] to eMMC ... done\n",Ini_Writer.UserImage[ImgNo].FileName);
                f_close(&file2);
            }
        }

        if (Ini_Writer.Env.user_choice == 1) {
            WDT_RSTCNT;
            printf("open [%s]\n",Ini_Writer.Env.FileName);
            result = f_open(&file2, Ini_Writer.Env.FileName, FA_OPEN_EXISTING | FA_READ);
            if (result)
                printf("result = %d\n",result);
            else
                printf("f_open [%s] ok\n",Ini_Writer.Env.FileName);

            printf("read [%s]\n",Ini_Writer.Env.FileName);
            result = f_read(&file2, Buff, EMMC_BLOCK_SIZE, &s2);
            if (result || s2 == 0) {
                printf("result = %d,read size = %d\n",result,s2);
                //break;   /* error or eof */
            } else
                printf("[%s] size = %d\n",Ini_Writer.Env.FileName,s2);

            pENV = (char*)malloc(0x10000);
            if (!pENV) {
                printf("malloc fail [%s][%d]\n",__FUNCTION__,__LINE__);
                while(1) {
                    WDT_RSTCNT;
                }
            }
            memset((void*)pENV,0,0x10000);
            _parsing_EnvTxt(s2,pENV);
            f_close(&file2);

            //Burn to eMMC
            printf("Write Environment variable to eMMC offset [%x] ... start\n", Ini_Writer.Env.address);
            ETimer1_cnt = 0;
            ETIMER_Start(1);
            fmiSD_Write((Ini_Writer.Env.address/SD_SECTOR),(0x10000/SD_SECTOR),(UINT32)pENV);
            ETIMER_Stop(1);
            printf("Write Environment variable to eMMC ... done\n");
        }
    }

    while(1) {
        WDT_RSTCNT;
    }
}
