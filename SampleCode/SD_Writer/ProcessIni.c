/****************************************************************************
 * @file     ProcessIni.c
 * @version  V1.00
 * $Revision: 4 $
 * $Date: 18/04/25 11:43a $
 * @brief    Process config file for SD writer
 *
 * @note
 * Copyright (C) 2018 Nuvoton Technology Corp. All rights reserved.
 *****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "writer.h"

#include "ff.h"
#include "diskio.h"

int ProcessINI(char *fileName);

#if 1
#define dbgprintf sysprintf
#else
#define dbgprintf(...)
#endif

#define    INI_BUF_SIZE     512
char iniBuf[INI_BUF_SIZE];
int  buffer_current = 0, buffer_end = 0;    /* position to buffer iniBuf */

INI_INFO_T Ini_Writer;
unsigned int u32ImageCount = 0;
unsigned int u32UserImageCount = 0;
/*-----------------------------------------------------------------------------
 * To read one string line from file FileHandle and save it to Cmd.
 * Return:
 *      Successful  : OK
 *      < 0 : error code of FAT, include ERR_FILE_EOF
 *---------------------------------------------------------------------------*/
int readLine (FIL *FileObject, char *Cmd)
{
    int i_cmd;
    unsigned int nReadLen;
    FRESULT     res;


    i_cmd = 0;

    while (1) {
        /* parse INI file in buffer iniBuf[] that read in at previous fsReadFile(). */
        while(buffer_current < buffer_end) {
            if (iniBuf[buffer_current] == 0x0D) {
                /*
                   DOS   use 0x0D 0x0A as end of line;
                   Linux use 0x0A as end of line;
                   To support both DOS and Linux, we treat 0x0A as real end of line and ignore 0x0D.
                */
                buffer_current++;
                continue;
            } else if (iniBuf[buffer_current] == 0x0A) { /* Found end of line */
                Cmd[i_cmd] = 0;     /* end of string */
                buffer_current++;
                return 0;//Successful;
            } else {
                Cmd[i_cmd] = iniBuf[buffer_current];
                buffer_current++;
                i_cmd++;
            }
        }

        /* buffer iniBuf[] is empty here. Try to read more data from file to buffer iniBuf[]. */
        /* no more data to read since previous fsReadFile() cannot read buffer full */
        if ((buffer_end < INI_BUF_SIZE) && (buffer_end > 0)) {
            if (i_cmd > 0) {
                /* return last line of INI file that without end of line */
                Cmd[i_cmd] = 0;     /* end of string */
                return 0;//Successful;
            } else {
                Cmd[i_cmd] = 0;     /* end of string to clear Cmd */
                return -1;//ERR_FILE_EOF;
            }
        } else {
            res = f_read(FileObject, iniBuf, INI_BUF_SIZE, &nReadLen);
            if (res || nReadLen == 0) break;   /* error or eof */
            //status = fsReadFile(FileHandle, (unsigned char *)iniBuf, INI_BUF_SIZE, &nReadLen);
            if (res) {
                Cmd[i_cmd] = 0;     /* end of string to clear Cmd */
                return res;      /* error or end of file */
            }
            buffer_current = 0;
            buffer_end = nReadLen;
        }
    }   /* end of while (1) */
    return 0;
}

int Convert(char *Data)
{
    int i = 0,j = 0;
    int value = 0;
    for(i=strlen(Data) -1 ; i>=0; i--) {
        if(Data[i] >= '0' && Data[i] <='9')
            value = value + (Data[i] - '0') * (1 << (4 * j) );
        else if(Data[i] >= 'A' && Data[i] <='F')
            value = value + (Data[i] - 'A' + 10) * (1 << (4 * j) );
        else if(Data[i] >= 'a' && Data[i] <='f')
            value = value + (Data[i] - 'a' + 10) * (1 << (4 * j) );
        j++;
    }
    return value;
}

FIL File_Obj;        /* File objects */

/*-----------------------------------------------------------------------------
 * To parse INI file and store configuration to global variable Ini_Writer.
 * Return:
 *      Successful  : OK
 *      < 0 : error code of FAT
 *---------------------------------------------------------------------------*/
int ProcessINI(char *fileName)
{
    char szFatFullName[64], suNvtFullName[512], Cmd[512],tmp[512],filename[512],address[256],block[256];
    int status, i;
    FRESULT     res;

    /* initial default value */
    Ini_Writer.Loader.FileName[0] = 0;
    Ini_Writer.ChipErase = 0;

    for(i=0; i<MAX_USER_IMAGE; i++) {
        Ini_Writer.UserImage[i].FileName[0] = 0;
        Ini_Writer.UserImage[i].address = 0;
    }
    /* open INI file */
    strcpy(szFatFullName, fileName);
    res = f_open(&File_Obj, fileName, FA_OPEN_EXISTING | FA_READ);
    printf("\n");
    if (res) {
        printf("res = %d\n",res);
        return res;  /* open file error */
    }

    printf("**Process %s file**\n", fileName);
    /* parse INI file */
    do {
        status = readLine(&File_Obj, Cmd);
        if (status < 0)     /* read file error. Coulde be end of file. */
            break;
        printf("Cmd = %s\n",Cmd);
NextMark2:
        if (strcmp(Cmd, "[TYPE]") == 0) {
            do {
                status = readLine(&File_Obj, Cmd);
                if (status < 0)
                    break;          /* use default value since error code from FAT. Coulde be end of file. */
                else if (Cmd[0] == 0)
                    continue;       /* skip empty line */
                else if ((Cmd[0] == '/') && (Cmd[1] == '/'))
                    continue;       /* skip comment line */
                else if (Cmd[0] == '[')
                    goto NextMark2; /* use default value since no assign value before next keyword */
                else {
                    Ini_Writer.Type = atoi(Cmd);
                    printf(" Type is %d\n",Ini_Writer.Type);
                    break;
                }
            } while (1);
        }
        if (strcmp(Cmd, "[DDR]") == 0) {
            do {
                status = readLine(&File_Obj, Cmd);
                if (status < 0)
                    break;          /* use default value since error code from FAT. Coulde be end of file. */
                else if (Cmd[0] == 0)
                    continue;       /* skip empty line */
                else if ((Cmd[0] == '/') && (Cmd[1] == '/'))
                    continue;       /* skip comment line */
                else if (Cmd[0] == '[')
                    goto NextMark2; /* use default value since no assign value before next keyword */
                else {
                    sscanf (Cmd,"%[^,]",filename);
                    strcpy(Ini_Writer.DDR.FileName, filename);
                    break;
                }
            } while (1);
        }
        if (strcmp(Cmd, "[ENV]") == 0) {
            do {
                status = readLine(&File_Obj, Cmd);
                if (status < 0)
                    break;          /* use default value since error code from FAT. Coulde be end of file. */
                else if (Cmd[0] == 0)
                    continue;       /* skip empty line */
                else if ((Cmd[0] == '/') && (Cmd[1] == '/'))
                    continue;       /* skip comment line */
                else if (Cmd[0] == '[')
                    goto NextMark2; /* use default value since no assign value before next keyword */
                else {
                    sscanf (Cmd,"%[^,], 0x%s",filename, address);
                    strcpy(Ini_Writer.Env.FileName, filename);
                    Ini_Writer.Env.address = Convert(address);
                    Ini_Writer.Env.user_choice = 1;

                    u32ImageCount++;
                    break;
                }
            } while (1);
        }
        if (strcmp(Cmd, "[Loader]") == 0) {
            do {
                status = readLine(&File_Obj, Cmd);
                if (status < 0)
                    break;          /* use default value since error code from FAT. Coulde be end of file. */
                else if (Cmd[0] == 0)
                    continue;       /* skip empty line */
                else if ((Cmd[0] == '/') && (Cmd[1] == '/'))
                    continue;       /* skip comment line */
                else if (Cmd[0] == '[')
                    goto NextMark2; /* use default value since no assign value before next keyword */
                else {
                    sscanf (Cmd,"%[^,], 0x%s",filename, address);
                    strcpy(Ini_Writer.Loader.FileName, filename);
                    Ini_Writer.Loader.address = Convert(address);
                    Ini_Writer.Loader.user_choice = 1;
                    u32ImageCount++;
                    break;
                }
            } while (1);
        } else if (strstr(Cmd,"[Data0]")) {
            do {
                status = readLine(&File_Obj, Cmd);
                if (status < 0)
                    break;          /* use default value since error code from FAT. Coulde be end of file. */
                else if (Cmd[0] == 0)
                    continue;       /* skip empty line */
                else if ((Cmd[0] == '/') && (Cmd[1] == '/'))
                    continue;       /* skip comment line */
                else if (Cmd[0] == '[')
                    goto NextMark2; /* use default value since no assign value before next keyword */
                else {
                    sscanf (Cmd,"%[^,], 0x%s",filename, block);
                    strcpy(Ini_Writer.UserImage[u32UserImageCount].FileName, filename);
                    Ini_Writer.UserImage[u32UserImageCount].address = Convert(block);
                    Ini_Writer.UserImage[u32UserImageCount].user_choice = 1;
                    u32ImageCount++;
                    u32UserImageCount++;
                    break;
                }
            } while (1);
        } else if (strstr(Cmd,"[Data1]")) {
            do {
                status = readLine(&File_Obj, Cmd);
                if (status < 0)
                    break;          /* use default value since error code from FAT. Coulde be end of file. */
                else if (Cmd[0] == 0)
                    continue;       /* skip empty line */
                else if ((Cmd[0] == '/') && (Cmd[1] == '/'))
                    continue;       /* skip comment line */
                else if (Cmd[0] == '[')
                    goto NextMark2; /* use default value since no assign value before next keyword */
                else {
                    sscanf (Cmd,"%[^,], 0x%s",filename, block);
                    strcpy(Ini_Writer.UserImage[u32UserImageCount].FileName, filename);
                    Ini_Writer.UserImage[u32UserImageCount].address = Convert(block);
                    Ini_Writer.UserImage[u32UserImageCount].user_choice = 1;
                    u32ImageCount++;
                    u32UserImageCount++;
                    break;
                }
            } while (1);
        } else if (strstr(Cmd,"[Data2]")) {
            do {
                status = readLine(&File_Obj, Cmd);
                if (status < 0)
                    break;          /* use default value since error code from FAT. Coulde be end of file. */
                else if (Cmd[0] == 0)
                    continue;       /* skip empty line */
                else if ((Cmd[0] == '/') && (Cmd[1] == '/'))
                    continue;       /* skip comment line */
                else if (Cmd[0] == '[')
                    goto NextMark2; /* use default value since no assign value before next keyword */
                else {
                    sscanf (Cmd,"%[^,], 0x%s",filename, block);
                    strcpy(Ini_Writer.UserImage[u32UserImageCount].FileName, filename);
                    Ini_Writer.UserImage[u32UserImageCount].address = Convert(block);
                    Ini_Writer.UserImage[u32UserImageCount].user_choice = 1;
                    u32ImageCount++;
                    u32UserImageCount++;
                    break;
                }
            } while (1);
        } else if (strstr(Cmd,"[Data3]")) {
            do {
                status = readLine(&File_Obj, Cmd);
                if (status < 0)
                    break;          /* use default value since error code from FAT. Coulde be end of file. */
                else if (Cmd[0] == 0)
                    continue;       /* skip empty line */
                else if ((Cmd[0] == '/') && (Cmd[1] == '/'))
                    continue;       /* skip comment line */
                else if (Cmd[0] == '[')
                    goto NextMark2; /* use default value since no assign value before next keyword */
                else {
                    sscanf (Cmd,"%[^,], 0x%s",filename, block);
                    strcpy(Ini_Writer.UserImage[u32UserImageCount].FileName, filename);
                    Ini_Writer.UserImage[u32UserImageCount].address = Convert(block);
                    Ini_Writer.UserImage[u32UserImageCount].user_choice = 1;
                    u32ImageCount++;
                    u32UserImageCount++;
                    break;
                }
            } while (1);
        } else if (strstr(Cmd,"[Data4]")) {
            do {
                status = readLine(&File_Obj, Cmd);
                if (status < 0)
                    break;          /* use default value since error code from FAT. Coulde be end of file. */
                else if (Cmd[0] == 0)
                    continue;       /* skip empty line */
                else if ((Cmd[0] == '/') && (Cmd[1] == '/'))
                    continue;       /* skip comment line */
                else if (Cmd[0] == '[')
                    goto NextMark2; /* use default value since no assign value before next keyword */
                else {
                    sscanf (Cmd,"%[^,], 0x%s",filename, block);
                    strcpy(Ini_Writer.UserImage[u32UserImageCount].FileName, filename);
                    Ini_Writer.UserImage[u32UserImageCount].address = Convert(block);
                    Ini_Writer.UserImage[u32UserImageCount].user_choice = 1;
                    u32ImageCount++;
                    u32UserImageCount++;
                    break;
                }
            } while (1);
        } else if (strstr(Cmd,"[Data5]")) {
            do {
                status = readLine(&File_Obj, Cmd);
                if (status < 0)
                    break;          /* use default value since error code from FAT. Coulde be end of file. */
                else if (Cmd[0] == 0)
                    continue;       /* skip empty line */
                else if ((Cmd[0] == '/') && (Cmd[1] == '/'))
                    continue;       /* skip comment line */
                else if (Cmd[0] == '[')
                    goto NextMark2; /* use default value since no assign value before next keyword */
                else {
                    sscanf (Cmd,"%[^,], 0x%s",filename, block);
                    strcpy(Ini_Writer.UserImage[u32UserImageCount].FileName, filename);
                    Ini_Writer.UserImage[u32UserImageCount].address = Convert(block);
                    Ini_Writer.UserImage[u32UserImageCount].user_choice = 1;
                    u32ImageCount++;
                    u32UserImageCount++;
                    break;
                }
            } while (1);
        } else if (strstr(Cmd,"[Data6]")) {
            do {
                status = readLine(&File_Obj, Cmd);
                if (status < 0)
                    break;          /* use default value since error code from FAT. Coulde be end of file. */
                else if (Cmd[0] == 0)
                    continue;       /* skip empty line */
                else if ((Cmd[0] == '/') && (Cmd[1] == '/'))
                    continue;       /* skip comment line */
                else if (Cmd[0] == '[')
                    goto NextMark2; /* use default value since no assign value before next keyword */
                else {
                    sscanf (Cmd,"%[^,], 0x%s",filename, block);
                    strcpy(Ini_Writer.UserImage[u32UserImageCount].FileName, filename);
                    Ini_Writer.UserImage[u32UserImageCount].address = Convert(block);
                    Ini_Writer.UserImage[u32UserImageCount].user_choice = 1;
                    u32ImageCount++;
                    u32UserImageCount++;
                    break;
                }
            } while (1);
        } else if (strstr(Cmd,"[Data7]")) {
            do {
                status = readLine(&File_Obj, Cmd);
                if (status < 0)
                    break;          /* use default value since error code from FAT. Coulde be end of file. */
                else if (Cmd[0] == 0)
                    continue;       /* skip empty line */
                else if ((Cmd[0] == '/') && (Cmd[1] == '/'))
                    continue;       /* skip comment line */
                else if (Cmd[0] == '[')
                    goto NextMark2; /* use default value since no assign value before next keyword */
                else {
                    sscanf (Cmd,"%[^,], 0x%s",filename, block);
                    strcpy(Ini_Writer.UserImage[u32UserImageCount].FileName, filename);
                    Ini_Writer.UserImage[u32UserImageCount].address = Convert(block);
                    Ini_Writer.UserImage[u32UserImageCount].user_choice = 1;
                    u32ImageCount++;
                    u32UserImageCount++;
                    break;
                }
            } while (1);
        } else if (strstr(Cmd,"[Data8]")) {
            do {
                status = readLine(&File_Obj, Cmd);
                if (status < 0)
                    break;          /* use default value since error code from FAT. Coulde be end of file. */
                else if (Cmd[0] == 0)
                    continue;       /* skip empty line */
                else if ((Cmd[0] == '/') && (Cmd[1] == '/'))
                    continue;       /* skip comment line */
                else if (Cmd[0] == '[')
                    goto NextMark2; /* use default value since no assign value before next keyword */
                else {
                    sscanf (Cmd,"%[^,], 0x%s",filename, block);
                    strcpy(Ini_Writer.UserImage[u32UserImageCount].FileName, filename);
                    Ini_Writer.UserImage[u32UserImageCount].address = Convert(block);
                    Ini_Writer.UserImage[u32UserImageCount].user_choice = 1;
                    u32ImageCount++;
                    u32UserImageCount++;
                    break;
                }
            } while (1);
        } else if (strstr(Cmd,"[Data9]")) {
            do {
                status = readLine(&File_Obj, Cmd);
                if (status < 0)
                    break;          /* use default value since error code from FAT. Coulde be end of file. */
                else if (Cmd[0] == 0)
                    continue;       /* skip empty line */
                else if ((Cmd[0] == '/') && (Cmd[1] == '/'))
                    continue;       /* skip comment line */
                else if (Cmd[0] == '[')
                    goto NextMark2; /* use default value since no assign value before next keyword */
                else {
                    sscanf (Cmd,"%[^,], 0x%s",filename, block);
                    strcpy(Ini_Writer.UserImage[u32UserImageCount].FileName, filename);
                    Ini_Writer.UserImage[u32UserImageCount].address = Convert(block);
                    Ini_Writer.UserImage[u32UserImageCount].user_choice = 1;
                    u32ImageCount++;
                    u32UserImageCount++;
                    break;
                }
            } while (1);
        } else if (strstr(Cmd,"[UserDefine]")) {
            do {
                status = readLine(&File_Obj, Cmd);
                if (status < 0)
                    break;          /* use default value since error code from FAT. Coulde be end of file. */
                else if (Cmd[0] == 0)
                    continue;       /* skip empty line */
                else if ((Cmd[0] == '/') && (Cmd[1] == '/'))
                    continue;       /* skip comment line */
                else if (Cmd[0] == '[')
                    goto NextMark2; /* use default value since no assign value before next keyword */
                else {
                    if (sscanf (Cmd,"PageSize=%d, SpareArea=%d",&(Ini_Writer.UserDef_SPI.PageSize), &(Ini_Writer.UserDef_SPI.SpareArea)) == 2)
                        Ini_Writer.UserDef_SPI.PageSizeDefined = 1;
                    if (sscanf (Cmd,"QuadReadCmd=0x%x, ReadStatusCmd=0x%x, WriteStatusCmd=0x%x, StatusValue=0x%x, DummyByte=0x%x",&(Ini_Writer.UserDef_SPI.QuadReadCmd), &(Ini_Writer.UserDef_SPI.ReadStatusCmd), &(Ini_Writer.UserDef_SPI.WriteStatusCmd), &(Ini_Writer.UserDef_SPI.StatusValue), &(Ini_Writer.UserDef_SPI.DummyByte)) == 5)
                        Ini_Writer.UserDef_SPI.QuadCmdDefined = 1;

                    break;
                }
            } while (1);
        } else if (strstr(Cmd,"[Format]")) {
            do {
                status = readLine(&File_Obj, Cmd);
                if (status < 0)
                    break;          /* use default value since error code from FAT. Coulde be end of file. */
                else if (Cmd[0] == 0)
                    continue;       /* skip empty line */
                else if ((Cmd[0] == '/') && (Cmd[1] == '/'))
                    continue;       /* skip comment line */
                else if (Cmd[0] == '[')
                    goto NextMark2; /* use default value since no assign value before next keyword */
                else {
                    if (sscanf (Cmd,"ReservedSpace=%d, PartitionNum=%d, PartitionS1Size=%d, PartitionS2Size=%d, PartitionS3Size=%d, PartitionS4Size=%d",&(Ini_Writer.EMMC_Format.ReservedSpace), &(Ini_Writer.EMMC_Format.PartitionNum), &(Ini_Writer.EMMC_Format.Partition1Size), &(Ini_Writer.EMMC_Format.Partition2Size), &(Ini_Writer.EMMC_Format.Partition3Size), &(Ini_Writer.EMMC_Format.Partition4Size)) == 6)
                        Ini_Writer.EMMC_Format.user_choice = 1;

                    break;
                }
            } while (1);
        } else if (strcmp(Cmd, "[Chip Erase]") == 0) {
            do {
                status = readLine(&File_Obj, Cmd);
                if (status < 0)
                    break;          /* use default value since error code from FAT. Coulde be end of file. */
                else if (Cmd[0] == 0)
                    continue;       /* skip empty line */
                else if ((Cmd[0] == '/') && (Cmd[1] == '/'))
                    continue;       /* skip comment line */
                else if (Cmd[0] == '[')
                    goto NextMark2; /* use default value since no assign value before next keyword */
                else {
                    Ini_Writer.ChipErase = atoi(Cmd);
                    printf(" Chip Erase is ");
                    Ini_Writer.ChipErase?printf("enable\n"): printf("disable\n");
                    break;
                }
            } while (1);
        }
    } while (status >= 0);  /* keep parsing INI file */

    f_close(&File_Obj);
    /* show final configuration */
    printf(" Loader = %s, address 0x%X\n",Ini_Writer.Loader.FileName,Ini_Writer.Loader.address);
    printf("PageSize=%d, SpareArea=%d\n",Ini_Writer.UserDef_SPI.PageSize, Ini_Writer.UserDef_SPI.SpareArea);
    printf("QuadReadCmd=0x%x, ReadStatusCmd=0x%x, WriteStatusCmd=0x%x, StatusValue=0x%x, DummyByte=0x%x",Ini_Writer.UserDef_SPI.QuadReadCmd, Ini_Writer.UserDef_SPI.ReadStatusCmd, Ini_Writer.UserDef_SPI.WriteStatusCmd, Ini_Writer.UserDef_SPI.StatusValue, Ini_Writer.UserDef_SPI.DummyByte);


    if(u32UserImageCount != 0) {
        printf(" User Image Count is %d\n",u32UserImageCount);
        for(i=0; i<u32UserImageCount; i++)
            printf("   User image name:%s, start block 0x%X\n",Ini_Writer.UserImage[i].FileName,Ini_Writer.UserImage[i].StartBlock);
    }

    if(u32ImageCount > 21)
        return -1;//Fail;

    return 0;//Successful;
}
