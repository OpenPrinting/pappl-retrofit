#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>  
#include <string.h>  
#include <unistd.h>  
#include <limits.h>  

#include "sane/sane.h"
#include "sane/saneopts.h"

#ifdef __cplusplus
extern "C" {
#endif

void initializeSane();
void cancelScan(SANE_Handle sane_handle);
void closeScanningDevice(SANE_Handle sane_handle);
void shutdownSane();
SANE_Status getScanningDevices(const SANE_Device ***device_list);
SANE_Status openScanningDevice(SANE_Device *device, SANE_Handle *sane_handle);
SANE_Status startScan(SANE_Handle sane_handle, SANE_String_Const fileName);

#ifdef __cplusplus
}
#endif
