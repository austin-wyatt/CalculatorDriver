#include "driver.h"

//Please do not load this driver onto your personal computer

NTSTATUS DriverEntry(
	_In_ PDRIVER_OBJECT DriverObject, 
	_In_ PUNICODE_STRING RegistryPath
)
{
	NTSTATUS status;

	WDFDRIVER hDriver;
	WDF_DRIVER_CONFIG driverConfig;

	//assign our DeviceAdd callback
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

	//Appease the compiler
	UNREFERENCED_PARAMETER(Driver);

	NTSTATUS status;

	KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "Calculator driver: CalcEvtDeviceAdd\n"));

	//KdBreakPoint();

	status = CalcDeviceCreate(DeviceInit);

	//KdBreakPoint();

	return status;
}

