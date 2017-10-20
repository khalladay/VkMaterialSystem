#pragma once

//I would love to get rid of glm in the future so
//we don't have to worry about including this here and can
//just forward declare what we need
#define GLM_FORCE_RADIANS
#define GLM_FORECE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <cstdint>
#include <atlstr.h>

#if _DEBUG
#define NDEBUG 1

#define checkf(expr, format, ...) if (!(expr))																\
{																											\
    fprintf(stdout, "CHECK FAILED: %s:%d:%s " format "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__);	\
    CString dbgstr;																							\
    dbgstr.Format(("%s"), format);																			\
    MessageBox(NULL,dbgstr, "FATAL ERROR", MB_OK);															\
	DebugBreak();																							\
}
#else
#undef NDEBUG
#define checkf(expr, format, ...) ;
#endif

#define APP_NAME "VkMaterialSystem"
#define INITIAL_SCREEN_W 800
#define INITIAL_SCREEN_H 600

#include "array.h"