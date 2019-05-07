
/*
 * Copyright 2002, SuSE Linux AG, Author: Egbert Eich
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* All drivers should typically include these */
#include "xf86.h"
#include "xf86_OSproc.h"

/* All drivers initialising the SW cursor need this */
#include "mipointer.h"

/* All drivers using the mi colormap manipulation need this */
#include "micmap.h"

/* For input support */
#include <xf86Xinput.h>

/* identifying atom needed by magnifiers */
#include <X11/Xatom.h>
#include "property.h"

#include "xf86cmap.h"

#include "xf86fbman.h"

#include "fb.h"

#include "picturestr.h"

/*
 * Driver data structures.
 */
#include "dummy.h"

/* These need to be checked */
#include <X11/X.h>
#include <X11/Xproto.h>
#include "scrnintstr.h"
#include "servermd.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "xf86Crtc.h"

#include "dummy_input.h"
#include <lorie-server-protocol.h>

/* Mandatory functions */
static void     DUMMYIdentify(int flags);
static Bool     DUMMYProbe(DriverPtr drv, int flags);
static Bool     DUMMYPreInit(ScrnInfoPtr pScrn, int flags);
static Bool     DUMMYScreenInit(SCREEN_INIT_ARGS_DECL);
static Bool     DUMMYEnterVT(VT_FUNC_ARGS_DECL);
static void     DUMMYLeaveVT(VT_FUNC_ARGS_DECL);
static Bool     DUMMYCloseScreen(CLOSE_SCREEN_ARGS_DECL);
static void     DUMMYFreeScreen(FREE_SCREEN_ARGS_DECL);
static ModeStatus DUMMYValidMode(SCRN_ARG_TYPE arg, DisplayModePtr mode,
                                 Bool verbose, int flags);
static Bool	DUMMYSaveScreen(ScreenPtr pScreen, int mode);

/* Internally used functions */
static Bool	dummyDriverFunc(ScrnInfoPtr pScrn, xorgDriverFuncOp op,
				pointer ptr);


/* static void     DUMMYDisplayPowerManagementSet(ScrnInfoPtr pScrn, */
/* 				int PowerManagementMode, int flags); */

static Bool DUMMYCreateScreenResources(ScreenPtr pScreen);
static void DUMMYBlockHandler(BLOCKHANDLER_ARGS_DECL);
static Bool DUMMYCrtc_set_mode_major(xf86CrtcPtr crtc, DisplayModePtr mode, Rotation rotation, int x, int y);
static Bool DUMMYUpdateModes(ScrnInfoPtr pScrn, int width, int height);

int xorg_lorie_server_init(ScrnInfoPtr pScrn);

#define DUMMY_VERSION 4000
#define DUMMY_NAME "LORIE"
#define DUMMY_DRIVER_NAME "lorie"

#define DUMMY_MAJOR_VERSION PACKAGE_VERSION_MAJOR
#define DUMMY_MINOR_VERSION PACKAGE_VERSION_MINOR
#define DUMMY_PATCHLEVEL PACKAGE_VERSION_PATCHLEVEL

#define DUMMY_MAX_WIDTH 32767
#define DUMMY_MAX_HEIGHT 32767

/*
 * This is intentionally screen-independent.  It indicates the binding
 * choice made in the first PreInit.
 */
static int pix24bpp = 0;


/*
 * This contains the functions needed by the server after loading the driver
 * module.  It must be supplied, and gets passed back by the SetupProc
 * function in the dynamic case.  In the static case, a reference to this
 * is compiled in, and this requires that the name of this DriverRec be
 * an upper-case version of the driver name.
 */

_X_EXPORT DriverRec DUMMY = {
    DUMMY_VERSION,
    DUMMY_DRIVER_NAME,
    DUMMYIdentify,
    DUMMYProbe,
    NULL,
    NULL,
    0,
    dummyDriverFunc
};

static SymTabRec DUMMYChipsets[] = {
    { DUMMY_CHIP,   "dummy" },
    { -1,		 NULL }
};

typedef enum {
    OPTION_SW_CURSOR
} DUMMYOpts;

#ifdef XFree86LOADER

static MODULESETUPPROTO(dummySetup);

static XF86ModuleVersionInfo dummyVersRec =
{
	"dummy",
	MODULEVENDORSTRING,
	MODINFOSTRING1,
	MODINFOSTRING2,
	XORG_VERSION_CURRENT,
	DUMMY_MAJOR_VERSION, DUMMY_MINOR_VERSION, DUMMY_PATCHLEVEL,
	ABI_CLASS_VIDEODRV,
	ABI_VIDEODRV_VERSION,
	MOD_CLASS_VIDEODRV,
	{0,0,0,0}
};

/*
 * This is the module init data.
 * Its name has to be the driver name followed by ModuleData
 */
_X_EXPORT XF86ModuleData lorieModuleData = { &dummyVersRec, dummySetup, NULL };

static pointer
dummySetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    static Bool setupDone = FALSE;

    if (!setupDone) {
	setupDone = TRUE;
        xf86AddDriver(&DUMMY, module, HaveDriverFuncs);

	/*
	 * Modules that this driver always requires can be loaded here
	 * by calling LoadSubModule().
	 */

	/*
	 * The return value must be non-NULL on success even though there
	 * is no TearDownProc.
	 */
	return (pointer)1;
    } else {
	if (errmaj) *errmaj = LDR_ONCEONLY;
	return NULL;
    }
}

#endif /* XFree86LOADER */

static Bool
DUMMYGetRec(ScrnInfoPtr pScrn)
{
    /*
     * Allocate a DUMMYRec, and hook it into pScrn->driverPrivate.
     * pScrn->driverPrivate is initialised to NULL, so we can check if
     * the allocation has already been done.
     */
    if (pScrn->driverPrivate != NULL)
        return TRUE;

    pScrn->driverPrivate = xnfcalloc(sizeof(DUMMYRec), 1);

    if (pScrn->driverPrivate == NULL)
        return FALSE;

    return TRUE;
}

static void
DUMMYFreeRec(ScrnInfoPtr pScrn)
{
    DUMMYPtr dPtr = DUMMYPTR(pScrn);

    if (dPtr == NULL)
        return;

    free(dPtr);

    pScrn->driverPrivate = NULL;
}

/* Mandatory */
static void
DUMMYIdentify(int flags)
{
    xf86PrintChipsets(DUMMY_NAME, "Driver for Dummy chipsets",
			DUMMYChipsets);
}

/* Mandatory */
static Bool
DUMMYProbe(DriverPtr drv, int flags)
{
    Bool foundScreen = FALSE;
    int numDevSections, numUsed;
    GDevPtr *devSections;
    int i;

    if (flags & PROBE_DETECT)
	return FALSE;
    /*
     * Find the config file Device sections that match this
     * driver, and return if there are none.
     */
    if ((numDevSections = xf86MatchDevice(DUMMY_DRIVER_NAME,
					  &devSections)) <= 0) {
	return FALSE;
    }

    numUsed = numDevSections;

    if (numUsed > 0) {

	for (i = 0; i < numUsed; i++) {
	    ScrnInfoPtr pScrn = NULL;
	    int entityIndex =
		xf86ClaimNoSlot(drv,DUMMY_CHIP,devSections[i],TRUE);
	    /* Allocate a ScrnInfoRec and claim the slot */
	    if ((pScrn = xf86AllocateScreen(drv,0 ))) {
		   xf86AddEntityToScreen(pScrn,entityIndex);
		    pScrn->driverVersion = DUMMY_VERSION;
		    pScrn->driverName    = DUMMY_DRIVER_NAME;
		    pScrn->name          = DUMMY_NAME;
		    pScrn->Probe         = DUMMYProbe;
		    pScrn->PreInit       = DUMMYPreInit;
		    pScrn->ScreenInit    = DUMMYScreenInit;
		    pScrn->SwitchMode    = DUMMYSwitchMode;
		    pScrn->AdjustFrame   = DUMMYAdjustFrame;
		    pScrn->EnterVT       = DUMMYEnterVT;
		    pScrn->LeaveVT       = DUMMYLeaveVT;
		    pScrn->FreeScreen    = DUMMYFreeScreen;
		    pScrn->ValidMode     = DUMMYValidMode;

		    foundScreen = TRUE;
	    }
	}
    }

    free(devSections);

    return foundScreen;
}

# define RETURN \
    { DUMMYFreeRec(pScrn);\
			    return FALSE;\
					     }

Bool DUMMYCrtc_resize(ScrnInfoPtr pScrn, int width, int height)
{
    xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    ScreenPtr pScreen = xf86ScrnToScreen(pScrn);
    DUMMYPtr dPtr = DUMMYPTR(pScrn);

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "PIXMAP SIZE: %dx%d\n", width, height);



    PixmapPtr pPixmap = pScreen->GetScreenPixmap(pScreen);

    void *data = lorie_server_resize_image(dPtr->lorie_server, width, height);
    if (data == NULL) return FALSE;

    pScrn->virtualX = width;
    pScrn->virtualY = height;
    pScrn->displayWidth = width; //XXX

    int cpp = (pScrn->bitsPerPixel + 7) / 8;
    pScreen->ModifyPixmapHeader(pPixmap, width, height, -1, -1, pScrn->displayWidth * cpp, data);


    //XXX
    int i;
    for (i = 0; i < xf86_config->num_crtc; i++)
    {
        xf86CrtcPtr crtc = xf86_config->crtc[i];

        if (!crtc->enabled)
            continue;

        DUMMYCrtc_set_mode_major(crtc, &crtc->mode, crtc->rotation, crtc->x, crtc->y);
    }

    return TRUE;
}

static const xf86CrtcConfigFuncsRec dummyCrtcConfigFuncs = {
    DUMMYCrtc_resize
    //Bool (*resize) (ScrnInfoPtr scrn, int width, int height);
};

//==================================================================================================

static void DUMMYCrtc_dpms(xf86CrtcPtr crtc, int mode)
{
}

//XXX
static Bool DUMMYCrtc_set_mode_major(xf86CrtcPtr crtc, DisplayModePtr mode, Rotation rotation, int x, int y)
{
    crtc->mode = *mode;
    crtc->x = x;
    crtc->y = y;
    crtc->rotation = rotation;

    return TRUE;
}

//XXX
static const xf86CrtcFuncsRec dummyCrtcFuncs = {
    .dpms = DUMMYCrtc_dpms,
    .set_mode_major = DUMMYCrtc_set_mode_major,
};

//==================================================================================================

static void
DUMMYOutput_dmps(xf86OutputPtr output, int mode)
{
}

static xf86OutputStatus
DUMMYOutput_detect(xf86OutputPtr output)
{
    return XF86OutputStatusConnected;
}

static int
DUMMYOutput_mode_valid(xf86OutputPtr output, DisplayModePtr pMode)
{
    //FIXME
    return MODE_OK;
}

static DisplayModePtr
DUMMYOutput_get_modes(xf86OutputPtr output)
{
    ScrnInfoPtr pScrn;
    DUMMYPtr dPtr;

    pScrn = output->scrn;
    dPtr = DUMMYPTR(pScrn);

    return xf86DuplicateModes(NULL, dPtr->modes);
}

static const xf86OutputFuncsRec dummyOutputFuncs = {
    .dpms = DUMMYOutput_dmps,
    .detect = DUMMYOutput_detect,
    .mode_valid = DUMMYOutput_mode_valid,
    .get_modes = DUMMYOutput_get_modes,
};

//==================================================================================================

/* Mandatory */
Bool
DUMMYPreInit(ScrnInfoPtr pScrn, int flags)
{
    //ClockRangePtr clockRanges;
    //int i;
    DUMMYPtr dPtr;
    int maxClock = 300000;
    GDevPtr device = xf86GetEntityInfo(pScrn->entityList[0])->device;
    xf86CrtcPtr crtc;
    xf86OutputPtr output;

    if (flags & PROBE_DETECT)
	return TRUE;

    /* Allocate the DummyRec driverPrivate */
    if (!DUMMYGetRec(pScrn)) {
	return FALSE;
    }

    dPtr = DUMMYPTR(pScrn);

    pScrn->chipset = (char *)xf86TokenToString(DUMMYChipsets,
					       DUMMY_CHIP);

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Chipset is a DUMMY\n");

    pScrn->monitor = pScrn->confScreen->monitor;

    if (!xf86SetDepthBpp(pScrn, 0, 0, 0,  Support24bppFb | Support32bppFb))
	return FALSE;
    else {
	/* Check that the returned depth is one we support */
	switch (pScrn->depth) {
	case 8:
	case 15:
	case 16:
	case 24:
	case 30:
	    break;
	default:
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Given depth (%d) is not supported by this driver\n",
		       pScrn->depth);
	    return FALSE;
	}
    }

    xf86PrintDepthBpp(pScrn);
    if (pScrn->depth == 8)
	pScrn->rgbBits = 8;

    /* Get the depth24 pixmap format */
    if (pScrn->depth == 24 && pix24bpp == 0)
	pix24bpp = xf86GetBppFromDepth(pScrn, 24);

    /*
     * This must happen after pScrn->display has been set because
     * xf86SetWeight references it.
     */
    if (pScrn->depth > 8) {
	/* The defaults are OK for us */
	rgb zeros = {0, 0, 0};

	if (!xf86SetWeight(pScrn, zeros, zeros)) {
	    return FALSE;
	} else {
	    /* XXX check that weight returned is supported */
	    ;
	}
    }

    if (!xf86SetDefaultVisual(pScrn, -1))
	return FALSE;

    if (pScrn->depth > 1) {
	Gamma zeros = {0.0, 0.0, 0.0};

	if (!xf86SetGamma(pScrn, zeros))
	    return FALSE;
    }

    xf86CollectOptions(pScrn, NULL);
    dPtr->swCursor = 0;

    if (device->videoRam != 0) {
	pScrn->videoRam = device->videoRam;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "VideoRAM: %d kByte\n",
		   pScrn->videoRam);
    } else {
	pScrn->videoRam = 16 * 1024;
	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "VideoRAM: %d kByte\n",
		   pScrn->videoRam);
    }

    if (device->dacSpeeds[0] != 0) {
	maxClock = device->dacSpeeds[0];
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Max Clock: %d kHz\n",
		   maxClock);
    } else {
	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Max Clock: %d kHz\n",
		   maxClock);
    }

    pScrn->progClock = TRUE;

    if (!monitorResolution)
        monitorResolution = xf86SetIntOption(pScrn->options, "DPI", 96);

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "DPI: %d\n", monitorResolution);

    dPtr->displayWidth = 800;
    dPtr->displayHeight = 600;
    dPtr->configuredWidth = dPtr->displayWidth;
    dPtr->configuredHeight = dPtr->displayHeight;
    dPtr->modes = xf86CVTMode(dPtr->displayWidth, dPtr->displayHeight, 60, 0, 0);

    //XXX Check results
    xf86CrtcConfigInit(pScrn, &dummyCrtcConfigFuncs);
    xf86CrtcSetSizeRange(pScrn, 240, 240, 4096, 4096);

    crtc = xf86CrtcCreate(pScrn, &dummyCrtcFuncs);
    output = xf86OutputCreate(pScrn, &dummyOutputFuncs, "lorie");
    output->possible_crtcs = 0x7f;

    xf86InitialConfiguration (pScrn, TRUE);

    pScrn->currentMode = pScrn->modes; //XXX

    crtc->funcs->set_mode_major(crtc, pScrn->currentMode, RR_Rotate_0, 0, 0); //XXX

    xf86SetDpi(pScrn, 0, 0);

    //output->mm_width = 0;
    //output->mm_height = 0;
#ifndef __ANDROID__
    if (xf86LoadSubModule(pScrn, "fb") == NULL) {
	RETURN;
    }
#endif

    if (!dPtr->swCursor) {
	if (!xf86LoadSubModule(pScrn, "ramdac"))
	    RETURN;
    }

    /* We have no contiguous physical fb in physical memory */
    pScrn->memPhysBase = 0;
    pScrn->fbOffset = 0;

    return TRUE;
}
#undef RETURN

/* Mandatory */
static Bool
DUMMYEnterVT(VT_FUNC_ARGS_DECL)
{
    return TRUE;
}

/* Mandatory */
static void
DUMMYLeaveVT(VT_FUNC_ARGS_DECL)
{
}

static void
DUMMYLoadPalette(
   ScrnInfoPtr pScrn,
   int numColors,
   int *indices,
   LOCO *colors,
   VisualPtr pVisual
){
   int i, index, shift, Gshift;
   DUMMYPtr dPtr = DUMMYPTR(pScrn);

   switch(pScrn->depth) {
   case 15:
	shift = Gshift = 1;
	break;
   case 16:
	shift = 0;
        Gshift = 0;
	break;
   default:
	shift = Gshift = 0;
	break;
   }

   for(i = 0; i < numColors; i++) {
       index = indices[i];
       dPtr->colors[index].red = colors[index].red << shift;
       dPtr->colors[index].green = colors[index].green << Gshift;
       dPtr->colors[index].blue = colors[index].blue << shift;
   }

}

static ScrnInfoPtr DUMMYScrn; /* static-globalize it */

/* Mandatory */
static Bool
DUMMYScreenInit(SCREEN_INIT_ARGS_DECL)
{
    ScrnInfoPtr pScrn;
    DUMMYPtr dPtr;
    int ret;
    VisualPtr visual;

    /*
     * we need to get the ScrnInfoRec for this screen, so let's allocate
     * one first thing
     */
    pScrn = xf86ScreenToScrn(pScreen);
    dPtr = DUMMYPTR(pScrn);
    DUMMYScrn = pScrn;
    
    xorg_lorie_server_init(pScrn);

    /*
     * Reset visual list.
     */
    miClearVisualTypes();

    /* Setup the visuals we support. */

    if (!miSetVisualTypes(pScrn->depth,
      		      miGetDefaultVisualMask(pScrn->depth),
		      pScrn->rgbBits, pScrn->defaultVisual))
         return FALSE;

    if (!miSetPixmapDepths ()) return FALSE;

    pScrn->displayWidth = pScrn->virtualX; //XXX

    /*
     * Call the framebuffer layer's ScreenInit function, and fill in other
     * pScreen fields.
     */
    ret = fbScreenInit(pScreen, NULL,
			    pScrn->virtualX, pScrn->virtualY,
			    pScrn->xDpi, pScrn->yDpi,
			    pScrn->displayWidth, pScrn->bitsPerPixel);
    if (!ret)
	return FALSE;

    if (pScrn->depth > 8) {
        /* Fixup RGB ordering */
        visual = pScreen->visuals + pScreen->numVisuals;
        while (--visual >= pScreen->visuals) {
	    if ((visual->class | DynamicClass) == DirectColor) {
		visual->offsetRed = pScrn->offset.red;
		visual->offsetGreen = pScrn->offset.green;
		visual->offsetBlue = pScrn->offset.blue;
		visual->redMask = pScrn->mask.red;
		visual->greenMask = pScrn->mask.green;
		visual->blueMask = pScrn->mask.blue;
	    }
	}
    }

    /* must be after RGB ordering fixed */
    fbPictureInit(pScreen, 0, 0);

    dPtr->CreateScreenResources = pScreen->CreateScreenResources;
    pScreen->CreateScreenResources = DUMMYCreateScreenResources;

    xf86SetBlackWhitePixels(pScreen);

    if (dPtr->swCursor)
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Using Software Cursor.\n");

    {


	BoxRec AvailFBArea;
	int lines = pScrn->videoRam * 1024 /
	    (pScrn->displayWidth * (pScrn->bitsPerPixel >> 3));
	AvailFBArea.x1 = 0;
	AvailFBArea.y1 = 0;
	AvailFBArea.x2 = pScrn->displayWidth;
	AvailFBArea.y2 = lines;
	xf86InitFBManager(pScreen, &AvailFBArea);

	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Using %i scanlines of offscreen memory \n"
		   , lines - pScrn->virtualY);
    }

    xf86SetBackingStore(pScreen);
    xf86SetSilkenMouse(pScreen);

    /* Initialise cursor functions */
    miDCInitialize (pScreen, xf86GetPointerScreenFuncs());


    if (!dPtr->swCursor) {
      /* HW cursor functions */
      if (!DUMMYCursorInit(pScreen)) {
	  xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		     "Hardware cursor initialization failed\n");
	  return FALSE;
      }
    }

    /* Initialise default colourmap */
    if(!miCreateDefColormap(pScreen))
	return FALSE;

    if (!xf86HandleColormaps(pScreen, 1024, pScrn->rgbBits,
                         DUMMYLoadPalette, NULL,
                         CMAP_PALETTED_TRUECOLOR
			     | CMAP_RELOAD_ON_MODE_SWITCH))
	return FALSE;

    pScreen->SaveScreen = DUMMYSaveScreen;

    /* Wrap the current CloseScreen function */
    dPtr->CloseScreen = pScreen->CloseScreen;
    pScreen->CloseScreen = DUMMYCloseScreen;

    dPtr->BlockHandler = pScreen->BlockHandler;
    pScreen->BlockHandler = DUMMYBlockHandler;

    xf86CrtcScreenInit(pScreen); //XXX Check result

    return TRUE;
}

/* Mandatory */
Bool
DUMMYSwitchMode(SWITCH_MODE_ARGS_DECL)
{
    return TRUE;
}

/* Mandatory */
void
DUMMYAdjustFrame(ADJUST_FRAME_ARGS_DECL)
{
}

/* Mandatory */
static Bool
DUMMYCloseScreen(CLOSE_SCREEN_ARGS_DECL)
{
    ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
    DUMMYPtr dPtr = DUMMYPTR(pScrn);
	if (dPtr->lorie_server != NULL) {
		RemoveNotifyFd(lorie_server_get_fd(dPtr->lorie_server));
		lorie_server_destroy(&dPtr->lorie_server);
	}

    if (dPtr->CursorInfo)
	xf86DestroyCursorInfoRec(dPtr->CursorInfo);

    DamageUnregister(dPtr->damage);
    DamageDestroy(dPtr->damage);

    pScreen->CreateScreenResources = dPtr->CreateScreenResources;
    pScreen->BlockHandler = dPtr->BlockHandler;
    
    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Destroying lorie_server\n");
    lorie_server_destroy(&dPtr->lorie_server);

    pScrn->vtSema = FALSE;
    pScreen->CloseScreen = dPtr->CloseScreen;
    return (*pScreen->CloseScreen)(CLOSE_SCREEN_ARGS);
}

/* Optional */
static void
DUMMYFreeScreen(FREE_SCREEN_ARGS_DECL)
{
    SCRN_INFO_PTR(arg);

    DUMMYFreeRec(pScrn);
}

static Bool
DUMMYSaveScreen(ScreenPtr pScreen, int mode)
{
    return TRUE;
}

/* Optional */
static ModeStatus
DUMMYValidMode(SCRN_ARG_TYPE arg, DisplayModePtr mode, Bool verbose, int flags)
{
    return(MODE_OK);
}

#ifndef HW_SKIP_CONSOLE
#define HW_SKIP_CONSOLE 4
#endif

static Bool
dummyDriverFunc(ScrnInfoPtr pScrn, xorgDriverFuncOp op, pointer ptr)
{
    CARD32 *flag;

    switch (op) {
	case GET_REQUIRED_HW_INTERFACES:
	    flag = (CARD32*)ptr;
	    (*flag) = HW_SKIP_CONSOLE;
	    return TRUE;
	default:
	    return FALSE;
    }
}

static Bool
DUMMYCreateScreenResources(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
    DUMMYPtr dPtr = DUMMYPTR(DUMMYScrn);
    PixmapPtr rootPixmap;
    Bool ret;

    pScreen->CreateScreenResources = dPtr->CreateScreenResources;
    ret = pScreen->CreateScreenResources(pScreen);
    pScreen->CreateScreenResources = DUMMYCreateScreenResources;

    rootPixmap = pScreen->GetScreenPixmap(pScreen);

    dPtr->damage = DamageCreate(NULL, NULL, DamageReportNone, TRUE, pScreen, rootPixmap);
    if (!dPtr->damage)
    {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Failed to create screen damage record\n");
        return FALSE;
    }

	DamageRegister(&rootPixmap->drawable, dPtr->damage);

    return ret;
}

static void DUMMYBlockHandler(BLOCKHANDLER_ARGS_DECL)
{
    SCREEN_PTR(arg);

    ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
    DUMMYPtr dPtr = DUMMYPTR(pScrn);

    pScreen->BlockHandler = dPtr->BlockHandler;
    pScreen->BlockHandler(BLOCKHANDLER_ARGS);
    pScreen->BlockHandler = DUMMYBlockHandler;

    RegionPtr pRegion = DamageRegion(dPtr->damage);

    if (RegionNotEmpty(pRegion))
    {
        DamageEmpty(dPtr->damage);
		if (dPtr->lorie_server == NULL) return;
        lorie_server_send_damage(dPtr->lorie_server, pRegion->extents.x1, pRegion->extents.y1, pRegion->extents.x2, pRegion->extents.y2);

    }
}

static Bool
DUMMYUpdateModes(ScrnInfoPtr pScrn, int width, int height)
{
    DUMMYPtr dPtr;
    dPtr = DUMMYPTR(pScrn);

    DisplayModePtr mode = dPtr->modes;
    while (mode != NULL)
    {
        DisplayModePtr nextMode = mode->next;
        free((void *)mode->name);
        free(mode);
        mode = nextMode;
    }
    dPtr->modes = NULL;

    dPtr->modes = xf86ModesAdd(dPtr->modes, xf86CVTMode(width, height, 60, 0, 0)); //Native

    dPtr->modes = xf86ModesAdd(dPtr->modes, xf86CVTMode(width*10/15, height*10/15, 60, 0, 0));
    dPtr->modes = xf86ModesAdd(dPtr->modes, xf86CVTMode(width/2, height/2, 60, 0, 0));
    dPtr->modes = xf86ModesAdd(dPtr->modes, xf86CVTMode(width/4, height/4, 60, 0, 0));

    dPtr->modes = xf86ModesAdd(dPtr->modes, xf86CVTMode(width*15/10, height*15/10, 60, 0, 0));

    xf86ProbeOutputModes(pScrn, 2048, 2048);
    xf86SetScrnInfoModes(pScrn); //XXX

    return TRUE;
}



//==================================================================================================

static InputInfoPtr lorie_input_info = NULL;

void lorie_set_input_driver_ptr(InputInfoPtr input) {
	lorie_input_info = input;
}

static void xorg_lorie_handle_event(int fd, int ready, void *data)
{
    ScrnInfoPtr pScrn = (ScrnInfoPtr)data;
    DUMMYPtr dPtr = DUMMYPTR(pScrn);

    if (ready)
        lorie_server_process(dPtr->lorie_server);
}

static void xorg_lorie_client_created_handler(void *data, int32_t fd) {
    ScrnInfoPtr pScrn = (ScrnInfoPtr)data;
	SetNotifyFd(fd, xorg_lorie_handle_event, X_NOTIFY_READ | X_NOTIFY_WRITE | X_NOTIFY_ERROR, pScrn);
}

static void xorg_lorie_client_destroyed_handler(void *data, int32_t fd) {
	RemoveNotifyFd(fd);
}

static void xorg_lorie_pointer_event_handler(void *data, int32_t state, int32_t button, int32_t x, int32_t y) {
    ScrnInfoPtr pScrn = (ScrnInfoPtr)data;
    InputInfoPtr pInfo = lorie_input_info;
    if (pInfo == NULL) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "lorie_input is unavailable\n");
		return;
	}

    int down;
    
	/*
	* Send events.
	*
	* We *must* generate a motion before a button change if pointer
	* location has changed as DIX assumes this. This is why we always
	* emit a motion, regardless of the kind of packet processed.
	*/
    xf86PostMotionEvent(pInfo->dev, Absolute, 0, 2, x, y);
	switch(state) {
		case LORIE_INPUT_STATE_UP: 
		case LORIE_INPUT_STATE_DOWN: 
			down = (state==LORIE_INPUT_STATE_DOWN) ? 1 : 0;
			xf86PostButtonEvent(pInfo->dev, Absolute, button, down, 0, 2, x, y);
			break;
	}
}

static void xorg_lorie_key_event_handler(void *data, int32_t state, int32_t key) {
    ScrnInfoPtr pScrn = (ScrnInfoPtr)data;
    InputInfoPtr pInfo = lorie_input_info;
    if (pInfo == NULL) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "lorie_input is unavailable\n");
		return;
	}
	xf86PostKeyboardEvent(pInfo->dev, key, state);
}

static void xorg_lorie_resolution_change_handler(void *data, int32_t width, int32_t height) {
    ScrnInfoPtr pScrn = (ScrnInfoPtr)data;
	
    ScreenPtr pScreen = xf86ScrnToScreen(pScrn);
    DUMMYPtr dPtr = DUMMYPTR(pScrn);

    dPtr->displayWidth = width;
    dPtr->displayHeight = height;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "DISPLAY SIZE: %dx%d\n", dPtr->displayWidth, dPtr->displayHeight);

    if (dPtr->displayWidth != dPtr->configuredWidth || dPtr->displayHeight != dPtr->configuredHeight)
    {
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Reconfiguring for %dx%d\n", dPtr->displayWidth, dPtr->displayHeight);

        DUMMYUpdateModes(pScrn, dPtr->displayWidth, dPtr->displayHeight);

        DisplayModePtr mode = dPtr->modes;
        //pScrn->currentMode = mode;

        int mmWidth = mode->HDisplay * 25.4 / monitorResolution;
        int mmHeight = mode->VDisplay * 25.4 / monitorResolution;

        RRScreenSizeSet(pScreen, mode->HDisplay, mode->VDisplay, mmWidth, mmHeight);
        xf86SetSingleMode(pScrn, mode, RR_Rotate_0);

        dPtr->configuredWidth = dPtr->displayWidth;
        dPtr->configuredHeight = dPtr->displayHeight;
    } else {
		lorie_server_send_current_buffer(dPtr->lorie_server);
	}
}

static struct lorie_server_callbacks xorg_server_callbacks = {
	xorg_lorie_client_created_handler,
	xorg_lorie_client_destroyed_handler,
	
	xorg_lorie_pointer_event_handler,
	xorg_lorie_key_event_handler, 
	xorg_lorie_resolution_change_handler
};

int xorg_lorie_server_init(ScrnInfoPtr pScrn) {
	if (pScrn == NULL) return -1;
	DUMMYPtr dPtr = DUMMYPTR(pScrn);
	if (dPtr->lorie_server != NULL) {
		lorie_server_destroy(&dPtr->lorie_server);
	}

    dPtr->lorie_server = lorie_server_create(&xorg_server_callbacks, pScrn);
    if (dPtr->lorie_server == NULL) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Failed to create Lorie server\n");
		return -1;
	}
    SetNotifyFd(lorie_server_get_fd(dPtr->lorie_server), xorg_lorie_handle_event, X_NOTIFY_READ | X_NOTIFY_WRITE | X_NOTIFY_ERROR, pScrn);
    return 0;
}

//==================================================================================================
