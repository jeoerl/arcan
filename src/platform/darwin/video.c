/*
 * Copyright 2014-2015, Björn Ståhl
 * License: 3-Clause BSD, see COPYING file in arcan source repository.
 * Reference: http://arcan-fe.com
 */

/*
 * PLATFORM DRIVER NOTICE:
 * This platform driver is incomplete in the sense that it was only set
 * up in order to allow for headless LWA/hijack/retro3d.
 *
 * Note that we can probably get decent working buffer passing without
 * readbacks here using IOSurface, see the serverOpenGLView.m
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>

#include "arcan_math.h"
#include "arcan_general.h"
#include "arcan_video.h"
#include "arcan_videoint.h"

#ifndef PLATFORM_SUFFIX
#define PLATFORM_SUFFIX platform
#endif

#define MERGE(X,Y) X ## Y
#define EVAL(X,Y) MERGE(X,Y)
#define PLATFORM_SYMBOL(fun) EVAL(PLATFORM_SUFFIX, fun)

#include <OpenGL/OpenGL.h>
#include <OpenGL/GL.h>

static char* darwin_synchopts[] = {
	"default", "driver- specific GL swap",
	NULL
};

static char* darwin_envopts[] = {
	NULL
};

static struct {
	size_t mdispw, mdisph;
	size_t canvasw, canvash;
} darwin;

static CGLContextObj context;

bool PLATFORM_SYMBOL(_video_init)(uint16_t w, uint16_t h,
	uint8_t bpp, bool fs, bool frames, const char* cap)
{
	CGLPixelFormatObj pix;
	CGLError errorCode;
	GLint num;

	CGLPixelFormatAttribute attributes[4] = {
  	kCGLPFAAccelerated,
  	kCGLPFAOpenGLProfile,
  	(CGLPixelFormatAttribute) kCGLOGLPVersion_Legacy,
  	(CGLPixelFormatAttribute) 0
	};

	errorCode = CGLChoosePixelFormat( attributes, &pix, &num );
  errorCode = CGLCreateContext( pix, NULL, &context );
	CGLDestroyPixelFormat( pix );
  errorCode = CGLSetCurrentContext( context );

/*
 * no double buffering,
 * we let the parent transfer process act as the limiting clock.
 */
	GLint si = 0;
	CGLSetParameter(context, kCGLCPSwapInterval, &si);

	darwin.mdispw = darwin.canvasw = w;
	darwin.mdisph = darwin.canvash = h;

	return true;
}

void PLATFORM_SYMBOL(_video_synch)(uint64_t tick_count, float fract,
	video_synchevent pre, video_synchevent post)
{
	if (pre)
		pre();

#ifndef HEADLESS_NOARCAN
	size_t nd;
	arcan_bench_register_cost( arcan_vint_refresh(fract, &nd) );

	agp_activate_rendertarget(NULL);

	arcan_vint_drawrt(arcan_vint_world(), 0, 0,
		darwin.mdispw, darwin.mdisph);
	arcan_vint_drawcursor(false);
#endif

	glFlush();

	if (post)
		post();
}

struct monitor_mode* PLATFORM_SYMBOL(_video_query_modes)(
	platform_display_id id, size_t* count)
{
	static struct monitor_mode mode = {};

	mode.width  = darwin.canvasw;
	mode.height = darwin.canvash;
	mode.depth  = sizeof(av_pixel) * 8;
	mode.refresh = 60; /* should be queried */
	*count = 1;

	return &mode;
}

struct monitor_mode PLATFORM_SYMBOL(_video_dimensions)()
{
	struct monitor_mode res = {
		.width = darwin.canvasw,
		.height = darwin.canvash,
		.phy_width = darwin.mdispw,
		.phy_height = darwin.mdisph
	};
	return res;
}

bool PLATFORM_SYMBOL(_video_map_handle)(
	struct storage_info_t* dst, int64_t handle)
{
	return false;
}

int64_t PLATFORM_SYMBOL(_output_handle)(
	struct storage_info_t* store, enum status_handle* status)
{
	*status = ERROR_UNSUPPORTED;
	return -1;
}

bool PLATFORM_SYMBOL(_video_specify_mode)(platform_display_id id,
	struct monitor_mode mode)
{
	return false;
}

const char** PLATFORM_SYMBOL(_video_synchopts)(void)
{
	return (const char**) darwin_synchopts;
}

const char** PLATFORM_SYMBOL(_video_envopts)(void)
{
	return (const char**) darwin_envopts;
}

void PLATFORM_SYMBOL(_video_setsynch)(const char* arg)
{
	arcan_warning("unhandled synchronization strategy (%s) ignored.\n", arg);
}

void PLATFORM_SYMBOL(_video_prepare_external) () {}
void PLATFORM_SYMBOL(_video_restore_external) () {}

void PLATFORM_SYMBOL(_video_shutdown) ()
{
}

void* PLATFORM_SYMBOL(_video_gfxsym)(const char* sym)
{
	static void* dlh = NULL;
  if (NULL == dlh)
    dlh = dlopen("/System/Library/Frameworks/"
			"OpenGL.framework/Versions/Current/OpenGL", RTLD_LAZY);

  return dlh ? dlsym(dlh, sym) : NULL;
}

const char* PLATFORM_SYMBOL(_video_capstr)(void)
{
	static char* capstr;

	if (!capstr){
		const char* vendor = (const char*) glGetString(GL_VENDOR);
		const char* render = (const char*) glGetString(GL_RENDERER);
		const char* version = (const char*) glGetString(GL_VERSION);
		const char* shading = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
		const char* exts = (const char*) glGetString(GL_EXTENSIONS);

		size_t interim_sz = 64 * 1024;
		char* interim = malloc(interim_sz);
		size_t nw = snprintf(interim, interim_sz, "Video Platform (LWA-Darwin)\n"
			"Vendor: %s\nRenderer: %s\nGL Version: %s\n"
			"GLSL Version: %s\n\n Extensions Supported: \n%s\n\n",
			vendor, render, version, shading, exts
		) + 1;

		if (nw < (interim_sz >> 1)){
			capstr = malloc(nw);
			memcpy(capstr, interim, nw);
			free(interim);
		}
		else
			capstr = interim;
	}

	return capstr;
}

void PLATFORM_SYMBOL(_video_minimize) () {}

