/****************************************************************************
 * @file     writer.h
 * @version  V1.00
 * $Revision: 4 $
 * $Date: 18/04/25 11:43a $
 * @brief    SpiWriter header file
 *
 * @note
 * Copyright (C) 2018 Nuvoton Technology Corp. All rights reserved.
 *****************************************************************************/

#define	MAJOR_VERSION_NUM	1
#define	MINOR_VERSION_NUM	3

#define __DEBUG__

#ifdef __DEBUG__
#define PRINTF sysprintf
#else
#define PRINTF(...)
#endif

#define TYPE_SPI_NOR		1
#define TYPE_SPI_NAND		2
#define TYPE_NAND			 	3
#define TYPE_EMMC			 	4

#define MAX_USER_IMAGE	10

#define START_BURN		0
#define BURN_PASS		1
#define BURN_FAIL		2

/* F/W update information */
//typedef struct fw_update_info_t
//{
//	unsigned short	imageNo;
//	unsigned short	imageFlag;
//	unsigned short	startBlock;
//	unsigned short	endBlock;
//	unsigned int	executeAddr;
//	unsigned int	fileLen;
//	char	imageName[32];
//} FW_UPDATE_INFO_T;

typedef struct IBR_boot_struct_t {
    unsigned int	BootCodeMarker;
    unsigned int	ExeAddr;
    unsigned int	ImageSize;
    unsigned int  Reserved;
} IBR_BOOT_STRUCT_T;

typedef struct USER_IMAGE_Info {
    char 					FileName[1024];
    int	 					StartBlock;
    int  					address;
    unsigned int	DataSize;
    int						user_choice;
} INI_USER_IMAGE_T;

typedef struct ERASE_Info {
    unsigned int EraseAll;
    unsigned int EraseStart;
    unsigned int EraseLength;
    unsigned int user_choice;
} ERASE_Info;

typedef struct USERDEF_SPI_Info {
    unsigned int PageSize;
    unsigned int SpareArea;
    unsigned int QuadReadCmd;
    unsigned int ReadStatusCmd;
    unsigned int WriteStatusCmd;
    unsigned int StatusValue;
    unsigned int DummyByte;
    unsigned int PageSizeDefined;
    unsigned int QuadCmdDefined;
} USERDEF_SPI_Info;

typedef struct EMMC_FORMAT_Info {
    unsigned int ReservedSpace;
    unsigned int PartitionNum;
    unsigned int Partition1Size;  //unit of MB
    unsigned int Partition2Size;  //unit of MB
    unsigned int Partition3Size;  //unit of MB
    unsigned int Partition4Size;  //unit of MB	
    unsigned int user_choice;
} EMMC_FORMAT_Info;

//----- Boot Code Optional Setting
typedef struct IBR_boot_optional_pairs_struct_t {
    unsigned int  address;
    unsigned int  value;
} IBR_BOOT_OPTIONAL_PAIRS_STRUCT_T;

#define IBR_BOOT_CODE_OPTIONAL_MARKER       0xAA55AA55
#define IBR_BOOT_CODE_OPTIONAL_MAX_NUMBER   63  // support maximum 63 pairs optional setting
typedef struct IBR_boot_optional_struct_t {
    unsigned int  OptionalMarker;
    unsigned int  Counter;
    IBR_BOOT_OPTIONAL_PAIRS_STRUCT_T Pairs[IBR_BOOT_CODE_OPTIONAL_MAX_NUMBER];
} IBR_BOOT_OPTIONAL_STRUCT_T;



typedef struct INI_Info {
    unsigned int Type;
    INI_USER_IMAGE_T DDR;
    INI_USER_IMAGE_T Loader;
    INI_USER_IMAGE_T Env;
    INI_USER_IMAGE_T UserImage[10];
    USERDEF_SPI_Info UserDef_SPI;
    EMMC_FORMAT_Info EMMC_Format;
    unsigned int Loader_size;
    ERASE_Info Erase;
} INI_INFO_T;

/* extern parameters */
extern unsigned int infoBuf;
extern unsigned char *pInfo;
extern unsigned int volatile g_SPI_SIZE;

int ProcessINI(char *fileName);
int ProcessOptionalINI(char *fileName);
extern unsigned int u32GpioPort_Start, u32GpioPort_Pass, u32GpioPort_Fail;
extern unsigned int u32GpioPin_Start, u32GpioPin_Pass, u32GpioPin_Fail;
extern unsigned int u32GpioLevel_Start, u32GpioLevel_Pass, u32GpioLevel_Fail;
extern unsigned int u32ImageCount, u32UserImageCount;
extern unsigned char volatile  SPI_ID[];

int usiInit(void);
int usiEraseSector(unsigned int addr, unsigned int secCount);
int usiWaitEraseReady(void);
int usiEraseAll(void);
int usiWrite(unsigned int addr, unsigned int len, unsigned char *buf);
int usiRead(unsigned int addr, unsigned int len, unsigned char *buf);
