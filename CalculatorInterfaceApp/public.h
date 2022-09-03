//Convenient C macro to globally define a GUID 
// 
//Some combination of "#define INITGUID" and "#include <initguid.h>" will need
//to be used in the files that reference CALCULATOR_GUID to appease the compiler
DEFINE_GUID(
	CALCULATOR_GUID,
	0x6229d641, 0x977f, 0x4216, 0xae, 0x41, 0xc6, 0x0c, 0xbe, 0x6c, 0x30, 0x17);
// {6229d641-977f-4216-ae41-c60cbe6c3017}