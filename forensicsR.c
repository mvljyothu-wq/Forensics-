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
}GPTEntry;
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
}GPTHeader;
#pragma pack(pop)
void DetectFile(int fd, uint64_t FirstLBA){
    unsigned char sector[512];
    lseek(fd,FirstLBA*Sector_Size,SEEK_SET);
    if(read(fd,sector,Sector_Size)!=Sector_Size){
        perror("read()");
        return;
    }
    if(memcmp(sector+3,"NTFS",4)==0){
        printf("The File System is NTFS");
        return;
    }
    if(memcmp(sector+3,"EXFAT",5)==0){
        printf("The FIle System is EXFAT");
        return;
    }
    if(memcmp(sector+82,"FAT32",5)==0){
        printf("This FileSystem is FAT32\n");
        return;
    }
    if(memcmp(sector+54,"FAT16",5)==0){
        printf("This FileSystem is FAT16\n");
        return;
    }
    if(memcmp(sector+32,"APFS",4)==0){
        printf("FileSystem : APFS\n");
        return;
    }
}
void PrintName(uint16_t *name){
    for(int i=0;i<36;i++){
        if(name[i]==0)
            break;

        printf("%c",(char)name[i]);
    }
}
void showHelp(void){
    printf("\n Available Commands \n");
    printf(" help - To show available Commands\n");
    printf(" info - GPT Header Information\n");
    printf(" fs - About the File System\n");
    printf(" part - For Partitions\n");
}
int posToGPT(int fd,GPTHeader *Header){
    lseek(fd,Sector_Size,SEEK_SET);
    if(read(fd,Header,Sector_Size)!=Sector_Size){
        perror("read()");
        return 0;
    }
    if(memcmp(Header->signature, "EFI PART", 8) != 0)
    {
        printf("GPT Signature Mismatch\n");
        return 0;
    }
    return 1;
}
void PrintGPTHeader(int fd){
    GPTHeader Header;
    posToGPT(fd,&Header);
    if(!posToGPT(fd, &Header))
        return;
    printf("Partition Entry LBA : %llu\n",(unsigned long long)Header.PartEntry);
    printf("Number of Entries   : %u\n",Header.NoOfPart);
    printf("Entry Size          : %u bytes\n\n",Header.SizeOfPart);
    printf("Signature : %.8s\n", Header.signature);
    printf("Revision  : %x\n", Header.revision);
}
void printPartitions(int fd){
    GPTHeader Header;
    posToGPT(fd,&Header);
    if(!posToGPT(fd, &Header))
        return;
    off_t entryOff=Header.PartEntry*Sector_Size;
    int cou=0;
    lseek(fd,entryOff,SEEK_SET);
    for(uint32_t i=0;i<Header.NoOfPart;i++){
        unsigned char buffer[Header.SizeOfPart];
        if(read(fd,buffer,Header.SizeOfPart)!=Header.SizeOfPart){
            perror("read()");
            break;
        }
        GPTEntry *entry=(GPTEntry *)buffer;
        int em=1;
        for(int j=0;j<16;j++){
            if(entry->PartType[j]!=0){
                em = 0;
                break;
            }
        }
        if(em){
            continue;
        }
        cou++;
        printf("===== Partition %d =====\n",cou);
        printf("Name         : ");
        PrintName(entry->name);
        printf("\n");
        printf("Start Sector : %llu\n",
            (unsigned long long)entry->start_sector);
        printf("End Sector   : %llu\n",
            (unsigned long long)entry->end_sector);
        printf("Size Sectors : %llu\n",
            (unsigned long long)
            (entry->end_sector - entry->start_sector + 1));
    }
}
void printFileSystem(int fd){
    GPTHeader Header;
    posToGPT(fd,&Header);
    if(!posToGPT(fd, &Header))
        return;
    off_t entryOff=Header.PartEntry*Sector_Size;
    lseek(fd, entryOff, SEEK_SET);
    for(uint32_t i=0;i<Header.NoOfPart;i++){
        unsigned char buffer[Header.SizeOfPart];
        if(read(fd,buffer,Header.SizeOfPart)!=Header.SizeOfPart){
            perror("read");
            break;
        }
        GPTEntry *entry=(GPTEntry *)buffer;
        int em=1;
        for(int j=0;j<16;j++){
            if(entry->PartType[j]!=0){
                em = 0;
                break;
            }
        }
        if(em){
            continue;
        }
        PrintName(entry->name);
        printf(" : ");
        DetectFile(fd,entry->start_sector);
        printf("\n");
    }
}
int main(){
    char command[50];

    int fd=open(DISK_PATH,O_RDONLY);

    if(fd<0)
    {
        perror("open()");
        return 1;
    }

    printf("=== Mini Forensics Tool ===\n");
    
    while(1)
    {
        printf("PD> ");

        fgets(command,sizeof(command),stdin);

        command[strcspn(command,"\n")] = '\0';

        if(strcmp(command,"help")==0)
        {
            showHelp();
        }
        else if(strcmp(command,"info")==0)
        {
            PrintGPTHeader(fd);
        }
        else if(strcmp(command,"part")==0)
        {
            printPartitions(fd);
        }
        else if(strcmp(command,"fs")==0)
        {
            printFileSystem(fd);
        }
        else if(strcmp(command,"exit")==0)
        {
            break;
        }
        else
        {
            printf("Unknown command\n");
        }
    }

    close(fd);
    return 0;
}