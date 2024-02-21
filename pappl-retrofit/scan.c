#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pappl-retrofit/pappl-sane.h>

int main(int argc, char *argv[])
{
    SANE_Status status;
    const SANE_Device **deviceList;
    int selectedDeviceIndex;
    char fileName[1024];
    char storagePath[1024];
    char fullPath[2048];

    initializeSane();

    status = getScanningDevices(&deviceList);
    if (status != SANE_STATUS_GOOD)
    {
        printf("Error: Unable to get scanning devices.\n");
        shutdownSane();
        return EXIT_FAILURE;
    }

    printf("Available Scanning Devices:\n");
    for (int i = 0; deviceList[i]; ++i)
    {
        printf("%d: %s - %s\n", i + 1, deviceList[i]->vendor, deviceList[i]->model);
    }
    printf("Select a device (number): ");
    if (scanf("%d", &selectedDeviceIndex) != 1) {
        fprintf(stderr, "Failed to read the device index.\n");
        shutdownSane();
        return EXIT_FAILURE;
    }
    selectedDeviceIndex--; 

    if (selectedDeviceIndex < 0 || deviceList[selectedDeviceIndex] == NULL)
    {
        printf("Invalid selection.\n");
        shutdownSane();
        return EXIT_FAILURE;
    }

    printf("Enter file name (without extension): ");
    if (scanf("%1023s", fileName) != 1) {
        fprintf(stderr, "Failed to read the file name.\n");
        shutdownSane();
        return EXIT_FAILURE;
    }

    printf("Enter storage path: ");
    if (scanf("%1023s", storagePath) != 1) {
        fprintf(stderr, "Failed to read the storage path.\n");
        shutdownSane();
        return EXIT_FAILURE;
    }

    if (snprintf(fullPath, sizeof(fullPath), "%s/%s.pnm", storagePath, fileName) >= (int)sizeof(fullPath)) {
        fprintf(stderr, "Error: File name or path is too long.\n");
        shutdownSane();
        return EXIT_FAILURE;
    }

    SANE_Handle handle;
    status = openScanningDevice((SANE_Device *)deviceList[selectedDeviceIndex], &handle);
    if (status != SANE_STATUS_GOOD)
    {
        printf("Error: Unable to open device.\n");
        shutdownSane();
        return EXIT_FAILURE;
    }

    status = startScan(handle, fullPath);
    if (status != SANE_STATUS_GOOD)
    {
        printf("Error: Unable to start scan.\n");
        closeScanningDevice(handle);
        shutdownSane();
        return EXIT_FAILURE;
    }

    printf("Scan completed successfully. File saved at: %s\n", fullPath);

    closeScanningDevice(handle);
    shutdownSane();

    return EXIT_SUCCESS;
}
