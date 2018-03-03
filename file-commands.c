#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <protocol.h>
#include <sha256_utils.h>

#define DOWNLOAD_FILEMETA  "download-filemeta"
#define DOWNLOAD_PACKETNUM "download-packetnum"

#define UPLOAD_FILEMETA  "upload-filemeta"
#define UPLOAD_PACKETNUM "upload-packetnum"

long fileLength(FILE *fp)
{
    if (fseek(fp, 0, SEEK_END) == -1) {
        perror("Error seeking file end");
        exit(-1);
    }

    long len = ftell(fp);
    rewind(fp);

    return len;
}

uint8_t* readFile(FILE *fp, size_t *size)
{
    long len = fileLength(fp);

    uint8_t *data = malloc(len);
    size_t read = fread(data, 1, len, fp);
    if (read != len) {
        printf("Error reading file\n");
        exit(-1);
    }
    fclose(fp);

    *size = len;
    return data;
}

void writeAllOrDie(int fd, const uint8_t *data, size_t dataLen)
{
    size_t written = 0;
    ssize_t result;

    while (written < dataLen) {
        result = write(fd, data + written, dataLen - written);
        if (result == -1) {
            perror("Error writing data to file descriptor");
            exit(-1);
        }
        written += result;
    }
}

void writeHeader(const uint8_t shaSum[32], size_t fileLen, size_t numPackets)
{
    uint64_t fileLen_64, numPackets_64;
    uint8_t outBuf[49];

    // Header format:
    //  * 1 byte for TRANSFER_START
    //  * 8 bytes for file size in bytes
    //  * 8 bytes for number of packets
    //  * 32 bytes for sha256sum

    fileLen_64 = fileLen;
    numPackets_64 = numPackets;

    outBuf[0] = TRANSFER_DOWNLOAD;
    memcpy(outBuf + 1, &fileLen_64, 8);
    memcpy(outBuf + 9, &numPackets_64, 8);
    memcpy(outBuf + 17, shaSum, 32);

    //writeAllOrDie(serialfd, outBuf, 49);
    // change to write message stuff
}

int startDownload(const uint8_t *buf, size_t buflen)
{
    // Takes a file path, and writes out the header for the transfer of that file

    // n bytes for path
    char *path = malloc(buflen + 1);
    memcpy(path, buf, buflen);
    path[buflen] = '\0';

    // Read in file and generate metadata
    size_t fileLen, packetNum;
    uint8_t *fileData;

    FILE *fp = fopen(path, "r");
    // If the file isn't actually open, return an error
    if (fp == NULL) {
        free(path);
        return ERROR_FILE_IO;
    }

    fileData = readFile(fp, &fileLen);
    packetNum = fileLen / PACKET_SIZE + (fileLen % PACKET_SIZE == 0 ? 0 : 1);

    fclose(fp);

    uint8_t shaSum[32];
    //char shaStr[65];

    calculateSHA256(fileData, fileLen, shaSum);
    //sha256Str(shaStr, shaSum);

    free(fileData);

    // Create file transfer metadata files
    // if there are already metadata files then return an error code
    if (access(DOWNLOAD_FILEMETA, F_OK) == 0 || access(DOWNLOAD_PACKETNUM, F_OK) == 0)
        return ERROR_ALREADY_DOWNLOADING;

    // Open both download meta and download packet number, but if either don't open, io error
    FILE *downMeta = fopen(DOWNLOAD_FILEMETA, "w");
    if (downMeta == NULL)
        return ERROR_FILE_IO;
    FILE *downPacket = fopen(DOWNLOAD_PACKETNUM, "w");
    if (downPacket == NULL)
        return ERROR_FILE_IO;

    // if either of the files aren't already open, return an io error
    if (downMeta == NULL || downPacket == NULL)
        return ERROR_FILE_IO;

    fputs(path, downMeta);
    uint64_t nextPacket = 0;
    fwrite(&nextPacket, 8, 1, downPacket);

    fclose(downMeta);
    fclose(downPacket);

    // TODO: replace with creating a struct/buffer containing the data
    writeHeader(shaSum, fileLen, packetNum);

    return 0;
}

int startUpload(const uint8_t *buf, size_t buflen)
{
}

uint8_t fileCommand(uint8_t *buf, size_t buflen)
{
    uint8_t command = buf[0];
    uint8_t returnCode = 0;
    int (*func)(const uint8_t *, size_t);
    switch (command) {
        case START_DOWNLOAD:
            // start a file download from payload
            func = startDownload;
            break;
        case START_UPLOAD:
            // start a file upload to payload
            func = startUpload;
            break;
        case REQUEST_PACKET:
            // send a packet to CDH
            break;
        case SEND_PACKET:
            // receive a packet from CDH
            break;
        case CANCEL_UPLOAD:
            // erase upload packet data (if any)
            break;
        case CANCEL_DOWNLOAD:
            // erase download packet data (if any)
            break;
    }

    return func(buf + 1, buflen - 1);
}
