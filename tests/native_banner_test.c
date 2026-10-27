#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <common.h>

struct sData sdata_static;
struct Data data;

int gNative60FpsEnabled = 0;
int gNativeForce30Fps = 0;
int gNativeGhostReplayFpsOverride = -1;
u8 *gCTRNativeScratchpadBase = NULL;

// Scratchpad buffer for banner test
static u8 s_testScratchpad[1024];
u8 *Platform_GetScratchpad(void)
{
	return s_testScratchpad;
}

// Dummy PROC_BirthWithObject for RB_Banner_LInB linkage
struct Thread *PROC_BirthWithObject(int flags, void *funcThTick, const char *name, struct Thread *relativeTh)
{
	(void)flags;
	(void)funcThTick;
	(void)name;
	(void)relativeTh;
	return NULL;
}

int MATH_Sin(u32 angle)
{
	(void)angle;
	return 0;
}

// Include RB_Banner directly
#include "../game/231/RB_Banner.c"

static void test_banner_animate_play_null_safety(void)
{
	// 1. Completely NULL mh
	RB_Banner_Animate_Play(NULL, 10);
	RB_Banner_Animate_Play(NULL, 0);
	RB_Banner_Animate_Play(NULL, -1);

	// 2. ModelHeader with NULL ptrColors
	struct ModelHeader mh;
	memset(&mh, 0, sizeof(mh));
	RB_Banner_Animate_Play(&mh, 10);

	// 3. ModelHeader with NULL ptrFrameData
	u32 dummyColors[0x40];
	memset(dummyColors, 0x55, sizeof(dummyColors));
	mh.ptrColors = dummyColors;
	mh.ptrFrameData = NULL;
	RB_Banner_Animate_Play(&mh, 10);

	puts("test_banner_animate_play_null_safety: PASSED");
}

static void test_banner_animate_play_execution(void)
{
	struct ModelHeader mh;
	memset(&mh, 0, sizeof(mh));

	u32 colors[0x40];
	for (int i = 0; i < 0x40; i++)
	{
		colors[i] = (u32)(i + 1);
	}
	mh.ptrColors = colors;

	struct {
		struct ModelFrame frame;
		u8 vertices[30 * 3];
	} frameBuffer;
	memset(&frameBuffer, 0, sizeof(frameBuffer));
	frameBuffer.frame.vertexOffset = sizeof(struct ModelFrame);
	for (int i = 0; i < 30; i++)
	{
		frameBuffer.vertices[i * 3 + 0] = (u8)(i * 4); // x
		frameBuffer.vertices[i * 3 + 1] = 0x80;        // y
		frameBuffer.vertices[i * 3 + 2] = 0x00;        // z
	}
	mh.ptrFrameData = &frameBuffer.frame;

	// Execute animation
	u32 firstColorBefore = colors[0];
	u32 secondColorBefore = colors[1];
	RB_Banner_Animate_Play(&mh, 30);

	// Colors should have rotated by 1
	assert(colors[0] == secondColorBefore);
	assert(colors[0x3f] == firstColorBefore);

	puts("test_banner_animate_play_execution: PASSED");
}

static void test_banner_thtick_null_safety_and_fallback(void)
{
	struct GameTracker gt;
	memset(&gt, 0, sizeof(gt));
	sdata->gGT = &gt;

	// 1. Thread is NULL
	RB_Banner_ThTick(NULL);

	// 2. Thread has NULL object
	struct Thread t;
	memset(&t, 0, sizeof(t));
	RB_Banner_ThTick(&t);

	// 3. Thread has object with numVertices == 0
	struct StartBanner banner;
	memset(&banner, 0, sizeof(banner));
	t.object = &banner;
	RB_Banner_ThTick(&t);

	// 4. numVertices > 0 but inst is NULL
	banner.numVertices = 10;
	t.inst = NULL;
	RB_Banner_ThTick(&t);

	// 5. inst exists but inst->model is NULL and gGT->modelPtr[STATIC_STARTBANNERWAVE] is NULL
	struct Instance inst;
	memset(&inst, 0, sizeof(inst));
	t.inst = &inst;
	RB_Banner_ThTick(&t);

	// 6. inst->model is NULL, but gGT->modelPtr has the banner model with NULL headers
	struct Model model;
	memset(&model, 0, sizeof(model));
	gt.modelPtr[STATIC_STARTBANNERWAVE] = &model;
	RB_Banner_ThTick(&t);
	// It should have re-bound inst->model to model
	assert(inst.model == &model);

	// 7. model has valid headers and colors
	struct ModelHeader mh;
	memset(&mh, 0, sizeof(mh));
	u32 colors[0x40];
	memset(colors, 0x11, sizeof(colors));
	mh.ptrColors = colors;
	model.headers = &mh;

	// Ticking should safely run
	RB_Banner_ThTick(&t);

	puts("test_banner_thtick_null_safety_and_fallback: PASSED");
}

static void test_banner_animate_init_null_safety(void)
{
	assert(RB_Banner_Animate_Init(NULL) == 0);

	struct ModelHeader mh;
	memset(&mh, 0, sizeof(mh));
	assert(RB_Banner_Animate_Init(&mh) == 0);

	puts("test_banner_animate_init_null_safety: PASSED");
}

int main(void)
{
	gCTRNativeScratchpadBase = s_testScratchpad;

	test_banner_animate_play_null_safety();
	test_banner_animate_play_execution();
	test_banner_thtick_null_safety_and_fallback();
	test_banner_animate_init_null_safety();

	puts("\n>>> ALL NATIVE BANNER TESTS PASSED! <<<");
	return 0;
}
