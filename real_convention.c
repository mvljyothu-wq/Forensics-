#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>

#define sectorSize 512

#define ntfsOffset  3
#define exfatOffset 3
#define fat32Offset 82
#define fat16Offset 54
#define apfsOffset  32

#pragma pack(push, 1)

typedef struct
{
    uint8_t partitionType[16];
    uint8_t uniquePartitionGuid[16];
    uint64_t startSector;
    uint64_t endSector;
    uint64_t attributes;
    uint16_t name[36];
} GPTEntry;

typedef struct
{
    uint8_t signature[8];
    uint32_t revision;
    uint32_t headerSize;
    uint32_t headerCrc;
    uint32_t reserved;
    uint64_t currentLba;
    uint64_t backupLba;
    uint64_t firstUsableLba;
    uint64_t lastUsableLba;
    uint8_t diskGuid[16];
    uint64_t partitionEntryLba;
    uint32_t numberOfPartitions;
    uint32_t sizeOfPartitionEntry;
    uint32_t partitionArrayCrc;
} GPTHeader;

#pragma pack(pop)

void printPartitionName(const uint16_t *name)
{
    for (int characterIndex = 0;
         characterIndex < 36;
         characterIndex++)
    {
        if (name[characterIndex] == 0)
        {
            break;
        }

        printf("%c", (char)name[characterIndex]);
    }
}

void detectFileSystem(int fd, uint64_t firstLba)
{
    unsigned char sector[sectorSize];

    if (lseek(fd, firstLba * sectorSize, SEEK_SET) == -1)
    {
        perror("lseek()");
        return;
    }

    if (read(fd, sector, sectorSize) != sectorSize)
    {
        perror("read()");
        return;
    }

    if (memcmp(sector + ntfsOffset, "NTFS", 4) == 0)
    {
        printf("FileSystem   : NTFS\n");
        return;
    }

    if (memcmp(sector + exfatOffset, "EXFAT", 5) == 0)
    {
        printf("FileSystem   : EXFAT\n");
        return;
    }

    if (memcmp(sector + fat32Offset, "FAT32", 5) == 0)
    {
        printf("FileSystem   : FAT32\n");
        return;
    }

    if (memcmp(sector + fat16Offset, "FAT16", 5) == 0)
    {
        printf("FileSystem   : FAT16\n");
        return;
    }

    if (memcmp(sector + apfsOffset, "APFS", 4) == 0)
    {
        printf("FileSystem   : APFS\n");
        return;
    }

    printf("FileSystem   : Unknown\n");
}

int isEmptyPartition(const GPTEntry *entry)
{
    for (int guidByteIndex = 0;
         guidByteIndex < 16;
         guidByteIndex++)
    {
        if (entry->partitionType[guidByteIndex] != 0)
        {
            return 0;
        }
    }

    return 1;
}

int main(void)
{
    int fd = open("/dev/disk4", O_RDONLY);

    if (fd < 0)
    {
        perror("open()");
        return EXIT_FAILURE;
    }

    GPTHeader header;

    if (lseek(fd, sectorSize, SEEK_SET) == -1)
    {
        perror("lseek()");
        close(fd);
        return EXIT_FAILURE;
    }

    if (read(fd, &header, sizeof(header)) != sizeof(header))
    {
        perror("read()");
        close(fd);
        return EXIT_FAILURE;
    }

    if (memcmp(header.signature, "EFI PART", 8) != 0)
    {
        printf("GPT Signature mismatch!\n");
        printf("Read Signature : ");

        for (int signatureByteIndex = 0;
             signatureByteIndex < 8;
             signatureByteIndex++)
        {
            printf("%02X ",
                   header.signature[signatureByteIndex]);
        }

        printf("\n");

        close(fd);
        return EXIT_FAILURE;
    }

    printf("========== GPT HEADER ==========\n");
    printf("Signature             : %.8s\n",
           header.signature);

    printf("Revision              : 0x%X\n",
           header.revision);

    printf("Partition Entry LBA   : %llu\n",
           (unsigned long long)header.partitionEntryLba);

    printf("Number Of Partitions  : %u\n",
           header.numberOfPartitions);

    printf("Entry Size            : %u bytes\n\n",
           header.sizeOfPartitionEntry);

    off_t entryOffset =
        header.partitionEntryLba * sectorSize;

    if (lseek(fd, entryOffset, SEEK_SET) == -1)
    {
        perror("lseek()");
        close(fd);
        return EXIT_FAILURE;
    }

    int partitionCount = 0;

    for (uint32_t partitionIndex = 0;
         partitionIndex < header.numberOfPartitions;
         partitionIndex++)
    {
        unsigned char buffer[header.sizeOfPartitionEntry];

        if (read(fd,
                 buffer,
                 header.sizeOfPartitionEntry)
            != header.sizeOfPartitionEntry)
        {
            perror("read()");
            break;
        }

        GPTEntry *entry = (GPTEntry *)buffer;

        if (isEmptyPartition(entry))
        {
            continue;
        }

        partitionCount++;

        printf("===== Partition %d =====\n",
               partitionCount);

        printf("Name         : ");
        printPartitionName(entry->name);
        printf("\n");

        printf("Start Sector : %llu\n",
               (unsigned long long)entry->startSector);

        printf("End Sector   : %llu\n",
               (unsigned long long)entry->endSector);

        printf("Size Sectors : %llu\n",
               (unsigned long long)
               (entry->endSector -
                entry->startSector + 1));

        off_t currentPosition =
            lseek(fd, 0, SEEK_CUR);

        if (currentPosition == -1)
        {
            perror("lseek()");
            break;
        }

        detectFileSystem(fd, entry->startSector);

        if (lseek(fd,
                  currentPosition,
                  SEEK_SET) == -1)
        {
            perror("lseek()");
            break;
        }

        printf("\n");
    }

    close(fd);
    return EXIT_SUCCESS;
}