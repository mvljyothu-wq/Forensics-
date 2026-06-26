
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdint.h>
#include<fcntl.h>
#include<unistd.h>

#define Sector_Size 512
#define DISK_PATH "/dev/disk4"

#pragma pack(push,1)

typedef struct{
    uint8_t PartType[16];
    uint8_t UniqueId[16];
    uint64_t start_sector;
    uint64_t end_sector;
    uint64_t attributes;
    uint16_t name[36];
} GPTEntry;

typedef struct{
    uint8_t signature[8];
    uint32_t revision;
    uint32_t HeaderSize;
    uint32_t HeaderCRC;
    uint32_t Reserved;
    uint64_t CurrLBA;
    uint64_t BackLBA;
    uint64_t FirstLBA;
    uint64_t LastLBA;
    uint8_t disk[16];
    uint64_t PartEntry;
    uint32_t NoOfPart;
    uint32_t SizeOfPart;
    uint32_t PartArray;
} GPTHeader;

#pragma pack(pop)

void PrintGUID(uint8_t *guid)
{
    for(int i=0;i<16;i++)
    {
        printf("%02X",guid[i]);
        if(i==3 || i==5 || i==7 || i==9)
            printf("-");
    }
}

void PrintName(uint16_t *name)
{
    for(int i=0;i<36;i++)
    {
        if(name[i]==0) break;
        printf("%c",(char)name[i]);
    }
}

double PartitionSizeGB(uint64_t start,uint64_t end)
{
    uint64_t sectors=end-start+1;
    return ((double)sectors*512.0)/(1024.0*1024.0*1024.0);
}

int posToGPT(int fd,GPTHeader *Header)
{
    lseek(fd,Sector_Size,SEEK_SET);

    if(read(fd,Header,Sector_Size)!=Sector_Size)
    {
        perror("read");
        return 0;
    }

    if(memcmp(Header->signature,"EFI PART",8)!=0)
    {
        printf("GPT Signature Mismatch\n");
        return 0;
    }

    return 1;
}

void DetectFile(int fd,uint64_t FirstLBA)
{
    unsigned char sector[512];

    lseek(fd,FirstLBA*Sector_Size,SEEK_SET);

    if(read(fd,sector,Sector_Size)!=Sector_Size)
    {
        perror("read");
        return;
    }

    if(memcmp(sector+3,"NTFS",4)==0)
        printf("NTFS");
    else if(memcmp(sector+3,"EXFAT",5)==0)
        printf("EXFAT");
    else if(memcmp(sector+82,"FAT32",5)==0)
        printf("FAT32");
    else if(memcmp(sector+54,"FAT16",5)==0)
        printf("FAT16");
    else if(memcmp(sector+32,"APFS",4)==0)
        printf("APFS");
    else
        printf("Unknown");
}

void PrintGPTHeader(int fd)
{
    GPTHeader Header;

    if(!posToGPT(fd,&Header))
        return;

    printf("\n===== GPT HEADER =====\n\n");

    printf("Signature           : %.8s\n",Header.signature);
    printf("Revision            : %X\n",Header.revision);
    printf("Header Size         : %u bytes\n",Header.HeaderSize);
    printf("Header CRC          : 0x%X\n",Header.HeaderCRC);
    printf("Current LBA         : %llu\n",(unsigned long long)Header.CurrLBA);
    printf("Backup LBA          : %llu\n",(unsigned long long)Header.BackLBA);
    printf("First Usable LBA    : %llu\n",(unsigned long long)Header.FirstLBA);
    printf("Last Usable LBA     : %llu\n",(unsigned long long)Header.LastLBA);
    printf("Partition Entry LBA : %llu\n",(unsigned long long)Header.PartEntry);
    printf("Number Of Entries   : %u\n",Header.NoOfPart);
    printf("Entry Size          : %u bytes\n",Header.SizeOfPart);

    printf("Disk GUID           : ");
    PrintGUID(Header.disk);
    printf("\n");
}

void printPartitions(int fd)
{
    GPTHeader Header;

    if(!posToGPT(fd,&Header))
        return;

    off_t entryOff=Header.PartEntry*Sector_Size;
    lseek(fd,entryOff,SEEK_SET);

    int count=0;

    for(uint32_t i=0;i<Header.NoOfPart;i++)
    {
        unsigned char buffer[Header.SizeOfPart];

        if(read(fd,buffer,Header.SizeOfPart)!=Header.SizeOfPart)
            break;

        GPTEntry *entry=(GPTEntry*)buffer;

        int empty=1;

        for(int j=0;j<16;j++)
        {
            if(entry->PartType[j]!=0)
            {
                empty=0;
                break;
            }
        }

        if(empty) continue;

        count++;

        printf("\n===== Partition %d =====\n",count);

        printf("Name           : ");
        PrintName(entry->name);
        printf("\n");

        printf("Start Sector   : %llu\n",
        (unsigned long long)entry->start_sector);

        printf("End Sector     : %llu\n",
        (unsigned long long)entry->end_sector);

        printf("Partition GUID : ");
        PrintGUID(entry->UniqueId);
        printf("\n");

        printf("Type GUID      : ");
        PrintGUID(entry->PartType);
        printf("\n");

        printf("Size           : %.2f GB\n",
        PartitionSizeGB(entry->start_sector,
                        entry->end_sector));
    }
}

void printFileSystem(int fd)
{
    GPTHeader Header;

    if(!posToGPT(fd,&Header))
        return;

    off_t entryOff=Header.PartEntry*Sector_Size;
    lseek(fd,entryOff,SEEK_SET);

    for(uint32_t i=0;i<Header.NoOfPart;i++)
    {
        unsigned char buffer[Header.SizeOfPart];

        if(read(fd,buffer,Header.SizeOfPart)!=Header.SizeOfPart)
            break;

        GPTEntry *entry=(GPTEntry*)buffer;

        int empty=1;

        for(int j=0;j<16;j++)
        {
            if(entry->PartType[j]!=0)
            {
                empty=0;
                break;
            }
        }

        if(empty) continue;

        PrintName(entry->name);
        printf(" : ");
        DetectFile(fd,entry->start_sector);
        printf("\n");
    }
}

void printDiskInfo(int fd)
{
    off_t size=lseek(fd,0,SEEK_END);

    if(size<0)
    {
        perror("lseek");
        return;
    }

    printf("\n===== DISK INFO =====\n");
    printf("Disk Path     : %s\n",DISK_PATH);
    printf("Sector Size   : %d bytes\n",Sector_Size);
    printf("Disk Size     : %.2f GB\n",
           (double)size/(1024.0*1024.0*1024.0));
    printf("Total Sectors : %llu\n",
           (unsigned long long)(size/Sector_Size));
}

void HexDump(int fd,uint64_t sectorNo)
{
    unsigned char buffer[512];

    lseek(fd,sectorNo*Sector_Size,SEEK_SET);

    if(read(fd,buffer,Sector_Size)!=Sector_Size)
    {
        perror("read");
        return;
    }

    printf("\n===== HEX DUMP =====\n");

    for(int i=0;i<512;i++)
    {
        if(i%16==0)
            printf("\n%04X : ",i);

        printf("%02X ",buffer[i]);
    }

    printf("\n");
}

void showHelp(void)
{
    printf("\n===== MINI FORENSICS TOOL =====\n\n");
    printf("help  - Show Commands\n");
    printf("info  - GPT Header Information\n");
    printf("part  - Partition Information\n");
    printf("fs    - File System Detection\n");
    printf("disk  - Disk Information\n");
    printf("hex   - Hex Dump Sector\n");
    printf("exit  - Quit Program\n");
}

int main()
{
    char command[50];

    int fd=open(DISK_PATH,O_RDONLY);

    if(fd<0)
    {
        perror("open");
        return 1;
    }

    printf("=== Mini Forensics Tool ===\n");

    while(1)
    {
        printf("PD> ");

        fgets(command,sizeof(command),stdin);
        command[strcspn(command,"\n")]='\0';

        if(strcmp(command,"help")==0)
            showHelp();

        else if(strcmp(command,"info")==0)
            PrintGPTHeader(fd);

        else if(strcmp(command,"part")==0)
            printPartitions(fd);

        else if(strcmp(command,"fs")==0)
            printFileSystem(fd);

        else if(strcmp(command,"disk")==0)
            printDiskInfo(fd);

        else if(strcmp(command,"hex")==0)
        {
            uint64_t sector;

            printf("Enter Sector Number : ");
            scanf("%llu",&sector);
            getchar();

            HexDump(fd,sector);
        }

        else if(strcmp(command,"exit")==0)
            break;

        else
            printf("Unknown command\n");
    }

    close(fd);
    return 0;
}
