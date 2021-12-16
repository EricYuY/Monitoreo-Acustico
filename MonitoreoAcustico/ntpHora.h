#ifndef NTPHORA_H
#define NTPHORA_H

#include <WiFi.h>
#include "time.h"

#define  ntpServer          "de.pool.ntp.org"
#define  gmtOffset_sec      -18000
#define  daylightOffset_sec 0

void printLocalTime();
void getHoraNTP(char *ntpHora);

#endif
