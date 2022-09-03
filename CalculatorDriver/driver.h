#pragma once

#include <initguid.h>
#include <ntddk.h>
#include <wdf.h>
#include "calcDevice.h"
#include "public.h"


DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD CalcEvtDeviceAdd;