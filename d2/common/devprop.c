#define ADDID_DEFINE_DEVPROPKEY
#include "devprop.h"

/* 実体定義: DEVPKEY_Device_InstanceId / DEVPKEY_Device_ContainerId */

/* DEVPKEY_Device_InstanceId */
const DEVPROPKEY DEVPKEY_Device_InstanceId = {
    {0x78c34fc8, 0x104a, 0x4aca, {0x9e, 0xa4, 0x52, 0x4d, 0x52, 0x99, 0x6e, 0x57}},
    256
};

/* DEVPKEY_Device_ContainerId */
const DEVPROPKEY DEVPKEY_Device_ContainerId = {
    {0x8C7ED206, 0x3F8A, 0x4827, {0xB3, 0xAB, 0xAE, 0x9E, 0x1F, 0xAE, 0xFC, 0x6C}},
    2
};