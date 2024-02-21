#include <pappl-retrofit/pappl-sane.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  uint8_t *data;
  int width;
  int height;
  int currentX;
  int currentY;
} scannedImage;

#define IMAGE_HEIGHT 256
static SANE_Handle scannerHandle = NULL;
static int isVerbose;
static SANE_Byte *scanBuffer;
static size_t scanBufferSize;

static void authenticationCallback(SANE_String_Const resource, SANE_Char *username, SANE_Char *password)
{
  printf("Authentication Callback\n");
}

void initializeSane()
{
  SANE_Int versionCode = 0;
  sane_init(&versionCode, authenticationCallback);
  printf("Version: %d\n", versionCode);
}

SANE_Status getScanningDevices(const SANE_Device ***deviceList)
{
  printf("Getting all Scanning Devices\n");
  SANE_Status status = sane_get_devices(deviceList, SANE_FALSE);
  if (status)
  {
    printf("Could not retrieve devices: %s\n", sane_strstatus(status));
  }
  return status;
}

SANE_Status openScanningDevice(SANE_Device *device, SANE_Handle *handle)
{
  SANE_Status status = sane_open(device->name, handle);
  if (status)
  {
    printf("Scanning device could not be opened %s: %s\n", device->name, sane_strstatus(status));
  }
  return status;
}

void cancelScan(SANE_Handle handle)
{
  sane_cancel(handle);
}

void closeScanningDevice(SANE_Handle handle)
{
  sane_close(handle);
}

void shutdownSane()
{
  sane_exit();
}

static void writeHeaders(SANE_Frame format, int width, int height, int depth, FILE *outputFile)
{
  switch (format)
  {
    case SANE_FRAME_RED:
    case SANE_FRAME_GREEN:
    case SANE_FRAME_BLUE:
    case SANE_FRAME_RGB:
      fprintf(outputFile, "P6\n# SANE data format:\n%d %d\n%d\n", width, height, (depth <= 8) ? 255 : 65535);
      break;
    default:
      if (depth == 1)
        fprintf(outputFile, "P4\n# SANE data format:\n%d %d\n", width, height);
      else
        fprintf(outputFile, "P5\n# SANE data format:\n%d %d\n%d\n", width, height, (depth <= 8) ? 255 : 65535);
      break;
  }
}

static void *imageCursor(scannedImage *image)
{
  if (++image->currentX >= image->width)
  {
    image->currentX = 0;
    if (++image->currentY >= image->height || !image->data)
  {
    size_t oldSize = 0, newSize;

    if (image->data)
    oldSize = image->height * image->width;

    image->height += IMAGE_HEIGHT;
    newSize = image->height * image->width;

    if (image->data)
    image->data = realloc(image->data, newSize);
    else
    image->data = malloc(newSize);
    if (image->data)
    memset(image->data + oldSize, 0, newSize - oldSize);
  }
  }
  if (!image->data)
  fprintf(stderr, "Image buffer could not be allocated: (%dx%d)\n", image->width, image->height);
  return image->data;
}

static SANE_Status scanImageToFile(FILE *outputFile)
{
  int index, bytesRead, isFirstFrame = 1, bufferOffset = 0, needsBuffering = 0;
  SANE_Byte minByteValue = 0xFF, maxByteValue = 0;
  SANE_Parameters scanParams;
  SANE_Status scanStatus;
  scannedImage scannedImage = {0, 0, 0, 0, 0};
  SANE_Word totalBytesScanned = 0;
  do
  {
    if (!isFirstFrame)
    {
      scanStatus = sane_start(scannerHandle);
      if (scanStatus != SANE_STATUS_GOOD)
      {
        goto cleanup;
      }
    }

    scanStatus = sane_get_parameters(scannerHandle, &scanParams);
    if (scanStatus != SANE_STATUS_GOOD)
    {
      goto cleanup;
    }

    if (isFirstFrame)
    {
      switch (scanParams.format)
      {
        case SANE_FRAME_RED:
        case SANE_FRAME_GREEN:
        case SANE_FRAME_BLUE:
          assert(scanParams.depth == 8);
          needsBuffering = 1;
          bufferOffset = scanParams.format - SANE_FRAME_RED;
          break;
        case SANE_FRAME_RGB:
          assert(scanParams.depth == 8 || scanParams.depth == 16);
        case SANE_FRAME_GRAY:
          assert(scanParams.depth == 1 || scanParams.depth == 8 || scanParams.depth == 16);
          if (scanParams.lines < 0)
          {
            needsBuffering = 1;
            bufferOffset = 0;
          }
          else
          {
            writeHeaders(scanParams.format, scanParams.pixels_per_line, scanParams.lines, scanParams.depth, outputFile);
          }
          break;
        default:
          break;
      }

      if (needsBuffering)
      {
        scannedImage.width = scanParams.bytes_per_line;
        scannedImage.height = (scanParams.lines >= 0) ? scanParams.lines - IMAGE_HEIGHT + 1 : 0;
        scannedImage.currentX = scannedImage.width - 1;
        scannedImage.currentY = -1;
        if (!imageCursor(&scannedImage))
        {
          scanStatus = SANE_STATUS_NO_MEM;
          goto cleanup;
        }
      }
    }
    else
    {
      assert(scanParams.format >= SANE_FRAME_RED && scanParams.format <= SANE_FRAME_BLUE);
      bufferOffset = scanParams.format - SANE_FRAME_RED;
      scannedImage.currentX = scannedImage.currentY = 0;
    }

    while (1)
    {
      double progress;
      scanStatus = sane_read(scannerHandle, scanBuffer, scanBufferSize, &bytesRead);
      totalBytesScanned += bytesRead;
      progress = ((totalBytesScanned * 100.) / (double) (scanParams.bytes_per_line * scanParams.lines * ((scanParams.format == SANE_FRAME_RGB || scanParams.format == SANE_FRAME_GRAY) ? 1 : 3)));
      if (progress > 100.)
        progress = 100.;

      if (scanStatus != SANE_STATUS_GOOD)
      {
        if (scanStatus != SANE_STATUS_EOF)
        {
          return scanStatus;
        }
        break;
      }

      if (needsBuffering)
      {
        for (index = 0; index < bytesRead; ++index)
        {
          if (scanParams.format == SANE_FRAME_RGB)
          {
            scannedImage.data[bufferOffset + index] = scanBuffer[index];
          }
          else if (scanParams.format >= SANE_FRAME_RED && scanParams.format <= SANE_FRAME_BLUE)
          {
            scannedImage.data[bufferOffset + 3 * index] = scanBuffer[index];
          }
          else
          {
            scannedImage.data[bufferOffset + index] = scanBuffer[index];
          }

          if (!imageCursor(&scannedImage))
          {
            scanStatus = SANE_STATUS_NO_MEM;
            goto cleanup;
          }
        }
        bufferOffset += (scanParams.format == SANE_FRAME_RGB) ? bytesRead : 3 * bytesRead;
      }
      else
      {
        if (scanParams.depth != 16)
        {
          fwrite(scanBuffer, 1, bytesRead, outputFile);
        }
        else
        {
          for (index = 0; index < (bytesRead - 1); index += 2)
          {
            unsigned char tempByte = scanBuffer[index];
            scanBuffer[index] = scanBuffer[index + 1];
            scanBuffer[index + 1] = tempByte;
          }
          fwrite(scanBuffer, 1, bytesRead, outputFile);
        }
      }

      if (isVerbose && scanParams.depth == 8)
      {
        for (index = 0; index < bytesRead; ++index)
        {
          if (scanBuffer[index] >= maxByteValue)
            maxByteValue = scanBuffer[index];
          else if (scanBuffer[index] < minByteValue)
            minByteValue = scanBuffer[index];
        }
      }
    }
    isFirstFrame = 0;
  } while (!scanParams.last_frame);

  if (needsBuffering)
  {
    scannedImage.height = scannedImage.currentY + 1;
    writeHeaders(scanParams.format, scannedImage.width, scannedImage.height, scanParams.depth, outputFile);
    fwrite(scannedImage.data, 1, scannedImage.width * scannedImage.height, outputFile);
  }

  fflush(outputFile);

cleanup:
  if (scannedImage.data)
  {
    free(scannedImage.data);
  }

  return scanStatus;
}

SANE_Status scanToFile(const char *fileName)
{
  SANE_Status scanStatus;
  FILE *outputFile = NULL;
  char finalPath[1024];
  char tempPath[1024];
  scanBufferSize = (32 * 1024);
  scanBuffer = (SANE_Byte *)malloc(scanBufferSize);

  do
  {
    int processID = getpid();
    sprintf(finalPath, "%s%d.pnm", fileName, processID);
    strcpy(tempPath, finalPath);
    strcat(tempPath, ".part");

    scanStatus = sane_start(scannerHandle);
    if (scanStatus != SANE_STATUS_GOOD)
    {
      break;
    }

    outputFile = fopen(tempPath, "w");
    if (outputFile == NULL)
    {
      scanStatus = SANE_STATUS_ACCESS_DENIED;
      break;
    }

    scanStatus = scanImageToFile(outputFile);

    if (scanStatus == SANE_STATUS_GOOD || scanStatus == SANE_STATUS_EOF)
    {
      scanStatus = SANE_STATUS_GOOD;
      if (outputFile && fclose(outputFile) != 0)
      {
        scanStatus = SANE_STATUS_ACCESS_DENIED;
        break;
      }
      outputFile = NULL;
      if (rename(tempPath, finalPath) != 0)
      {
        scanStatus = SANE_STATUS_ACCESS_DENIED;
        break;
      }
    }
    else
    {
      break;
    }
  } while (0);

  if (scanStatus != SANE_STATUS_GOOD && scannerHandle)
  {
    sane_cancel(scannerHandle);
  }
  if (outputFile)
  {
    fclose(outputFile);
  }
  if (scanBuffer)
  {
    free(scanBuffer);
    scanBuffer = NULL;
  }

  return scanStatus;
}

SANE_Status startScan(SANE_Handle handle, SANE_String_Const fileName)
{
  scannerHandle = handle;
  return scanToFile(fileName);
}

#ifdef __cplusplus
}
#endif
