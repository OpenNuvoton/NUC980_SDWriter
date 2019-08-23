#ifndef __SDGLUE_H__
#define __SDGLUE_H__

#include "nuc980.h"
#include "sd.h"

#define SD_SECTOR 512
#define SD_MUL 8
#define MMC_INFO_SECTOR 1

#define MBR_BUFFER       0x600000
#define DOWNLOAD_BASE    0x500000 //0x10000
#define NON_CACHE        0x80000000

//#define    pSD0     (((inpw(REG_SYS_PWRON) & 0x00000300) == 0x300) ? (_pSD0):(_pSD1))
#define    pSD0     (((inpw(REG_SYS_PWRON) & 0x00000300) == 0x300) ? (_pSD1):(_pSD0))

INT  fmiInitSDDevice(void);
INT  fmiSD_Read(UINT32 uSector, UINT32 uBufcnt, UINT32 uDAddr);
INT  fmiSD_Write(UINT32 uSector, UINT32 uBufcnt, UINT32 uSAddr);


void Burn_MMC_RAW(UINT32 len, UINT32 offset,UINT8 *ptr);
void Read_MMC_RAW(UINT32 len, UINT32 offset,UINT8 *ptr);


//------------------------------------------------------------------
int ChangeMMCImageType(UINT32 imageNo, UINT32 imageType);
int SetMMCImageInfo(FW_MMC_IMAGE_T *mmcImageInfo);
UINT32 GetMMCImageInfo(unsigned int *image);
UINT32 GetMMCReserveSpace(void);
void GetMMCImage(void);
int DelMMCImage(UINT32 imageNo);
#endif
