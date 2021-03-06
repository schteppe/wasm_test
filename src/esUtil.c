#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include "esUtil.h"

#include  <X11/Xlib.h>
#include  <X11/Xatom.h>
#include  <X11/Xutil.h>

#include  <emscripten.h>
#include <emscripten/html5.h>

static Display *x_display = NULL;

EGLBoolean CreateEGLContext ( EGLNativeWindowType hWnd, EGLDisplay* eglDisplay,
                              EGLContext* eglContext, EGLSurface* eglSurface,
                              EGLint attribList[])
{
   EGLint numConfigs;
   EGLint majorVersion;
   EGLint minorVersion;
   EGLDisplay display;
   EGLContext context;
   EGLSurface surface;
   EGLConfig config;
   EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE, EGL_NONE };

   // Get Display
   display = eglGetDisplay((EGLNativeDisplayType)x_display);
   if ( display == EGL_NO_DISPLAY )
   {
      return EGL_FALSE;
   }

   // Initialize EGL
   if ( !eglInitialize(display, &majorVersion, &minorVersion) )
   {
      return EGL_FALSE;
   }

   // Get configs
   if ( !eglGetConfigs(display, NULL, 0, &numConfigs) )
   {
      return EGL_FALSE;
   }

   // Choose config
   if ( !eglChooseConfig(display, attribList, &config, 1, &numConfigs) )
   {
      return EGL_FALSE;
   }

   // Create a surface
   surface = eglCreateWindowSurface(display, config, (EGLNativeWindowType)hWnd, NULL);
   if ( surface == EGL_NO_SURFACE )
   {
      return EGL_FALSE;
   }

   // Create a GL context
   context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs );
   if ( context == EGL_NO_CONTEXT )
   {
      return EGL_FALSE;
   }   
   
   // Make the context current
   if ( !eglMakeCurrent(display, surface, surface, context) )
   {
      return EGL_FALSE;
   }

   *eglDisplay = display;
   *eglSurface = surface;
   *eglContext = context;
   return EGL_TRUE;
} 


///
//  WinCreate()
//
//      This function initialized the native X11 display and window for EGL
//
EGLBoolean WinCreate(ESContext *esContext, const char *title)
{
    Window root;
    XSetWindowAttributes swa;
    XSetWindowAttributes  xattr;
    Atom wm_state;
    XWMHints hints;
    XEvent xev;
    EGLConfig ecfg;
    EGLint num_config;
    Window win;

    /*
     * X11 native display initialization
     */
    x_display = XOpenDisplay(NULL);
    if ( x_display == NULL )
    {
        return EGL_FALSE;
    }

    root = DefaultRootWindow(x_display);

    swa.event_mask  =  ExposureMask | PointerMotionMask | KeyPressMask;
    win = XCreateWindow(
               x_display, root,
               0, 0, esContext->width, esContext->height, 0,
               CopyFromParent, InputOutput,
               CopyFromParent, CWEventMask,
               &swa );

    xattr.override_redirect = FALSE;
    XChangeWindowAttributes ( x_display, win, CWOverrideRedirect, &xattr );

    hints.input = TRUE;
    hints.flags = InputHint;
    XSetWMHints(x_display, win, &hints);

    // make the window visible on the screen
    XMapWindow (x_display, win);
    XStoreName (x_display, win, title);

    // get identifiers for the provided atom name strings
    wm_state = XInternAtom (x_display, "_NET_WM_STATE", FALSE);

    memset ( &xev, 0, sizeof(xev) );
    xev.type                 = ClientMessage;
    xev.xclient.window       = win;
    xev.xclient.message_type = wm_state;
    xev.xclient.format       = 32;
    xev.xclient.data.l[0]    = 1;
    xev.xclient.data.l[1]    = FALSE;
    XSendEvent (
       x_display,
       DefaultRootWindow ( x_display ),
       FALSE,
       SubstructureNotifyMask,
       &xev );

    esContext->hWnd = (EGLNativeWindowType) win;
    return EGL_TRUE;
}


///
//  userInterrupt()
//
//      Reads from X11 event loop and interrupt program if there is a keypress, or
//      window close action.
//
GLboolean userInterrupt(ESContext *esContext)
{
    XEvent xev;
    KeySym key;
    GLboolean userinterrupt = GL_FALSE;
    char text;

    // Pump all messages from X server. Keypresses are directed to keyfunc (if defined)
    while ( XPending ( x_display ) )
    {
        XNextEvent( x_display, &xev );
        if ( xev.type == KeyPress )
        {
            if (XLookupString(&xev.xkey,&text,1,&key,0)==1)
            {
                if (esContext->keyFunc != NULL)
                    esContext->keyFunc(esContext, text, 0, 0);
            }
        }
        if ( xev.type == DestroyNotify )
            userinterrupt = GL_TRUE;
    }
    return userinterrupt;
}


void ESUTIL_API esInitContext ( ESContext *esContext )
{
   if ( esContext != NULL )
   {
      memset( esContext, 0, sizeof( ESContext) );
   }
}


///
//  esCreateWindow()
//
//      title - name for title bar of window
//      width - width of window to create
//      height - height of window to create
//      flags  - bitwise or of window creation flags 
//          ES_WINDOW_ALPHA       - specifies that the framebuffer should have alpha
//          ES_WINDOW_DEPTH       - specifies that a depth buffer should be created
//          ES_WINDOW_STENCIL     - specifies that a stencil buffer should be created
//          ES_WINDOW_MULTISAMPLE - specifies that a multi-sample buffer should be created
//
GLboolean ESUTIL_API esCreateWindow ( ESContext *esContext, const char* title, GLint width, GLint height, GLuint flags )
{
   EGLint attribList[] =
   {
       EGL_RED_SIZE,       5,
       EGL_GREEN_SIZE,     6,
       EGL_BLUE_SIZE,      5,
       EGL_ALPHA_SIZE,     (flags & ES_WINDOW_ALPHA) ? 8 : EGL_DONT_CARE,
       EGL_DEPTH_SIZE,     (flags & ES_WINDOW_DEPTH) ? 8 : EGL_DONT_CARE,
       EGL_STENCIL_SIZE,   (flags & ES_WINDOW_STENCIL) ? 8 : EGL_DONT_CARE,
       EGL_SAMPLE_BUFFERS, (flags & ES_WINDOW_MULTISAMPLE) ? 1 : 0,
       EGL_NONE
   };
   
   if ( esContext == NULL )
   {
      return GL_FALSE;
   }

   esContext->width = width;
   esContext->height = height;
   
   esContext->deltatime = 0.0f;
   esContext->totaltime = 0.0f;
   esContext->frames = 0;

   if ( !WinCreate ( esContext, title) )
   {
      return GL_FALSE;
   }

  
   if ( !CreateEGLContext ( esContext->hWnd,
                            &esContext->eglDisplay,
                            &esContext->eglContext,
                            &esContext->eglSurface,
                            attribList) )
   {
      return GL_FALSE;
   }
   

   return GL_TRUE;
}

struct timeval t1, t2;
struct timezone tz;

void update(void* data){
    ESContext *esContext = (ESContext*)data;

    gettimeofday(&t2, &tz);
    if(esContext->frames == 0){
        t1 = t2;
    }
    esContext->deltatime = (float)(t2.tv_sec - t1.tv_sec + (t2.tv_usec - t1.tv_usec) * 1e-6);
    t1 = t2;

    if (esContext->updateFunc != NULL)
        esContext->updateFunc(esContext, esContext->deltatime);
    if (esContext->drawFunc != NULL)
        esContext->drawFunc(esContext);

    eglSwapBuffers(esContext->eglDisplay, esContext->eglSurface);

    esContext->totaltime += esContext->deltatime;
    esContext->frames++;

    emscripten_async_call(update, (void*)esContext, -1);
}


EM_BOOL mouseMoveCallback(int eventType, const EmscriptenMouseEvent *mouseEvent, void *userData){
    printf("mousemove %ld %ld\n", mouseEvent->canvasX, mouseEvent->canvasY);
    return false;
}

EM_BOOL mouseDownCallback(int eventType, const EmscriptenMouseEvent *mouseEvent, void *userData){
    printf("mousedown %ld %ld\n", mouseEvent->canvasX, mouseEvent->canvasY);
    return false;
}

EM_BOOL mouseUpCallback(int eventType, const EmscriptenMouseEvent *mouseEvent, void *userData){
    printf("mouseup %ld %ld\n", mouseEvent->canvasX, mouseEvent->canvasY);
    return false;
}

EM_BOOL mouseClickCallback(int eventType, const EmscriptenMouseEvent *mouseEvent, void *userData){
    printf("click %ld %ld\n", mouseEvent->canvasX, mouseEvent->canvasY);
    return false;
}

EM_BOOL resizeCallback(int eventType, const EmscriptenUiEvent *event, void *userData){
    printf("resize %d %d\n", event->windowInnerWidth, event->windowInnerHeight);
    //emscripten_set_element_css_size("canvas", event->windowInnerWidth, event->windowInnerHeight);
    emscripten_set_canvas_element_size("canvas", event->windowInnerWidth, event->windowInnerHeight);
    
    ESContext* esContext = (ESContext*)userData;
    esContext->width = event->windowInnerWidth;
    esContext->height = event->windowInnerHeight;

    return false;
}

EM_BOOL scrollCallback(int eventType, const EmscriptenUiEvent *event, void *userData){
    printf("scroll %ld\n", event->detail);
    return false;
}

EM_BOOL wheelCallback(int eventType, const EmscriptenWheelEvent *event, void *userData){
    printf("wheel %lf %lf %lf\n", event->deltaX, event->deltaY, event->deltaZ);
    return false;
}


void ESUTIL_API esMainLoop ( ESContext *esContext )
{
    emscripten_set_mousemove_callback("canvas",esContext,false,mouseMoveCallback);
    emscripten_set_mousedown_callback("canvas",esContext,false,mouseDownCallback);
    emscripten_set_mouseup_callback("canvas",esContext,false,mouseUpCallback);
    emscripten_set_click_callback("canvas",esContext,false,mouseClickCallback);
    
    emscripten_set_resize_callback(nullptr,esContext,false,resizeCallback);
    emscripten_set_scroll_callback(nullptr,esContext,false,scrollCallback);

    emscripten_set_wheel_callback("canvas",esContext,false,wheelCallback);

    emscripten_async_call(update, (void*)esContext, -1);
}


void ESUTIL_API esRegisterDrawFunc ( ESContext *esContext, void (ESCALLBACK *drawFunc) (ESContext* ) )
{
   esContext->drawFunc = drawFunc;
}

void ESUTIL_API esRegisterUpdateFunc ( ESContext *esContext, void (ESCALLBACK *updateFunc) ( ESContext*, float ) )
{
   esContext->updateFunc = updateFunc;
}

void ESUTIL_API esRegisterKeyFunc ( ESContext *esContext,
                                    void (ESCALLBACK *keyFunc) (ESContext*, unsigned char, int, int ) )
{
   esContext->keyFunc = keyFunc;
}

void ESUTIL_API esLogMessage ( const char *formatStr, ... )
{
    va_list params;
    char buf[BUFSIZ];

    va_start ( params, formatStr );
    vsprintf ( buf, formatStr, params );
    
    printf ( "%s", buf );
    
    va_end ( params );
}
