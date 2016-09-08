#include "stdafx.h"

#define EXPORT __declspec(dllexport)

// basically NJS_MATRIX
DataArray(float, TransformationMatrix, 0x03D0FD80, 16);
DataArray(float, BaseTransformationMatrix, 0x0389D318, 16);

DataPointer(float, ViewPortWidth_Half, 0x03D0FA0C);
DataPointer(float, ViewPortHeight_Half, 0x03D0FA10);
DataPointer(Bool, TransformAndViewportInvalid, 0x03D0FD1C);

static bool mirror_x = true;
static bool mirror_y = false;

static void __cdecl njDrawSprite3D_3_r(NJS_SPRITE *a1, int n, NJD_SPRITE attr, float a7);
static Trampoline njDrawSprite3D_3_trampoline(0x0077E390, 0x0077E398, njDrawSprite3D_3_r);
static void __cdecl njDrawSprite3D_3_r(NJS_SPRITE *a1, int n, NJD_SPRITE attr, float a7)
{
	FunctionPointer(void, original, (NJS_SPRITE *a1, int n, NJD_SPRITE attr, float a7),
		njDrawSprite3D_3_trampoline.Target());

	if (mirror_x)
	{
		attr = (attr & NJD_SPRITE_HFLIP) ? attr & ~NJD_SPRITE_HFLIP : attr | NJD_SPRITE_HFLIP;
	}
	if (mirror_y)
	{
		attr = (attr & NJD_SPRITE_VFLIP) ? attr & ~NJD_SPRITE_VFLIP : attr | NJD_SPRITE_VFLIP;
	}

	original(a1, n, attr, a7);
}

static void __stdcall sprite_flip_c(NJS_VECTOR* v)
{
	auto v8 = _nj_screen_.dist / v->z;

	v->x = v->x * v8 + ViewPortWidth_Half;
	v->y = v->y * v8 + ViewPortHeight_Half;

	if (mirror_x)
	{
		v->x = (float)HorizontalResolution - v->x;
	}

	if (mirror_y)
	{
		v->y = (float)VerticalResolution - v->y;
	}
}

constexpr auto sprite_flip_retn = (void*)0x0077E46E;
static void __declspec(naked) sprite_flip()
{
	__asm
	{
		lea     ecx, [esp + 30h]
		push    ecx
		call    sprite_flip_c

		mov     ecx, dword ptr [_nj_screen_.dist]
		fld     dword ptr [ecx]
		fdiv    dword ptr [esp + 38h]

		jmp     sprite_flip_retn
	}
}

extern "C"
{
	EXPORT ModInfo SADXModInfo = { ModLoaderVer };
	EXPORT void __cdecl Init()
	{
		WriteJump((void*)0x0077E444, sprite_flip);
		BaseTransformationMatrix[M00] *= -1.0f;
	}

	EXPORT void __cdecl OnInput()
	{
		for (Uint32 i = 0; i < 8; i++)
		{
			ControllerData* pad = ControllerPointers[i];
			if (pad == nullptr)
				continue;

			if (pad->PressedButtons & Buttons_D)
			{
				mirror_x = !mirror_x;
				BaseTransformationMatrix[M00] *= -1.0f;
				TransformAndViewportInvalid = 1;
			}
			if (pad->PressedButtons & Buttons_C)
			{
				mirror_y = !mirror_y;
				BaseTransformationMatrix[M11] *= -1.0f;
				TransformAndViewportInvalid = 1;
			}

			if (!mirror_x)
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
