#pragma once
#include "os_input.h"
#include "os_support.h"

#define checkhf(expr, format, ...) if (FAILED(expr))														\
{																											\
    fprintf(stdout, "CHECK FAILED: %s:%d:%s " format "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__);	\
    CString dbgstr;																							\
    dbgstr.Format(("%s"), format);																			\
    MessageBox(NULL,dbgstr, "FATAL ERROR", MB_OK);															\
}


#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

struct InputState
{
	int screenWidth;
	int screenHeight;
	int mouseX;
	int mouseY;
	int mouseDX;
	int mouseDY;

	unsigned char keyStates[256];
	DIMOUSESTATE mouseState;

	IDirectInput8* diDevice;
	IDirectInputDevice8* diKeyboard;
	IDirectInputDevice8* diMouse;

};

InputState GInputState;

void os_initializeInput()
{
	HRESULT result;
	
	GInputState.screenHeight = GAppInfo.curH;
	GInputState.screenWidth = GAppInfo.curH;
	GInputState.mouseX = 0;
	GInputState.mouseY = 0;

	// Initialize the main direct input interface.
	result = DirectInput8Create(GAppInfo.instance, DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&GInputState.diDevice, NULL);
	checkhf(result, "Failed to create DirectInput device");

	// Initialize the direct input interface for the keyboard.
	result = GInputState.diDevice->CreateDevice(GUID_SysKeyboard, &GInputState.diKeyboard, NULL);
	checkhf(result, "Failed to create DirectInput keyboard");

	// Set the data format.  In this case since it is a keyboard we can use the predefined data format.
	result = GInputState.diKeyboard->SetDataFormat(&c_dfDIKeyboard);
	checkhf(result, "Failed to set data format for DirectInput kayboard");

	// Set the cooperative level of the keyboard to not share with other programs.
	result = GInputState.diKeyboard->SetCooperativeLevel(GAppInfo.wndHdl, DISCL_FOREGROUND | DISCL_EXCLUSIVE);
	checkhf(result, "Failed to set keyboard to foreground exclusive");

	// Now acquire the keyboard.
	result = GInputState.diKeyboard->Acquire();
	checkhf(result, "Failed to acquire DI Keyboard");

	// Initialize the direct input interface for the mouse.
	result = GInputState.diDevice->CreateDevice(GUID_SysMouse, &GInputState.diMouse, NULL);
	checkhf(result, "Failed ot create DirectInput keyboard");

	// Set the data format for the mouse using the pre-defined mouse data format.
	result = GInputState.diMouse->SetDataFormat(&c_dfDIMouse);
	checkhf(result, "Failed to set data format for DirectInput mouse");

	//result = GInputState.diMouse->SetCooperativeLevel(wndHdl, DISCL_FOREGROUND | DISCL_EXCLUSIVE);
	//checkhf(result, "Failed to get exclusive access to DirectInput mouse");

	// Acquire the mouse.
	result = GInputState.diMouse->Acquire();
	checkhf(result, "Failed to acquire DirectInput mouse");
}

void os_pollInput()
{
	HRESULT result;

	// Read the keyboard device.
	result = GInputState.diKeyboard->GetDeviceState(sizeof(GInputState.keyStates), (LPVOID)&GInputState.keyStates);
	if (FAILED(result))
	{
		// If the keyboard lost focus or was not acquired then try to get control back.
		if ((result == DIERR_INPUTLOST) || (result == DIERR_NOTACQUIRED))
		{
			GInputState.diKeyboard->Acquire();
		}
		else
		{
			checkf(0, "Error polling keyboard");
		}
	}

	// Read the mouse device.
	result = GInputState.diMouse->GetDeviceState(sizeof(DIMOUSESTATE), (LPVOID)&GInputState.mouseState);
	if (FAILED(result))
	{
		// If the mouse lost focus or was not acquired then try to get control back.
		if ((result == DIERR_INPUTLOST) || (result == DIERR_NOTACQUIRED))
		{
			GInputState.diMouse->Acquire();
		}
		else
		{
			checkf(0, "Error polling mouse");
		}
	}

	// Update the location of the mouse cursor based on the change of the mouse location during the frame.
	GInputState.mouseX += GInputState.mouseState.lX;
	GInputState.mouseY += GInputState.mouseState.lY;
	GInputState.mouseDX = GInputState.mouseState.lX;
	GInputState.mouseDY = GInputState.mouseState.lY;

	// Ensure the mouse location doesn't exceed the screen width or height.
	if (GInputState.mouseX < 0) { GInputState.mouseX = 0; }
	if (GInputState.mouseY < 0) { GInputState.mouseY = 0; }

	if (GInputState.mouseX > GInputState.screenWidth) { GInputState.mouseX = GInputState.screenWidth; }
	if (GInputState.mouseY > GInputState.screenHeight) { GInputState.mouseY = GInputState.screenHeight; }
}

void os_shutdownInput()
{
	if (GInputState.diMouse)
	{
		GInputState.diMouse->Unacquire();
		GInputState.diMouse->Release();
		GInputState.diMouse = 0;
	}

	// Release the keyboard.
	if (GInputState.diKeyboard)
	{
		GInputState.diKeyboard->Unacquire();
		GInputState.diKeyboard->Release();
		GInputState.diKeyboard = 0;
	}

	// Release the main interface to direct input.
	if (GInputState.diDevice)
	{
		GInputState.diDevice->Release();
		GInputState.diDevice = 0;
	}
}

bool getKey(KeyCode key)
{
	return GInputState.keyStates[key] > 0;
}

int getMouseDX()
{
	return GInputState.mouseDX;
}

int getMouseDY()
{
	return GInputState.mouseDY;
}

int getMouseX()
{
	return GInputState.mouseX;
}

int getMouseY()
{
	return GInputState.mouseY;
}

int getMouseLeftButton()
{
	return GInputState.mouseState.rgbButtons[0] > 0;
}

int getMouseRightButton()
{
	return GInputState.mouseState.rgbButtons[2] > 0;
}