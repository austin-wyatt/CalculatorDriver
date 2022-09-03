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

    //Get the driver's instance name via the GUI defined in public.h
    CM_Get_Device_Interface_List((LPGUID)&CALCULATOR_GUID, NULL, nameBuffer, MAX_DEVPATH_LENGTH, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);

    std::wcout << nameBuffer << std::endl;
    
    HANDLE hDevice;

    int totalSize = 0, index = 0;

    //Figure out how big the buffer we are going to send to the driver as input needs to be 
    //to accommodate all of the arguments.
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

    //Combine command line arguments into a single string with the arguments separated by a single space
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

    std::cout << commandLineString << std::endl;


    char outputBuffer[1024];

    ZeroMemory(outputBuffer, 1024);

    if (*nameBuffer != 0) 
    {
        //Open a read/write file handle to the driver
        hDevice = CreateFile(nameBuffer, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

        BOOL operationSuccess;

        //Send our space-separated argument string to the driver for processing
        operationSuccess = WriteFile(hDevice, commandLineString, index, NULL, NULL);

        if(!operationSuccess)
        {
            std::cout << "Write operation failed" << std::endl;
        }

        DWORD bytesRead;

        //Pull data from the driver's output buffer (regardless of whether we 
        //sent a command that would elicit a change in it or not)
        operationSuccess = ReadFile(hDevice, outputBuffer, 256, &bytesRead, NULL);

        std::cout << "Bytes read: " << bytesRead << std::endl;

        if (!operationSuccess)
        {
            std::cout << "Read operation failed" << std::endl;
        }

        //Free the handle or bad stuff will happen
        CloseHandle(hDevice);
    }

    //Print whatever we managed to pull from the driver's output buffer
    printf(outputBuffer);
    
    delete[](commandLineString);
}
