#include "stdafx.h"
#include <SADXModLoader.h>
#include <Trampoline.h>

#define EXPORT __declspec(dllexport)

// MAINMEMORY PLS
//DataPointer(NJS_MATRIX, TransformationMatrix, 0x03D0FD80);
DataPointer(float, TransformationMatrix, 0x03D0FD80);
DataPointer(float, BaseTransformationMatrix, 0x0389D318);

void __cdecl ajfskdfhsadf(NJS_SPRITE *a1, int n, NJD_SPRITE attr, float a7);
Trampoline Draw3DSpriteThing(0x0077E390, 0x0077E398, (DetourFunction)ajfskdfhsadf);
void __cdecl ajfskdfhsadf(NJS_SPRITE *a1, int n, NJD_SPRITE attr, float a7)
{
	NJS_POINT3 old_pos = a1->p;
	NJS_POINT3* p = &a1->p;

	//p->x = -p->x + HorizontalResolution;

	FunctionPointer(void, original, (NJS_SPRITE *a1, int n, NJD_SPRITE attr, float a7), Draw3DSpriteThing.Target());
	original(a1, n, attr, a7);

	a1->p = old_pos;
}

extern "C"
{
	EXPORT ModInfo SADXModInfo = { ModLoaderVer };
	EXPORT void __cdecl Init()
	{
		NJS_MATRIX* m = (NJS_MATRIX*)&BaseTransformationMatrix;
		*m[M00] = -1.0f;
	}

	EXPORT void __cdecl OnInput()
	{
		for (Uint32 i = 0; i < 8; i++)
		{
			ControllerData* pad = ControllerPointers[i];
			if (pad == nullptr)
				continue;

			pad->LeftStickX = -pad->LeftStickX;
			pad->RightStickX = -pad->RightStickX;

			auto lt = pad->LTriggerPressure;
			auto rt = pad->RTriggerPressure;
			pad->LTriggerPressure = rt;
			pad->RTriggerPressure = lt;
		}
	}
}
