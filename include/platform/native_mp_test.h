#ifndef PLATFORM_NATIVE_MP_TEST_H
#define PLATFORM_NATIVE_MP_TEST_H

#include <common.h>

#ifdef __cplusplus
extern "C" {
#endif

int NativeMpTest_ConfigureFromArgs(int argc, char **argv);
int NativeMpTest_IsActive(void);
const char *NativeMpTest_GetWindowTitle(void);
void NativeMpTest_Update(void);
void NativeMpTest_ProcessPadSnapshots(struct PlatformInputPadSnapshot *pads, int count);

#ifdef __cplusplus
}
#endif

#endif
