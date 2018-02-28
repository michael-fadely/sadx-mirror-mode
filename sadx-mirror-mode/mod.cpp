#include "stdafx.h"

#define EXPORT __declspec(dllexport)

#pragma pack(push, 1)

struct __declspec(align(2)) PolyBuff_RenderArgs
{
	Uint32 StartVertex;
	Uint32 PrimitiveCount;
	Uint32 CullMode;
	Uint32 d;
};

static_assert(sizeof(PolyBuff_RenderArgs) == 0x10, "PolyBuff_RenderArgs size is incorrect");

struct PolyBuff
{
	void *pStreamData; // IDirect3DVertexBuffer8
	Uint32 TotalSize;
	Uint32 CurrentSize;
	Uint32 Stride;
	Uint32 FVF;
	PolyBuff_RenderArgs *RenderArgs;
	Uint32 LockCount;
	const char *name;
	int i;
};

static_assert(sizeof(PolyBuff) == 0x24, "PolyBuff size is incorrect");

#pragma pack(pop)

enum MirrorDirection : Uint8
{
	none,
	mirror_x  = 1 << 0,
	mirror_y  = 1 << 1,
	mirror_xy = mirror_x | mirror_y
};

static bool  applied_chao_fix = false;
static Uint8 last_mirror_mode = none;
static Uint8 mirror_mode      = none;

static void __fastcall PolyBuff_DrawTriangleStrip_r(PolyBuff *_this);
static void __fastcall PolyBuff_DrawTriangleList_r(PolyBuff *_this);
static void __fastcall njProjectScreen_r(NJS_MATRIX *m, NJS_VECTOR *p3, NJS_POINT2 *p2);
static char __cdecl GetPauseDisplayOptions_r(Uint8 *a1);
static void __cdecl njDrawSprite3D_DrawNow_r(NJS_SPRITE *a1, int n, NJD_SPRITE attr, float a7);
static void TornadoTarget_MoveTargetWithinBounds_r();

static Trampoline PolyBuff_DrawTriangleStrip_t(0x00794760, 0x00794767, PolyBuff_DrawTriangleStrip_r);
static Trampoline PolyBuff_DrawTriangleList_t(0x007947B0, 0x007947B7, PolyBuff_DrawTriangleList_r);
static Trampoline njProjectScreen_t(0x00788700, 0x00788705, njProjectScreen_r);
static Trampoline GetPauseDisplayOptions_t(0x004582E0, 0x004582E8, GetPauseDisplayOptions_r);
static Trampoline njDrawSprite3D_DrawNow_t(0x0077E390, 0x0077E398, njDrawSprite3D_DrawNow_r);
static Trampoline TornadoTarget_MoveTargetWithinBounds_t(0x00628970, 0x00628978, TornadoTarget_MoveTargetWithinBounds_r);

inline bool is_mirrored()
{
	return mirror_mode != none && mirror_mode != mirror_xy;
}

// Toggles the provided mirror directions and applies the Chao render fix (more info below).
static void toggle_mirror(Uint8 direction)
{
	if (direction & mirror_x)
	{
		mirror_mode ^= mirror_x;
		BaseTransformationMatrix.m[0][0] *= -1.0f;
		TransformAndViewportInvalid = true;
	}

	if (direction & mirror_y)
	{
		mirror_mode ^= mirror_y;
		BaseTransformationMatrix.m[1][1] *= -1.0f;
		TransformAndViewportInvalid = true;
	}

	if (!TransformAndViewportInvalid)
	{
		return;
	}

	// The model used for the Chao are triangle strips, so if the
	// transformation matrix is only flipped in one direction,
	// the culling modes for the triangle strips must be swapped.
	// It uses separate culling modes depending on which way the faces
	// are pointing (forward or backward).

	bool mirrored = is_mirrored();
	const auto clockwise = static_cast<int>(mirrored ? D3DCULL_CCW : D3DCULL_CW);
	const auto counter_clockwise = static_cast<int>(mirrored ? D3DCULL_CW : D3DCULL_CCW);

	WriteData(reinterpret_cast<int*>(0x00795BD3), counter_clockwise);
	WriteData(reinterpret_cast<int*>(0x00795BDE), clockwise);

	WriteData(reinterpret_cast<int*>(0x00795CD3), counter_clockwise);
	WriteData(reinterpret_cast<int*>(0x00795CDE), clockwise);

	WriteData(reinterpret_cast<int*>(0x00795DF3), counter_clockwise);
	WriteData(reinterpret_cast<int*>(0x00795DFE), clockwise);

	WriteData(reinterpret_cast<int*>(0x00795F28), counter_clockwise);
	WriteData(reinterpret_cast<int*>(0x00795F33), clockwise);

	WriteData(reinterpret_cast<int*>(0x00796068), counter_clockwise);
	WriteData(reinterpret_cast<int*>(0x00796073), clockwise);

	WriteData(reinterpret_cast<int*>(0x007961BB), clockwise);
	WriteData(reinterpret_cast<int*>(0x007961CE), counter_clockwise);

	WriteData(reinterpret_cast<int*>(0x00796328), counter_clockwise);
	WriteData(reinterpret_cast<int*>(0x00796333), clockwise);

	WriteData(reinterpret_cast<int*>(0x00796483), counter_clockwise);
	WriteData(reinterpret_cast<int*>(0x0079648E), clockwise);
}

static void __fastcall PolyBuff_DrawTriangleStrip_r(PolyBuff *_this)
{
	auto original = reinterpret_cast<decltype(PolyBuff_DrawTriangleStrip_r)*>(PolyBuff_DrawTriangleStrip_t.Target());

	if (!mirror_mode || mirror_mode == mirror_xy || _this->RenderArgs->CullMode == D3DCULL_NONE)
	{
		original(_this);
		return;
	}

	PolyBuff_RenderArgs* args = _this->RenderArgs;
	Uint32 last = args->CullMode;

	if (args->CullMode == D3DCULL_CW)
	{
		args->CullMode = D3DCULL_CCW;
	}
	else if (args->CullMode == D3DCULL_CCW)
	{
		args->CullMode = D3DCULL_CW;
	}

	original(_this);
	args->CullMode = last;
}

static void __fastcall PolyBuff_DrawTriangleList_r(PolyBuff *_this)
{
	auto original = reinterpret_cast<decltype(PolyBuff_DrawTriangleList_r)*>(PolyBuff_DrawTriangleList_t.Target());

	if (!mirror_mode || mirror_mode == mirror_xy || _this->RenderArgs->CullMode == D3DCULL_NONE)
	{
		original(_this);
		return;
	}

	PolyBuff_RenderArgs* args = _this->RenderArgs;
	Uint32 last = args->CullMode;

	if (args->CullMode == D3DCULL_CW)
	{
		args->CullMode = D3DCULL_CCW;
	}
	else if (args->CullMode == D3DCULL_CCW)
	{
		args->CullMode = D3DCULL_CW;
	}

	original(_this);
	args->CullMode = last;
}

// Overrides njProjectScreen and flips the screen space coordinates before returning.
static void __fastcall njProjectScreen_r(NJS_MATRIX *m, NJS_VECTOR *p3, NJS_POINT2 *p2)
{
	auto original = reinterpret_cast<decltype(njProjectScreen_r)*>(njProjectScreen_t.Target());
	original(m, p3, p2);

	if (!is_mirrored())
	{
		return;
	}

	if (mirror_mode & mirror_x)
	{
		p2->x = static_cast<float>(HorizontalResolution) - p2->x;
	}

	if (mirror_mode & mirror_y)
	{
		p2->y = static_cast<float>(VerticalResolution) - p2->y;
	}
}

// Removes the map option from the pause menu because
// it's wrong when mirrored and nobody uses it anyway.
static char __cdecl GetPauseDisplayOptions_r(Uint8 *a1)
{
	auto original = reinterpret_cast<decltype(GetPauseDisplayOptions_r)*>(GetPauseDisplayOptions_t.Target());
	auto result = original(a1);

	if (*a1 & PauseOptions_Map)
	{
		*a1 &= ~PauseOptions_Map;
		--result;
	}

	return result;
}

// Used to check if any sprites in 3D space need their sprite flipped (not cooridnates).
static void __cdecl njDrawSprite3D_DrawNow_r(NJS_SPRITE *a1, int n, NJD_SPRITE attr, float a7)
{
	auto original = reinterpret_cast<decltype(njDrawSprite3D_DrawNow_r)*>(njDrawSprite3D_DrawNow_t.Target());

	// Flips the combo count on Gamma's targets only
	if (a1 && a1->tlist == reinterpret_cast<NJS_TEXLIST*>(0x0091D5E0))
	{
		if (mirror_mode & mirror_x)
		{
			attr = (attr & NJD_SPRITE_HFLIP) ? attr & ~NJD_SPRITE_HFLIP : attr | NJD_SPRITE_HFLIP;
		}

		if (mirror_mode & mirror_y)
		{
			attr = (attr & NJD_SPRITE_VFLIP) ? attr & ~NJD_SPRITE_VFLIP : attr | NJD_SPRITE_VFLIP;
		}
	}

	original(a1, n, attr, a7);
}

static void TornadoTarget_MoveTargetWithinBounds_c(ObjectMaster *a1)
{
	auto original = TornadoTarget_MoveTargetWithinBounds_t.Target();

	// we need to un-flip the input so that the cursor moves in the right direction
	ControllerData* real = ControllerPointers[0];
	ControllerData* fixed = &Controllers[0];

	if (is_mirrored() && real != nullptr)
	{
		real->LeftStickX = -real->LeftStickX;
		fixed->LeftStickX = -fixed->LeftStickX;
	}

	__asm
	{
		mov eax, a1
		call original
	}

	if (is_mirrored() && real != nullptr)
	{
		real->LeftStickX = -real->LeftStickX;
		fixed->LeftStickX = -fixed->LeftStickX;
	}
}

static void __declspec(naked) TornadoTarget_MoveTargetWithinBounds_r()
{
	__asm
	{
		push eax
		call TornadoTarget_MoveTargetWithinBounds_c
		pop eax
		retn
	}
}

// Intercepts the code that sets the screen space coordinates for 3D sprites
// (projected from 3D space to screen space) and flips the coordinates.
static void __stdcall flip_3d_sprites_c(NJS_VECTOR* v)
{
	const float v8 = _nj_screen_.dist / v->z;

	v->x = v->x * v8 + ViewPortWidth_Half;
	v->y = v->y * v8 + ViewPortHeight_Half;

	if (mirror_mode & mirror_x)
	{
		v->x = static_cast<float>(HorizontalResolution) - v->x;
	}

	if (mirror_mode & mirror_y)
	{
		v->y = static_cast<float>(VerticalResolution) - v->y;
	}
}

static const auto loc_77E46E = reinterpret_cast<void*>(0x0077E46E);

static void __declspec(naked) flip_3d_sprites()
{
	__asm
	{
		lea     ecx, [esp + 30h]
		push    ecx
		call    flip_3d_sprites_c

		mov     ecx, dword ptr [_nj_screen_.dist]
		fld     dword ptr [ecx]
		fdiv    dword ptr [esp + 38h]

		jmp     loc_77E46E
	}
}

// Disables mirroring in the Chao Race Entrance, Chao Race, and Black Market. Otherwise, enables mirroring.
static bool check_mirror_state()
{
	if (!applied_chao_fix)
	{
		if (!is_mirrored() || !IsChaoStage)
		{
			return false;
		}

		auto current = GetCurrentChaoStage();

		applied_chao_fix = current == SADXChaoStage_RaceEntry
			|| current == SADXChaoStage_BlackMarket
			|| current == SADXChaoStage_Race;

		if (applied_chao_fix)
		{
			last_mirror_mode = mirror_mode;
			toggle_mirror(mirror_mode);
		}
	}
	else
	{
		SADXChaoStage current = GetCurrentChaoStage();

		if (!IsChaoStage
			|| (current != SADXChaoStage_RaceEntry && current != SADXChaoStage_BlackMarket && current != SADXChaoStage_Race))
		{
			toggle_mirror(last_mirror_mode);
			applied_chao_fix = false;
		}
	}

	return applied_chao_fix;
}

static constexpr uint32_t TRIGGER_MASK = Buttons_L | Buttons_R;
inline void swap_triggers(uint32_t& buttons)
{
	if (!(buttons & TRIGGER_MASK) || (buttons & TRIGGER_MASK) == TRIGGER_MASK)
	{
		return;
	}

	if (buttons & Buttons_L)
	{
		buttons = (buttons & ~Buttons_L) | Buttons_R;
	}
	else if (buttons & Buttons_R)
	{
		buttons = (buttons & ~Buttons_R) | Buttons_L;
	}
}

template <typename T>
static void swap_things(T& a, T& b)
{
	auto temp = a;

	a = b;
	b = temp;
}

static void flip_uv(NJS_OBJECT* object)
{
	if (object->child)
	{
		flip_uv(object->child);
	}

	if (object->sibling)
	{
		flip_uv(object->sibling);
	}

	NJS_MODEL_SADX* model = object->getbasicdxmodel();

	if (model == nullptr)
	{
		throw;
	}

	auto meshsets = model->meshsets;

	if (meshsets == nullptr)
	{
		throw;
	}

	for (size_t i = 0; i < model->nbMeshset; i++)
	{
		const NJS_MESHSET_SADX& meshset = meshsets[i];
		NJS_TEX* vertuv = meshset.vertuv;

		if (vertuv == nullptr)
		{
			throw;
		}
		
		size_t uv_count = 0;

		switch (meshset.type_matId & NJD_MESHSET_MASK)
		{
			case NJD_MESHSET_TRIMESH:
			case NJD_MESHSET_N:
			{
				size_t index = 0;
				for (size_t j = 0; j < meshset.nbMesh; j++)
				{
					const auto& n = meshset.meshes[index++];
					uv_count += n;
					index += n;
				}
				break;
			}

			case NJD_MESHSET_3:
				uv_count = meshset.nbMesh * 3;
				break;

			case NJD_MESHSET_4:
				uv_count = meshset.nbMesh * 4;
				break;

			default:
				throw;
		}

		for (size_t j = 0; j < uv_count; j++)
		{
			NJS_TEX& uv = vertuv[j];
			swap_things(uv.u, uv.v);
		}
	}
}

extern "C"
{
	EXPORT ModInfo SADXModInfo = { ModLoaderVer, nullptr, nullptr, 0, nullptr, 0 , nullptr, 0, nullptr, 0 };

	EXPORT void __cdecl Init()
	{
		WriteJump(reinterpret_cast<void*>(0x0077E444), flip_3d_sprites);
		toggle_mirror(mirror_x);
		flip_uv(&JumpPanelDigit_OBJECT);
	}

	EXPORT void __cdecl OnInput()
	{
		if (check_mirror_state())
		{
			return;
		}

		for (Uint32 i = 0; i < 8; i++)
		{
			ControllerData* pad = ControllerPointers[i];
			
			if (pad == nullptr)
			{
				continue;
			}

#ifdef _DEBUG
			auto pressed = pad->PressedButtons;

			if (pressed & Buttons_D)
			{
				toggle_mirror(mirror_x);
			}

			if (pressed & Buttons_C)
			{
				toggle_mirror(mirror_y);
			}
#endif

			// We want to skip axis flipping if we're on a menu or the game is paused.
			if (!(mirror_mode & mirror_x) || GameState == 21 || GameState == 16)
			{
				continue;
			}

			// This flips the X axis of both sticks and swaps the triggers.

			pad->LeftStickX = -pad->LeftStickX;
			pad->RightStickX = -pad->RightStickX;

			auto lt = pad->LTriggerPressure;
			auto rt = pad->RTriggerPressure;

			pad->LTriggerPressure = rt;
			pad->RTriggerPressure = lt;

			swap_triggers(pad->HeldButtons);

			auto not = ~pad->NotHeldButtons;
			swap_triggers(not);
			pad->NotHeldButtons = ~not;

			swap_triggers(pad->PressedButtons);
			swap_triggers(pad->ReleasedButtons);
			swap_triggers(pad->Old);
		}
	}
}
