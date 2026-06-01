#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdint.h>
#include<fcntl.h>
#include<unistd.h>
#define Sector_Size 512
#pragma pack(push,1)
typedef struct{
    uint8_t status;
    uint8_t ChsF[3];
    uint8_t PartTypeMBR;
    uint8_t ChsL[3];
    uint32_t FirstLba;
    uint32_t sectorss;
}MBR;
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
}GPTStructure;
#pragma pack(pop)
void startParsing(const char *path);
int detection(int fd);
void ParseMBR(int fd);
void ParseGPT(int fd);
void printG(uint8_t *guid);
void printP(uint16_t *name);
void startParsing(const char *path){
    int fd=open(path,O_RDONLY);
    if(fd<0){
        perror("open()");
        return;
    }
    int type=detection(fd);
    if(type==1){
        printf("MBR Disk Detected\n");
        ParseMBR(fd);
    }
    else if(type==2){
        printf("GPT Disk Detected\n");
        ParseGPT(fd);
    }
    else{
        printf("It's neither GPT nor MBR\n");
    }
    close(fd);
}
int detection(int fd){
    unsigned char sector[Sector_Size];
    lseek(fd,0,SEEK_SET);
    if(read(fd,sector,Sector_Size)!=Sector_Size){
        perror("read()");
        return 0;
    }
    if(sector[510]!=0x55 || sector[511]!=0xAA){
        return 0;
    }
    MBR *part=(MBR*)(sector+446);
    for(int i=0;i<4;i++){
        if(part[i].PartTypeMBR==0xEE){
            return 2;
        }
    }
    return 1;
}
void ParseMBR(int fd){
    unsigned char sector[Sector_Size];
    lseek(fd,0,SEEK_SET);
    if(read(fd,sector,Sector_Size)!=Sector_Size){
        perror("read()");
        return;
    }
    MBR *part=(MBR*)(sector+446);
    for(int i=0;i<4;i++){
        if(part[i].PartTypeMBR==0x00){
            continue;
        }
        printf("===== Partition %d =====\n",i+1);
        printf("Status : %02X\n",part[i].status);
        printf("Partition Type : %02X\n",part[i].PartTypeMBR);
        printf("First LBA : %u\n",part[i].FirstLba);
        printf("Total Sectors : %u\n\n",part[i].sectorss);
    }
}
void ParseGPT(int fd){
    GPTStructure header;
    lseek(fd,Sector_Size,SEEK_SET);
    if(read(fd,&header,sizeof(GPTStructure))!=sizeof(GPTStructure)){
        perror("read()");
        return;
    }
    off_t pos=header.PartEntry*Sector_Size;
    lseek(fd,pos,SEEK_SET);
    int count=0;
    for(int i=0;i<header.NoOfPart;i++){
        unsigned char buffer[header.SizeOfPart];
        if(read(fd,buffer,header.SizeOfPart)!=header.SizeOfPart){
            perror("read()");
            return;
        }
        GPTEntry *entry=(GPTEntry*)buffer;
        int empty=1;
        for(int j=0;j<16;j++){
            if(entry->PartType[j]!=0){
                empty=0;
                break;
            }
        }
        if(empty){
            continue;
        }
        count++;
        printf("===== Partition %d =====\n",count);
        printf("Partition GUID : ");
        printG(entry->PartType);
        printf("\n");
        printf("Unique GUID : ");
        printG(entry->UniqueId);
        printf("\n");
        printf("First LBA : %llu\n",(unsigned long long)entry->start_sector);
        printf("Last LBA : %llu\n",(unsigned long long)entry->end_sector);
        printf("Partition Name : ");
        printP(entry->name);
        printf("\n\n");
    }
}
void printG(uint8_t *guid){
    for(int i=0;i<16;i++){
        printf("%02X",guid[i]);
        if(i==3||i==5||i==7||i==9){
            printf("-");
        }
    }
}
void printP(uint16_t *name){
    for(int i=0;i<36;i++){
        if(name[i]==0){
            break;
        }
        printf("%c",(char)name[i]);
    }
}
int main(){
    startParsing("mbr_disk.img");
    return 0;
}