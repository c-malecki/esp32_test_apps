#ifndef RC_BLE_H
#define RC_BLE_H

#include "host/ble_hs.h"

#define RC_CAR_SVC_UUID 0x1100
#define RC_MOTOR_CHAR_UUID 0x1101

int ble_svr_init(void);
void ble_host_task(void *param);

#endif