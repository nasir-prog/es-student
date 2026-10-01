#include "log.h"
#include "device.h"

void log_version(void)
{
    printf("device: %s %s, built %s %s, log level %d\n",
           DEVICE_NAME, FIRMWARE_VERSION,
           __DATE__, __TIME__, LOG_LEVEL);
}

void log_prefix(const char *level, const char *function, int line)
{
    printf("%s %s:%d ", level, function, line);
}
