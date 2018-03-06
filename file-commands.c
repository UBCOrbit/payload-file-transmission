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
        // TODO: proper error handling/return code
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
        // TODO: proper error handling/return code
        printf("Error reading file\n");
        exit(-1);
    }
    fclose(fp);

    *size = len;
    return data;
}

struct reply startDownload(const uint8_t *buf, size_t buflen)
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
        return EMPTY_REPLY(ERROR_FILE_IO);
    }

    fileData = readFile(fp, &fileLen);
    fclose(fp);

    packetNum = fileLen / PACKET_SIZE + (fileLen % PACKET_SIZE == 0 ? 0 : 1);

    uint8_t shaSum[32];
    //char shaStr[65];

    calculateSHA256(fileData, fileLen, shaSum);
    //sha256Str(shaStr, shaSum);

    free(fileData);

    // Create file transfer metadata files
    // if there are already metadata files then return an error code
    if (access(DOWNLOAD_FILEMETA, F_OK) == 0 || access(DOWNLOAD_PACKETNUM, F_OK) == 0)
        return EMPTY_REPLY(ERROR_ALREADY_DOWNLOADING);


    // Open both download meta and download packet number, but if either don't open, io error
    FILE *downMeta = fopen(DOWNLOAD_FILEMETA, "w");
    if (downMeta == NULL)
        return EMPTY_REPLY(ERROR_FILE_IO);
    FILE *downPacket = fopen(DOWNLOAD_PACKETNUM, "w");
    if (downPacket == NULL) {
        fclose(downMeta);
        return EMPTY_REPLY(ERROR_FILE_IO);
    }

    fputs(path, downMeta);
    uint64_t nextPacket = 0;
    fwrite(&nextPacket, 8, 1, downPacket);

    fclose(downMeta);
    fclose(downPacket);

    // create and return the formatted reply
    // Download success reply format
    //   8 bytes for file length
    //   8 bytes for packet count
    //   32 bytes for sha256sum
    uint64_t fileLen_64 = fileLen;
    uint64_t numPackets_64 = packetNum;

    struct reply r;
    r.status = SUCCESS;
    r.payloadLen = 48;
    r.payload = malloc(48);

    memcpy(r.payload + 0, &fileLen_64, 8);
    memcpy(r.payload + 8, &numPackets_64, 8);
    memcpy(r.payload + 16, shaSum, 32);

    return r;
}

struct reply startUpload(const uint8_t *buf, size_t buflen)
{
    // Upload payload format
    //   8 bytes for file length
    //   8 bytes for packet count
    //   32 bytes for sha256sum
    return EMPTY_REPLY(SUCCESS);
}

void fileCommand(uint8_t *buf, size_t buflen)
{
    uint8_t command = buf[0];
    struct reply (*func)(const uint8_t *, size_t);
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

    struct reply r = func(buf + 1, buflen - 1);
    // TODO: send the generated reply
    if (r.payloadLen > 0)
        free(r.payload);
}
