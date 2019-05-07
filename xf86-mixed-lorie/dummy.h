
#define SPARKLE_MODE

/* All drivers should typically include these */
#include "xf86.h"
#include "xf86_OSproc.h"

#include "xf86Cursor.h"

#ifdef XvExtension
#include "xf86xv.h"
#include <X11/extensions/Xv.h>
#endif
#include <string.h>

#include "compat-api.h"

#include <lorie-server.h>

/* Supported chipsets */
typedef enum {
    DUMMY_CHIP
} DUMMYType;

/* function prototypes */

extern Bool DUMMYSwitchMode(SWITCH_MODE_ARGS_DECL);
extern void DUMMYAdjustFrame(ADJUST_FRAME_ARGS_DECL);

/* in dummy_cursor.c */
extern Bool DUMMYCursorInit(ScreenPtr pScrn);
extern void DUMMYShowCursor(ScrnInfoPtr pScrn);
extern void DUMMYHideCursor(ScrnInfoPtr pScrn);

/* globals */
typedef struct _color
{
    int red;
    int green;
    int blue;
} dummy_colors;

typedef struct dummyRec
{
#ifndef SPARKLE_MODE
    /* options */
    OptionInfoPtr Options;
#endif
    Bool swCursor;
    /* proc pointer */
    CloseScreenProcPtr CloseScreen;
    xf86CursorInfoPtr CursorInfo;

    Bool DummyHWCursorShown;
    int cursorX, cursorY;
    int cursorFG, cursorBG;

    dummy_colors colors[1024];
    Bool prop;
    
    struct lorie_server *lorie_server;
    int displayWidth;
    int displayHeight;
    int configuredWidth;
    int configuredHeight;

    CreateScreenResourcesProcPtr CreateScreenResources;
    DamagePtr damage;
    ScreenBlockHandlerProcPtr BlockHandler;
    OsTimerPtr timer;

    DisplayModePtr modes;
} DUMMYRec, *DUMMYPtr;

/* The privates of the DUMMY driver */
#define DUMMYPTR(p)	((DUMMYPtr)((p)->driverPrivate))

