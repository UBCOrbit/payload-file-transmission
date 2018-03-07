#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <protocol.h>
#include <sha256_utils.h>

#define DOWNLOAD_FILEPATH   "download-filepath"
#define DOWNLOAD_FILEOFFSET "download-fileoffset"

#define UPLOAD_FILEMETA  "upload-filemeta"
#define UPLOAD_RECEIVED  "upload-received"

// TODO: More specific io error reporting

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

// Create data for sending a file and return file shasum
Reply startDownload(const uint8_t *buf, size_t buflen)
{
    // download payload format
    //   n bytes for path

    // if there are already download metadata files then return an error code
    if (access(DOWNLOAD_FILEPATH, F_OK) == 0 || access(DOWNLOAD_FILEOFFSET, F_OK) == 0)
        return EMPTY_REPLY(ERROR_ALREADY_DOWNLOADING);

    // Read file path from buffer
    char *path = malloc(buflen + 1);
    memcpy(path, buf, buflen);
    path[buflen] = '\0';

    // check that the requested file exists
    if (access(path, F_OK) != 0)
        return EMPTY_REPLY(ERROR_FILE_IO);

    // Read in file and generate metadata
    FILE *fp = fopen(path, "r");
    // If the file wasn't opened return an error
    if (fp == NULL) {
        free(path);
        return EMPTY_REPLY(ERROR_FILE_IO);
    }

    size_t fileLen;
    uint8_t *fileData;

    fileData = readFile(fp, &fileLen);
    fclose(fp);

    uint8_t shaSum[32];
    sha256calc(fileData, fileLen, shaSum);

    free(fileData);

    // Create file transfer metadata files
    // Open both download meta and download packet number, but if either don't open, io error
    FILE *downMeta, *downOffset;

    downMeta = fopen(DOWNLOAD_FILEPATH, "w");
    if (downMeta == NULL)
        return EMPTY_REPLY(ERROR_FILE_IO);

    downOffset = fopen(DOWNLOAD_FILEOFFSET, "w");
    if (downOffset == NULL) {
        fclose(downMeta);
        return EMPTY_REPLY(ERROR_FILE_IO);
    }

    // write path to downMeta and zero to downPacket
    // TODO: fwrite error checking?
    fputs(path, downMeta);
    uint64_t offset = 0;
    fwrite(&offset, 8, 1, downOffset);

    fclose(downMeta);
    fclose(downOffset);

    // create and return the formatted reply
    // Download success reply format
    //   32 bytes for sha256sum

    Reply r;
    r.status = SUCCESS;
    r.payloadLen = 32;
    r.payload = malloc(32);

    memcpy(r.payload + 32, shaSum, 32);

    return r;
}

// Create data for receiving a file
Reply startUpload(const uint8_t *buf, size_t buflen)
{
    // Upload payload format
    //   32 bytes for sha256sum

    // If any of the upload files exist, return already uploading
    if (access(UPLOAD_FILEMETA, F_OK) == 0 || access(UPLOAD_RECEIVED, F_OK) == 0)
        return EMPTY_REPLY(ERROR_ALREADY_UPLOADING);

    // Open upload meta, upload packet number, and received data file, if either don't return io error
    FILE *upMeta, *upReceived;

    upMeta = fopen(UPLOAD_FILEMETA, "w");
    if (upMeta == NULL)
        return EMPTY_REPLY(ERROR_FILE_IO);

    upReceived = fopen(UPLOAD_RECEIVED, "w");
    if (upReceived == NULL) {
        fclose(upMeta);
        return EMPTY_REPLY(ERROR_FILE_IO);
    }

    // write shasum to the upload meta file
    // TODO: fwrite error checking
    fwrite(buf, 32, 1, upMeta);

    fclose(upMeta);
    fclose(upReceived);

    return EMPTY_REPLY(SUCCESS);
}

// Send a packet to CDH
Reply requestPacket(const uint8_t *buf, size_t buflen)
{
    // check if all the download metadata files exist
    if (access(DOWNLOAD_FILEPATH, F_OK) != 0 || access(DOWNLOAD_FILEOFFSET, F_OK) != 0)
        return EMPTY_REPLY(ERROR_NOT_DOWNLOADING);

    // Open both filepath and fileoffset, and verify that they're open
    FILE *downMeta, *downOffset;

    downMeta = fopen(DOWNLOAD_FILEPATH, "r");
    if (downMeta == NULL)
        return EMPTY_REPLY(ERROR_FILE_IO);

    downOffset = fopen(DOWNLOAD_FILEOFFSET, "r+");
    if (downOffset == NULL) {
        fclose(downMeta);
        return EMPTY_REPLY(ERROR_FILE_IO);
    }

    // Read path from filepath and offset from fileoffset
    // TODO: fread error checking
    long pathlen = fileLength(downMeta);
    char *path = malloc(pathlen);
    fread(path, 1, pathlen, downMeta);

    uint64_t offset;
    fread(&offset, 8, 1, downOffset);

    fclose(downMeta);

    // Do a sanity check that the file exists
    if (access(path, F_OK) != 0)
        return EMPTY_REPLY(ERROR_FILE_DOESNT_EXIST);

    // Open the download file
    // If it wasn't opened return a file io error
    FILE *downFile = fopen(path, "r");
    if (downFile == NULL) {
        free(path);
        return EMPTY_REPLY(ERROR_FILE_IO);
    }

    free(path);

    // Check that the offset is less than the file length
    long filelen = fileLength(downFile);
    if (filelen <= offset) {
        fclose(downOffset);
        fclose(downFile);
        return EMPTY_REPLY(ERROR_DOWNLOAD_OVER);
    }

    // calculate the packet length
    uint16_t packetlen = filelen - offset > PACKET_SIZE ? PACKET_SIZE : filelen - offset;

    Reply r;
    r.status = SUCCESS;
    r.payloadLen = packetlen;
    r.payload = malloc(packetlen);

    // Read the next packet data from the file
    // TODO: fseek, fread error checking
    fseek(downFile, offset, SEEK_SET);
    fread(r.payload, 1, packetlen, downFile);

    // Update the offset file
    // TODO: fwrite error checking
    offset += packetlen;
    rewind(downOffset);
    fwrite(&offset, 8, 1, downFile);

    fclose(downOffset);
    fclose(downFile);

    return r;
}

// Receive a packet from CDH
Reply sendPacket(const uint8_t *buf, size_t buflen)
{
    // packet payload format
    //   n bytes for raw data

    // check if all the upload metadata files exist
    if (access(UPLOAD_FILEMETA, F_OK) != 0 || access(UPLOAD_RECEIVED, F_OK) != 0)
        return EMPTY_REPLY(ERROR_NOT_UPLOADING);

    FILE *upReceived = fopen(UPLOAD_RECEIVED, "a");
    // If the file wasn't opened return an io error
    if (upReceived == NULL)
        return EMPTY_REPLY(ERROR_FILE_IO);

    // write packet data to the receiving file
    // TODO: fwrite error checking
    fwrite(buf, 1, buflen, upReceived);

    fclose(upReceived);

    return EMPTY_REPLY(SUCCESS);
}

// Erase upload packet data (if any)
Reply cancelUpload(const uint8_t *buf, size_t buflen)
{
    // Remove the upload files
    // TODO: log errors if any occur
    remove(UPLOAD_FILEMETA);
    remove(UPLOAD_RECEIVED);
    return EMPTY_REPLY(SUCCESS);
}

// Erase download packet data (if any)
Reply cancelDownload(const uint8_t *buf, size_t buflen)
{
    // Remove the download files
    // TODO: log errors if any occur
    remove(DOWNLOAD_FILEPATH);
    remove(DOWNLOAD_FILEOFFSET);
    return EMPTY_REPLY(SUCCESS);
}

// Verify the upload file integrity, move the file, and remove upload metadata
Reply finalizeUpload(const uint8_t *buf, size_t buflen)
{
    // verify that a file is being transferred
    if (access(UPLOAD_FILEMETA, F_OK) != 0 || access(UPLOAD_RECEIVED, F_OK) != 0)
        return EMPTY_REPLY(ERROR_NOT_UPLOADING);

    // open uploading meta files
    FILE *upMeta, *upReceived;

    upMeta = fopen(UPLOAD_FILEMETA, "r");
    if (upMeta == NULL)
        return EMPTY_REPLY(ERROR_FILE_IO);

    upReceived = fopen(UPLOAD_RECEIVED, "r");
    if (upReceived == NULL) {
        fclose(upMeta);
        return EMPTY_REPLY(ERROR_FILE_IO);
    }

    // load file and calculate sha256sum
    size_t fileLen;
    uint8_t *fileData;

    fileData = readFile(upReceived, &fileLen);
    fclose(upReceived);

    uint8_t shaSum[32];
    sha256calc(fileData, fileLen, shaSum);

    free(fileData);

    // read in the given shasum
    // TODO: fread error checking
    uint8_t shaSumGiven[32];
    fread(shaSumGiven, 32, 1, upMeta);
    fclose(upMeta);

    // verify the calculated sum matches the given sum
    if (!sha256cmp(shaSum, shaSumGiven))
        return EMPTY_REPLY(ERROR_SHASUM_MISMATCH);

    // Rename the upload temp file
    // TODO: log errors if any occur
    char *path = malloc(buflen + 1);
    memcpy(path, buf, buflen);
    path[buflen] = '\0';

    rename(UPLOAD_RECEIVED, path);

    free(path);

    // Remove upload file metadata
    // TODO: log errors
    remove(UPLOAD_FILEMETA);

    return EMPTY_REPLY(SUCCESS);
}

Reply (*commands[])(const uint8_t *, size_t) = {
    NULL,
    startDownload,
    startUpload,
    requestPacket,
    sendPacket,
    cancelUpload,
    cancelDownload,
    finalizeUpload
};

void runCommand(uint8_t command, uint8_t *buf, size_t buflen)
{
    Reply r = commands[command](buf + 1, buflen - 1);
    // TODO: send the generated reply
    if (r.payloadLen > 0)
        free(r.payload);
}
