#pragma once

//these map to directInput keycodes
//but I didn't want to have to include directInput
//in any file that needed keycodes, so I redefined them
enum KeyCode
{
	KEY_ESCAPE = 0x01,
	KEY_1 = 0x02,
	KEY_2 = 0x03,
	KEY_3 = 0x04,
	KEY_4 = 0x05,
	KEY_5 = 0x06,
	KEY_6 = 0x07,
	KEY_7 = 0x08,
	KEY_8 = 0x09,
	KEY_9 = 0x0A,
	KEY_0 = 0x0B,
	KEY_MINUS = 0x0C,
	KEY_EQUALS = 0x0D,
	KEY_BACKSPACE = 0x0E,
	KEY_TAB = 0x0F,
	KEY_Q = 0x10,
	KEY_W = 0x11,
	KEY_E = 0x12,
	KEY_R = 0x13,
	KEY_T = 0x14,
	KEY_Y = 0x15,
	KEY_U = 0x16,
	KEY_I = 0x17,
	KEY_O = 0x18,
	KEY_P = 0x19,
	KEY_LBRACKET = 0x1A,
	KEY_RBRACKET = 0x1B,
	KEY_RETURN = 0x1C,
	KEY_LCTRL = 0x1D,
	KEY_A = 0x1E,
	KEY_S = 0x1F,
	KEY_D = 0x20,
	KEY_F = 0x21,
	KEY_G = 0x22,
	KEY_H = 0x23,
	KEY_J = 0x24,
	KEY_K = 0x25,
	KEY_L = 0x26,
	KEY_SEMICOLON = 0x27,
	KEY_GRAVE = 0x29,
	KEY_APOSTROPHE = 0x28,
	KEY_LSHIFT = 0x2A,
	KEY_BACKKSLASH = 0x2B,
	KEY_Z = 0x2C,
	KEY_X = 0x2D,
	KEY_C = 0x2E,
	KEY_V = 0x2F,
	KEY_B = 0x30,
	KEY_N = 0x31,
	KEY_M = 0x32,
	KEY_COMMA = 0x33,
	KEY_PERIOD = 0x34,
	KEY_SLASH = 0x35,
	KEY_RSHIFT = 0x36,
	KEY_SPACE = 0x39,
	KEY_CAPS = 0x3A,
	KEY_F1 = 0x3B,
	KEY_F2 = 0x3C,
	KEY_F3 = 0x3D,
	KEY_F4 = 0x3E,
	KEY_F5 = 0x3F,
	KEY_F6 = 0x40,
	KEY_F7 = 0x41,
	KEY_F8 = 0x42,
	KEY_F9 = 0x43,
	KEY_F10 = 0x44,
	KEY_F11 = 0x57,
	KEY_F12 = 0x58,
	KEY_UP = 0xC8,
	KEY_LEFT = 0xCB,
	KEY_RIGHT = 0xCD,
	KEY_DOWN = 0xD0
};

//debating if these initialization functions should be in 
//os_support or not, but I don't want to expose the InputState struct
//in a header. 
void os_initializeInput();
void os_pollInput();
void os_shutdownInput();

bool getKey(KeyCode key);
int getMouseDX();
int getMouseDY();
int getMouseX();
int getMouseY();
int getMouseLeftButton();
int getMouseRightButton();