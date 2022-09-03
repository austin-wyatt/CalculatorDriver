#define BUFFER_SIZE 1024
#define MAX_WORD_SIZE 4

//Commands:
//s (int)  : set calculated value to supplied int
//c        : clear instruction buffer
//ec	   : evalute and clear instruction buffer 
//e        : evalute instruction buffer 
//+ (int)  : add supplied int to calculated value
//- (int)  : subtract supplied int from calculated value
//* (int)  : multiply supplied int to calculated value
//'/' (int): divide calculated value by supplied int
// 
//sb       : get buffer, move write buffer into read buffer
//gv       : get value, move calculated value into read buffer

enum calc_instruction {
	CALC_INSTRUCTION_NOOP = 'no',
	CALC_INSTRUCTION_SET = 's',
	CALC_INSTRUCTION_CLEAR = 'c',
	CALC_INSTRUCTION_EC = 'ec',
	CALC_INSTRUCTION_EVAL = 'e',
	CALC_INSTRUCTION_ADD = '+',
	CALC_INSTRUCTION_SUB = '-',
	CALC_INSTRUCTION_MULT = '*',
	CALC_INSTRUCTION_DIV = '/',
	CALC_INSTRUCTION_GET_BUFFER = 'gb',
	CALC_INSTRUCTION_GET_VALUE = 'gv'
};


//The device extension that holds the IO and instruction buffers as well as our current calculated value
typedef struct _CALC_DEVICE_CONTEXT
{
	PVOID ReadBuffer;
	ULONG ReadLength;
	PVOID WriteBuffer;
	ULONG WriteLength;
	LONG CalculatedValue;

	int* InstructionBuffer;
	ULONG InstructionLength;
} CALC_DEVICE_CONTEXT, * PCALC_DEVICE_CONTEXT;

//Necessary C macro to call for any device extensions that need to be created.
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(
	CALC_DEVICE_CONTEXT,
	GetCalcDeviceContext)


NTSTATUS CalcDeviceCreate(PWDFDEVICE_INIT DeviceInit);
NTSTATUS InitializeDeviceInterface(WDFDEVICE Device);

//Return the contents of the read buffer to the requester
EVT_WDF_IO_QUEUE_IO_READ CalcEvtIoRead;

//Evaluate the passed command string and make any necessary changes to the 
//calculated value and read buffer.
EVT_WDF_IO_QUEUE_IO_WRITE CalcEvtIoWrite;


//Clean up buffers when the driver is unloaded
EVT_WDF_OBJECT_CONTEXT_DESTROY CalcEvtDeviceContextDestroy;

//Called every time a read or write occurs
//If the buffers haven't been initialized, allocate memory for them
void InitializeBuffers(IN WDFOBJECT queue);

//Convert the character string that was passed into the input buffer into integer instruction codes
//that can be evaluated
void ResolveInputBuffer(IN PCALC_DEVICE_CONTEXT deviceContext);
//Evaluate the instructions in the instruction buffer and calculate the new value/update 
//the read buffer accordingly
void EvaluateInstructions(IN PCALC_DEVICE_CONTEXT deviceContext, int newInstructionCount);
//Reverse the order of the input string and pad with null terminators to simplify their conversion
//to integer instruction codes
void FillTempBuffer(char* tempBuffer, char* sourceBuffer, int start, int length);
//Enumerate the instruction buffer and resolve any add, subtract, multiply, or divide instructions
void CalculateValue(IN PCALC_DEVICE_CONTEXT deviceContext);
