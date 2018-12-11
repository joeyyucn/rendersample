#ifndef RENDER_BACKEND_API_INCLUDED
#define RENDER_BACKEND_API_INCLUDED
#include "render_cmds.h"

extern const BackEndData* g_backendData;

void RB_RenderThread( int context );
void RB_RenderThreadCircle();
void RB_PumpMessageQueue();

#endif