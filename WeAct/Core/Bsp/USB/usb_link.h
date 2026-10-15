#ifndef USB_LINK_H
#define USB_LINK_H
#include <stdint.h>
/* Вызываются из основного цикла после MX_USB_HOST_Process. */
int UsbLink_ReadLine(char line[96]);
int UsbLink_SendLine(const char *line);
#endif
