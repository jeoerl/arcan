/*
 * Copyright 2003-2014, Björn Ståhl
 * License: 3-Clause BSD, see COPYING file in arcan source repository.
 * Reference: http://arcan-fe.com
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

#include <tchar.h>

#include <SDL.h>
#include <SDL_syswm.h>
#include <SDL_mutex.h>
#include <SDL_types.h>

#include <arcan_math.h>
#include <arcan_shmif.h>
#include <arcan_general.h>
#include <arcan_event.h>
#include <arcan_video.h>
#include <arcan_audio.h>
#include <arcan_frameserver.h>

/*
 * refactor needs:
 * should be able to get rid of SDL here
 */

sem_handle async, vsync, esync;
HANDLE parent;

static BOOL SafeTerminateProcess(HANDLE hProcess, UINT* uExitCode);

void arcan_frameserver_killchild(arcan_frameserver* src)
{
	if (!src || src->parent != ARCAN_EID || !src->child)
		return;

	UINT ec;

	SafeTerminateProcess(src->child, &ec);
	src->flags.alive = false;
	src->child = NULL;
}

/* Dr.Dobb, anno 99, because a working signaling system and kill is too clean */
static BOOL SafeTerminateProcess(HANDLE hProcess, UINT* uExitCode)
{
	DWORD dwTID, dwCode, dwErr = 0;
	HANDLE hProcessDup = INVALID_HANDLE_VALUE;
	HANDLE hRT = NULL;
	HINSTANCE hKernel = GetModuleHandle("Kernel32");
	BOOL bSuccess = FALSE;
	BOOL bDup = DuplicateHandle(GetCurrentProcess(),
		hProcess,
		GetCurrentProcess(),
		&hProcessDup,
		PROCESS_ALL_ACCESS,
		FALSE,
		0);

	if (GetExitCodeProcess((bDup) ? hProcessDup :
		 hProcess, &dwCode) && (dwCode == STILL_ACTIVE)) {
		FARPROC pfnExitProc;
		pfnExitProc = GetProcAddress(hKernel, "exit");
		hRT = CreateRemoteThread((bDup) ? hProcessDup : hProcess,
			NULL,
			0,
			(LPTHREAD_START_ROUTINE)pfnExitProc,
			(PVOID)uExitCode, 0, &dwTID);

		if (hRT == NULL)
			dwErr = GetLastError();
	}
	else {
		dwErr = ERROR_PROCESS_ABORTED;
	}
	if (hRT) {
		CloseHandle(hRT);
/*
 (created big stalls, assume target plays somewhat nice)
   	WaitForSingleObject((bDup) ? hProcessDup : hProcess, INFINITE);
		CloseHandle(hRT);
		bSuccess = TRUE;
*/
	}

	if (bDup)
		CloseHandle(hProcessDup);

	if (!bSuccess)
		SetLastError(dwErr);

	return bSuccess;
}

bool arcan_frameserver_validchild(arcan_frameserver* src)
{
	if (!src || !src->flags.alive || !src->child)
		return false;

	DWORD exitcode;
	GetExitCodeProcess((HANDLE) src->child, &exitcode);

	return exitcode == STILL_ACTIVE;
}

void arcan_frameserver_dropshared(arcan_frameserver* src)
{
	arcan_frameserver_dropsemaphores(src);
	struct arcan_shmif_page* shmpage =
		(struct arcan_shmif_page*) src->shm.ptr;

	if (src->shm.ptr && false == UnmapViewOfFile((void*) shmpage))
		arcan_warning("BUG -- arcan_frameserver_free()"
			", munmap failed: %s\n", strerror(errno));

	CloseHandle(src->async);
	CloseHandle(src->vsync);
	CloseHandle(src->esync);

	CloseHandle( src->shm.handle );
	arcan_mem_free(src->shm.key);

	src->shm.ptr = NULL;
}

arcan_errc arcan_frameserver_pushfd(arcan_frameserver* fsrv, int fd)
{
	if (!fsrv)
		return ARCAN_ERRC_BAD_ARGUMENT;

	HANDLE dh, childh, fdh;
	DWORD pid;
	arcan_errc rv;

	childh = OpenProcess(PROCESS_DUP_HANDLE, FALSE, fsrv->childp);
	if (INVALID_HANDLE_VALUE == childh){
		arcan_warning("arcan_frameserver(win32)::push_handle, "
			"couldn't open child process (%ld)\n", GetLastError());
		return ARCAN_ERRC_UNACCEPTED_STATE;
	}

/* assume valid input fd, 64bit issues with this one? */
	fdh = (HANDLE) _get_osfhandle(fd);

	if (DuplicateHandle(GetCurrentProcess(), fdh, childh,
	&dh, 0, FALSE, DUPLICATE_SAME_ACCESS)){
		arcan_event ev = {
			.category = EVENT_TARGET,
			.tgt.kind = TARGET_COMMAND_FDTRANSFER
		};

		ev.tgt.fh = dh;
		arcan_frameserver_pushevent( fsrv, &ev );
		rv = ARCAN_OK;
	}
	else {
		arcan_warning("arcan_frameserver(win32)::push_handle "
			"failed (%ld)\n", GetLastError() );
			rv = ARCAN_ERRC_BAD_ARGUMENT;
	}

	_close(fd);
	return rv;
}

arcan_frameserver* arcan_frameserver_listen_external(const char* key)
{
	arcan_warning("(win32) platform issue, listen_external() not implemented");
	return NULL;
}

/*
 * this could be implemented by (a) adding a WIN32- specific extension
 * in shmif that takes the newseg- argument and packs the necessary
 * handles as well, set the packing globals and just alloc.
 */
arcan_frameserver* arcan_frameserver_spawn_subsegment(
	arcan_frameserver* ctx, bool input, int hintw, int hinth, int tag)
{
	arcan_warning("(win32) platform issue, spawn subsegment() not implemented");
	return NULL;
}

static TCHAR* alloc_wchar(const char* key)
{
	CHAR* mskey = (TCHAR*) malloc((sizeof(TCHAR))*(strlen(key)));
	memset(mskey, 0, _tcslen(mskey));
	mbstowcs((wchar_t*)mskey, key, (strlen(key)));
	return mskey;
}

static SECURITY_ATTRIBUTES nullsec_attr;
static struct arcan_shmif_page* setupshmipc(HANDLE* dfd)
{
	struct arcan_shmif_page* res = NULL;

	nullsec_attr.nLength = sizeof(SECURITY_ATTRIBUTES);
	nullsec_attr.lpSecurityDescriptor = NULL;
	nullsec_attr.bInheritHandle = TRUE;

	*dfd = CreateFileMapping(INVALID_HANDLE_VALUE,  /* hack for specifying shm */
		&nullsec_attr, /* security, want to inherit */
		PAGE_READWRITE, /* access */
		0, /* big-endian size? */
		ARCAN_SHMPAGE_MAX_SZ, /* little-endian size */
		NULL /* null, pass by inheritance */
	);

	if (*dfd != NULL && (res = MapViewOfFile(*dfd,
			FILE_MAP_ALL_ACCESS, 0, 0, ARCAN_SHMPAGE_MAX_SZ))){
		memset(res, 0, sizeof(struct arcan_shmif_page));
		return res;
	}

	arcan_warning("arcan_frameserver_spawn_server(), "
		"could't allocate shared memory.\n");
	return NULL;
}

bool arcan_frameserver_resize(shm_handle* src, int w, int h)
{
	w = abs(w);
	h = abs(h);

	size_t sz = arcan_shmif_getsize(w, h);
	if (sz > ARCAN_SHMPAGE_MAX_SZ)
		return false;

/*
 *  we always overcommit on windows
 */
	return true;
}

arcan_errc arcan_frameserver_spawn_server(arcan_frameserver* ctx,
	struct frameserver_envp setup)
{
	img_cons cons = {.w = 32, .h = 32, .bpp = 4};
	arcan_frameserver_meta vinfo = {0};
	size_t shmsize = ARCAN_SHMPAGE_MAX_SZ;

	HANDLE shmh;
	struct arcan_shmif_page* shmpage = setupshmipc(&shmh);

	if (!shmpage)
		goto error;

	SDL_SysWMinfo wmi;
	SDL_VERSION(&wmi.version);
	SDL_GetWMInfo(&wmi);
	HWND handle = wmi.window;

	ctx->launchedtime = arcan_frametime();

/* is the ctx in an uninitialized state? */
	if (ctx->vid == ARCAN_EID) {
		vfunc_state state = {.tag = ARCAN_TAG_FRAMESERV, .ptr = ctx};
		ctx->source = strdup(setup.args.builtin.resource);
		ctx->vid = arcan_video_addfobject(
			(arcan_vfunc_cb)arcan_frameserver_emptyframe, state, cons, 0);
		ctx->aid = ARCAN_EID;
	}
	else if (setup.custom_feed == false){
/* revert back to empty vfunc? */
		vfunc_state* cstate = arcan_video_feedstate(ctx->vid);
		arcan_video_alterfeed(ctx->vid,
		(arcan_vfunc_cb)arcan_frameserver_emptyframe, *cstate);
	}

	ctx->vsync = CreateSemaphore(&nullsec_attr, 0, 1, NULL);
	ctx->async = CreateSemaphore(&nullsec_attr, 0, 1, NULL);
	ctx->esync = CreateSemaphore(&nullsec_attr, 1, 200, NULL);

	if (!ctx->vsync || !ctx->async ||!ctx->esync)
		arcan_fatal("arcan_frameserver(win32) couldn't allocate semaphores.\n");

	ctx->shm.key = strdup("win32_static");
	ctx->shm.ptr = (void*) shmpage;
	ctx->shm.shmsize = ARCAN_SHMPAGE_MAX_SZ;
	ctx->shm.handle = shmh;
	shmpage->parent = handle;
	shmpage->dms = true;
	shmpage->major = ARCAN_VERSION_MAJOR;
	shmpage->minor = ARCAN_VERSION_MINOR;
	shmpage->cookie = arcan_shmif_cookie();

	arcan_frameserver_configure(ctx, setup);

	char cmdline[4196];
	snprintf(cmdline, sizeof(cmdline) - 1, "\"%s\" %i %i %i %i %s",
		setup.args.builtin.resource, shmh,
		ctx->vsync, ctx->async, ctx->esync, setup.args.builtin.mode);

	PROCESS_INFORMATION pi;
	STARTUPINFO si = {0};
	DWORD exitcode;

	si.cb = sizeof(si);
	if (CreateProcess(
		"arcan_frameserver.exe",
		cmdline,
		0, /* don't make the parent process handle inheritable */
		0, /* don't make the parent thread handle(s) inheritable */
		TRUE, /* inherit the rest (we want semaphore / shm handle) */
/* console application, however we don't need a console window spawned with it */
		CREATE_NO_WINDOW,
		0, /* don't need an ENV[] */
		0, /* don't change CWD */
		&si, /* Startup info */
		&pi /* process-info */)) {

/* anything else that can happen to the child at
 * this point is handled in frameserver_tick_control */
		ctx->child = pi.hProcess;
		ctx->childp = pi.dwProcessId;

		return ARCAN_OK;
	}
	else
		arcan_warning("arcan_frameserver_spawn_server(),"
			" couldn't spawn frameserver.\n");

error:
	arcan_warning("arcan_frameserver(win32): couldn't spawn "
		"frameserver session (out of shared memory?)\n");

	CloseHandle(ctx->async);
	CloseHandle(ctx->vsync);
	CloseHandle(ctx->esync);
	free(ctx->audb);

	if (shmpage)
		UnmapViewOfFile((void*) shmpage);

	if (shmh)
		CloseHandle(shmh);

	return ARCAN_ERRC_OUT_OF_SPACE;
}

