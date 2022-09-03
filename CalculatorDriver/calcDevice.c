#include "driver.h"
#include <stdio.h>
#include <stdlib.h>

BOOLEAN BUFFERS_INITIALIZED = FALSE;

NTSTATUS CalcDeviceCreate(PWDFDEVICE_INIT DeviceInit) 
{
	NTSTATUS status;
	WDFDEVICE hDevice;

	KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "Calculator driver: CalcDeviceCreate\n"));

	status = WdfDeviceCreate(&DeviceInit, WDF_NO_OBJECT_ATTRIBUTES, &hDevice);

	if (NT_SUCCESS(status))
	{
		status = WdfDeviceCreateDeviceInterface(
			hDevice,
			&CALCULATOR_GUID,
			NULL);

		if (NT_SUCCESS(status))
		{
			status = InitializeDeviceInterface(hDevice);
		}
	}

	return status;
}

NTSTATUS InitializeDeviceInterface(WDFDEVICE Device)
{
	NTSTATUS status;
	WDFQUEUE queue;
	WDF_IO_QUEUE_CONFIG queueConfig;
	//We want to clear the device's buffer when destroyed so we need to initialize attributes for this queue
	WDF_OBJECT_ATTRIBUTES attributes;

	PCALC_DEVICE_CONTEXT deviceContext;

	PAGED_CODE();

	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchSequential);
	queueConfig.EvtIoRead = CalcEvtIoRead;
	queueConfig.EvtIoWrite = CalcEvtIoWrite;

	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, CALC_DEVICE_CONTEXT);
	attributes.EvtDestroyCallback = CalcEvtDeviceContextDestroy;
	attributes.SynchronizationScope = WdfSynchronizationScopeNone;

	status = WdfIoQueueCreate(Device, &queueConfig, &attributes, &queue);
	
	if (!NT_SUCCESS(status)) 
	{
		KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "Calculator driver: IO queue create failed\n"));
		return status;
	}

	deviceContext = GetCalcDeviceContext(queue);

	deviceContext->CalculatedValue = 0;
	deviceContext->WriteBuffer = NULL;
	deviceContext->ReadBuffer = NULL;
	deviceContext->InstructionBuffer = NULL;

	//if this were async, create spin lock/wait lock here

	return status;
}

void CalcEvtIoRead(IN WDFQUEUE Queue, IN WDFREQUEST Request, IN size_t Length)
{
	//KdBreakPoint();

	InitializeBuffers(Queue);

	NTSTATUS status;
	WDFMEMORY memory;
	PCALC_DEVICE_CONTEXT deviceContext = GetCalcDeviceContext(Queue);

	if (deviceContext->ReadLength == 0) 
	{
		WdfRequestComplete(Request, STATUS_SUCCESS);
		return;
	}

	status = WdfRequestRetrieveOutputMemory(Request, &memory);

	if (deviceContext->ReadLength < Length) 
	{
		Length = deviceContext->ReadLength;
	}

	status = WdfMemoryCopyFromBuffer(memory, 0, deviceContext->ReadBuffer, Length);

	WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, Length);
	return;
}

void CalcEvtIoWrite(IN WDFQUEUE Queue, IN WDFREQUEST Request, IN size_t Length)
{
	//KdBreakPoint();

	InitializeBuffers(Queue);

	NTSTATUS status;
	WDFMEMORY memory;
	PCALC_DEVICE_CONTEXT deviceContext = GetCalcDeviceContext(Queue);

	status = WdfRequestRetrieveInputMemory(Request, &memory);
	status = WdfMemoryCopyToBuffer(memory, 0, deviceContext->WriteBuffer, Length);

	if (NT_SUCCESS(status)) 
	{
		deviceContext->WriteLength = (ULONG)Length;
	}
	else 
	{
		deviceContext->WriteLength = 0;
	}

	ResolveInputBuffer(deviceContext);

	WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, deviceContext->WriteLength);
	return;
}

void ResolveInputBuffer(IN PCALC_DEVICE_CONTEXT deviceContext)
{
	char tempInstructionBuffer[MAX_WORD_SIZE];

	int wordStart = 0, wordEnd = 0;
	BOOLEAN inWord = FALSE;

	char* writeBuffer = (char*)deviceContext->WriteBuffer;

	int newInstructionCount = 0;

	char currentChar;

	for(ULONG i = 0; i < deviceContext->WriteLength; i++)
	{
		currentChar = *(writeBuffer + i);

		if (currentChar == ' ' || currentChar == '\0')
		{
			if (inWord) 
			{
				wordEnd = i;
				inWord = FALSE;

				FillTempBuffer(tempInstructionBuffer, writeBuffer, wordStart, wordEnd - wordStart);

				int newInstruction = atoi(writeBuffer + wordStart);
				if (newInstruction == 0 && *writeBuffer + wordStart != '0')
				{
					newInstruction = *(int*)tempInstructionBuffer;
				}
				
				*(deviceContext->InstructionBuffer + deviceContext->InstructionLength) = newInstruction;
				deviceContext->InstructionLength++;

				if (deviceContext->InstructionLength >= BUFFER_SIZE - 1)
				{
					KdBreakPoint();

					memcpy(deviceContext->ReadBuffer, "Instruction buffer overflow. The instruction buffer has been cleared.", 70);
					deviceContext->ReadLength = 70;
					deviceContext->InstructionLength = 0;
					return;
				}

				newInstructionCount++;
				i--;
			}
			else
			{
				wordStart = i + 1;
				inWord = TRUE;
			}
		}
	}

	RtlZeroMemory(writeBuffer, deviceContext->WriteLength);
	deviceContext->WriteLength = 0;

	if (newInstructionCount > 0) 
	{
		EvaluateInstructions(deviceContext, newInstructionCount);
	}
}

void InitializeBuffers(IN WDFOBJECT queue)
{
	if (!BUFFERS_INITIALIZED) 
	{
		//KIRQL oldIRQL = PASSIVE_LEVEL;
		//KeRaiseIrql(PASSIVE_LEVEL, &oldIRQL);

		KIRQL irql = KeGetCurrentIrql();
		char msg[20];
		sprintf(msg, "%c", irql);
		KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, msg));

		BUFFERS_INITIALIZED = TRUE;

		PCALC_DEVICE_CONTEXT deviceContext = GetCalcDeviceContext(queue);

		deviceContext->ReadBuffer = ExAllocatePool2(POOL_FLAG_NON_PAGED, BUFFER_SIZE, 'Read');
		deviceContext->WriteBuffer = ExAllocatePool2(POOL_FLAG_NON_PAGED, BUFFER_SIZE, 'Writ');
		deviceContext->InstructionBuffer = ExAllocatePool2(POOL_FLAG_NON_PAGED, BUFFER_SIZE, 'Inst');

		//KeLowerIrql(oldIRQL);
	}
}

void CalcEvtDeviceContextDestroy(IN WDFOBJECT queue)
{
	PCALC_DEVICE_CONTEXT deviceContext = GetCalcDeviceContext(queue);

	UNREFERENCED_PARAMETER(queue);
	UNREFERENCED_PARAMETER(deviceContext);

	/*KIRQL oldIRQL = PASSIVE_LEVEL;
	KeRaiseIrql(DISPATCH_LEVEL, &oldIRQL);*/

	KIRQL irql = KeGetCurrentIrql();
	char msg[20];
	sprintf(msg, "%c", irql);
	KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, msg));

	if (deviceContext->ReadBuffer != NULL)
	{
		ExFreePool(deviceContext->ReadBuffer);
		deviceContext->ReadBuffer = NULL;
	}

	if (deviceContext->WriteBuffer != NULL)
	{
		ExFreePool(deviceContext->WriteBuffer);
		deviceContext->WriteBuffer = NULL;
	}

	if (deviceContext->InstructionBuffer != NULL)
	{
		ExFreePool(deviceContext->InstructionBuffer);
		deviceContext->InstructionBuffer = NULL;
	}

	//KeLowerIrql(oldIRQL);

	BUFFERS_INITIALIZED = FALSE;
}

void EvaluateInstructions(IN PCALC_DEVICE_CONTEXT deviceContext, int newInstructionCount)
{
	char* readBuffer = (char*)deviceContext->ReadBuffer;

	int* instructionBuffer = deviceContext->InstructionBuffer;

	int currInstruction;
	int nextValue;

	int strLen;

	BOOLEAN hasNextInstruction;

	//iterate through new instructions only
	for(ULONG i = deviceContext->InstructionLength - newInstructionCount; i < deviceContext->InstructionLength; i++)
	{
		currInstruction = *(instructionBuffer + i);

		if (i < deviceContext->InstructionLength - 1) 
			hasNextInstruction = TRUE;
		else 
			hasNextInstruction = FALSE;

		switch (currInstruction) 
		{
			case CALC_INSTRUCTION_SET:
				if(hasNextInstruction)
				{
					nextValue = *(instructionBuffer + i + 1);
					deviceContext->CalculatedValue = nextValue;

					*(instructionBuffer + i) = CALC_INSTRUCTION_NOOP;
					*(instructionBuffer + i + 1) = CALC_INSTRUCTION_NOOP;
					i++;
				}
				break;
			case CALC_INSTRUCTION_EC:
				CalculateValue(deviceContext);
			case CALC_INSTRUCTION_CLEAR:
				deviceContext->InstructionLength = 0;
				break;
			case CALC_INSTRUCTION_EVAL:
				CalculateValue(deviceContext);
				break;
			case CALC_INSTRUCTION_GET_BUFFER:
				memcpy(readBuffer, instructionBuffer, deviceContext->InstructionLength);
				deviceContext->ReadLength = deviceContext->InstructionLength * sizeof(int);
				break;
			case CALC_INSTRUCTION_GET_VALUE:
				strLen = sprintf(readBuffer, "%ld", deviceContext->CalculatedValue);
				deviceContext->ReadLength = strLen;
				break;
		}
	}
}

void CalculateValue(IN PCALC_DEVICE_CONTEXT deviceContext)
{
	int* instructionBuffer = deviceContext->InstructionBuffer;
	int currInstruction;

	int nextValue;

	BOOLEAN hasNextInstruction;

	for (ULONG i = 0; i < deviceContext->InstructionLength; i++) 
	{
		currInstruction = *(instructionBuffer + i);

		if (i < deviceContext->InstructionLength - 1)
			hasNextInstruction = TRUE;
		else
			hasNextInstruction = FALSE;

		switch (currInstruction) 
		{
			case CALC_INSTRUCTION_ADD:
				if (hasNextInstruction)
				{
					nextValue = *(instructionBuffer + i + 1);
					deviceContext->CalculatedValue += nextValue;
					i++;
				}
				break;
			case CALC_INSTRUCTION_SUB:
				if (hasNextInstruction)
				{
					nextValue = *(instructionBuffer + i + 1);
					deviceContext->CalculatedValue -= nextValue;
					i++;
				}
				break;
			case CALC_INSTRUCTION_MULT:
				if (hasNextInstruction)
				{
					nextValue = *(instructionBuffer + i + 1);
					deviceContext->CalculatedValue *= nextValue;
					i++;
				}
				break;
			case CALC_INSTRUCTION_DIV:
				if (hasNextInstruction)
				{
					nextValue = *(instructionBuffer + i + 1);
					deviceContext->CalculatedValue /= nextValue;
					i++;
				}
				break;
		}
	}
}

void FillTempBuffer(char* tempBuffer, char* sourceBuffer, int start, int length)
{
	if (length > MAX_WORD_SIZE)
		length = MAX_WORD_SIZE;

	for(int i = 0; i < length; i++)
	{
		*(tempBuffer + i) = *(sourceBuffer + start + (length - i - 1));
	}

	for(int i = length; i < MAX_WORD_SIZE; i++)
	{
		*(tempBuffer + i) = '\0';
	}
}


