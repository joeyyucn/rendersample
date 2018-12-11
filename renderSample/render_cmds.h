#ifndef RENDER_CMDS_INLCUDED
#define RENDER_CMDS_INLCUDED
struct BackEndData
{
	int fakeData;
};

void R_BeginFrame();
void R_EndFrame();
void R_ToggleFrame();
void R_HandOffToBackend();

#endif