#include "driver.h"

NTSTATUS DriverEntry(
	_In_ PDRIVER_OBJECT DriverObject, 
	_In_ PUNICODE_STRING RegistryPath
)
{
	NTSTATUS status;

	WDFDRIVER hDriver;
	WDF_DRIVER_CONFIG driverConfig;

	WDF_DRIVER_CONFIG_INIT(&driverConfig, CalcEvtDeviceAdd);

	KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "Calculator driver: DriverEntry\n"));

	status = WdfDriverCreate(DriverObject, RegistryPath, NULL, &driverConfig, &hDriver);

	return status;
}


NTSTATUS CalcEvtDeviceAdd(
	_In_ WDFDRIVER Driver,
	_Inout_ PWDFDEVICE_INIT DeviceInit
) 
{
	PAGED_CODE();

	UNREFERENCED_PARAMETER(Driver);

	NTSTATUS status;

	KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "Calculator driver: CalcEvtDeviceAdd\n"));

	//KdBreakPoint();

	status = CalcDeviceCreate(DeviceInit);

	//KdBreakPoint();

	return status;
}

