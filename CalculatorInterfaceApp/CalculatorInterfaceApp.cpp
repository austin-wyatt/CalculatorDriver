// CalculatorInterfaceApp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#pragma comment(lib, "Cfgmgr32")


#define INITGUID

#include <windows.h>
#include <iostream>
#include <cfgmgr32.h>
#include <Fileapi.h>
#include <initguid.h>
#include "public.h"


#define MAX_DEVPATH_LENGTH 256

int main(int argc, char* argv[])
{
    WCHAR nameBuffer[MAX_DEVPATH_LENGTH];
    CM_Get_Device_Interface_List((LPGUID)&CALCULATOR_GUID, NULL, nameBuffer, MAX_DEVPATH_LENGTH, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);

    std::wcout << nameBuffer << std::endl;
    
    HANDLE hDevice;

    int totalSize = 0, index = 0;

    for (int i = 1; i < argc; i++) 
    {
        for(int j = 0;;j++)
        {
            if (argv[i][j] == '\0')
                break;
            totalSize++;
        }
    }

    char* commandLineString = new char[totalSize + argc];
    
    *(commandLineString + index++) = ' ';

    for (int i = 1; i < argc; i++)
    {
        for (int j = 0;;j++)
        {
            if (argv[i][j] == '\0')
                break;
            
            *(commandLineString + index++) = argv[i][j];
        }

        if (i != argc - 1) 
        {
            *(commandLineString + index++) = ' ';
        }
        else 
        {
            *(commandLineString + index++) = '\0';
        }
    }
    //"s 1000 gv"

    std::cout << commandLineString << std::endl;
    //std::cout << totalSize << std::endl;
    //std::cout << argc << std::endl;
    //std::cout << *nameBuffer << std::endl;
    //std::cout << index;

    char outputBuffer[1024];

    ZeroMemory(outputBuffer, 1024);

    if (*nameBuffer != 0) 
    {
        hDevice = CreateFile(nameBuffer, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

        BOOL operationSuccess;

        operationSuccess = WriteFile(hDevice, commandLineString, index, NULL, NULL);

        if(!operationSuccess)
        {
            std::cout << "Write operation failed" << std::endl;
        }

        /*CloseHandle(hDevice);
        hDevice = CreateFile(nameBuffer, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);*/
        DWORD bytesRead;
        operationSuccess = ReadFile(hDevice, outputBuffer, 256, &bytesRead, NULL);

        std::cout << "Bytes read: " << bytesRead << std::endl;

        if (!operationSuccess)
        {
            std::cout << "Read operation failed" << std::endl;
        }

        CloseHandle(hDevice);
    }

    printf(outputBuffer);
    
    delete[](commandLineString);
}
