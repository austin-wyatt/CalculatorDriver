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
		//Create the device interface. This is what the CALC_DEVICE_CONTEXT device extension is attached to.
		//This device interface uses the GUID defined in public.h which will be used to find its system path later
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
	WDF_OBJECT_ATTRIBUTES attributes;

	PCALC_DEVICE_CONTEXT deviceContext;

	PAGED_CODE();

	//Initialize the device's IO queue
	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchSequential);
	//Assign read and write callbacks
	queueConfig.EvtIoRead = CalcEvtIoRead;
	queueConfig.EvtIoWrite = CalcEvtIoWrite;

	//Initialize object attributes for this device. Since we have a device extension 
	//we use the extended CONTEXT_TYPE macro which calls WDF_OBJECT_ATTRIBUTES_INIT
	//followed by WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE to assign the context type 
	//to our device interface.
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, CALC_DEVICE_CONTEXT);

	//Assign our destructor callback
	attributes.EvtDestroyCallback = CalcEvtDeviceContextDestroy;
	attributes.SynchronizationScope = WdfSynchronizationScopeNone;

	//Using the config and attributes we assigned, create the WDFQUEUE object for the device interface
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

	//Retrieve the memory address that the read request wants the driver to place data into
	status = WdfRequestRetrieveOutputMemory(Request, &memory);

	if (deviceContext->ReadLength < Length) 
	{
		Length = deviceContext->ReadLength;
	}

	//Move the contents of the ReadBuffer into the request's memory
	status = WdfMemoryCopyFromBuffer(memory, 0, deviceContext->ReadBuffer, Length);

	//Return the amount of bytes that the requester will be recieving 
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

	//Retrieve the memory address that the write request wants the driver to pull from
	status = WdfRequestRetrieveInputMemory(Request, &memory);
	//Pull the memory from the request memory into the WriteBuffer
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

				//Fill the tempInstructionBuffer with a reversed and \0 padded version of the parameter string
				FillTempBuffer(tempInstructionBuffer, writeBuffer, wordStart, wordEnd - wordStart);

				//Check if our instruction is a number or a string. If it is a number then we want to convert the string 
				//to an integer. Otherwise we want to cast the 4 byte tempInstructionBuffer into an integer
				int newInstruction = atoi(writeBuffer + wordStart);
				if (newInstruction == 0 && *writeBuffer + wordStart != '0')
				{
					newInstruction = *(int*)tempInstructionBuffer;
				}
				
				//Place the new instruction onto the end of the instruction buffer 
				*(deviceContext->InstructionBuffer + deviceContext->InstructionLength) = newInstruction;
				deviceContext->InstructionLength++;

				//Check if we are about to overflow and, if so, clear the instruction buffer
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

	//Zero out the write buffer to prepare for another write request
	//(it would also be sufficient to set the WriteLength to 0 and simply
	//write over the old data)
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
				memcpy(readBuffer, instructionBuffer, deviceContext->InstructionLength * sizeof(int));
				deviceContext->ReadLength = deviceContext->InstructionLength * sizeof(int);
				break;
			case CALC_INSTRUCTION_GET_VALUE:
				//print our calculated value into the read buffer
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


