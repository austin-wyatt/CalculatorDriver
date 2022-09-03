#define BUFFER_SIZE 1024
#define MAX_WORD_SIZE 4

//written data will be passed as a character array separated by spaces
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

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(
	CALC_DEVICE_CONTEXT,
	GetCalcDeviceContext)

NTSTATUS CalcDeviceCreate(PWDFDEVICE_INIT DeviceInit);
NTSTATUS InitializeDeviceInterface(WDFDEVICE Device);

EVT_WDF_IO_QUEUE_IO_READ CalcEvtIoRead;
EVT_WDF_IO_QUEUE_IO_WRITE CalcEvtIoWrite;

EVT_WDF_OBJECT_CONTEXT_DESTROY CalcEvtDeviceContextDestroy;


void InitializeBuffers(IN WDFOBJECT queue);

void ResolveInputBuffer(IN PCALC_DEVICE_CONTEXT deviceContext);
void EvaluateInstructions(IN PCALC_DEVICE_CONTEXT deviceContext, int newInstructionCount);
void FillTempBuffer(char* tempBuffer, char* sourceBuffer, int start, int length);
void CalculateValue(IN PCALC_DEVICE_CONTEXT deviceContext);
