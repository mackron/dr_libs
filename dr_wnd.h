// Window manager. Public Domain. See "unlicense" statement at the end of this file.

// !!!! THIS IS INCOMPLETE AND EXPERIMENTAL !!!!

// NOTE: ONLY WIN32 IS CURRENTLY SUPPORTED. X11 COMING SOON.

// USAGE
//
// dr_wnd is a single-file library. To use it, do something like the following in one .c file.
//   #define DR_WND_IMPLEMENTATION
//   #include "dr_wnd.h"
//
// You can then #include dr_wnd.h in other parts of the program as you would with any other header file.
//
// dr_wnd is a very simple library to help you create windows for your application in a cross-platform way. It's good
// if you have simple requirements or for just quickly getting something up and running, but it isn't full-featured
// enough for very large programs.
//
// Before creating any windows you'll need to call dr_init_window_system(). On Win32 builds this will disable automatic
// DPI scaling and register the window classes. On the X11 backend it will open a global display which will be used for
// future API calls.
//
//
// OPTIONS
// #define these before including this file.
//
// None so far.

#ifndef dr_wnd_h
#define dr_wnd_h

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#include <windows.h>    // Unfortunate #include, but it makes things so much easier.
#endif


#define DR_WINDOW_CENTERED          0x0001
#define DR_WINDOW_FULLSCREEN        0x0002

// Common mouse buttons.
#define DR_MOUSE_BUTTON_LEFT        1
#define DR_MOUSE_BUTTON_RIGHT       2
#define DR_MOUSE_BUTTON_MIDDLE      3

// Common key codes.
#define DR_BACKSPACE                0x08
#define DR_SHIFT                    0x10
#define DR_ESCAPE                   0x1B
#define DR_PAGE_UP                  0x21
#define DR_PAGE_DOWN                0x22
#define DR_END                      0x23
#define DR_HOME                     0x24
#define DR_ARROW_LEFT               0x25
#define DR_ARROW_UP                 0x26
#define DR_ARROW_DOWN               0x27
#define DR_ARROW_RIGHT              0x28
#define DR_DELETE                   0x2E

// Key state flags.
#define DR_MOUSE_BUTTON_LEFT_DOWN   (1 << 0)
#define DR_MOUSE_BUTTON_RIGHT_DOWN  (1 << 1)
#define DR_MOUSE_BUTTON_MIDDLE_DOWN (1 << 2)
#define DR_MOUSE_BUTTON_4_DOWN      (1 << 3)
#define DR_MOUSE_BUTTON_5_DOWN      (1 << 4)
#define DR_KEY_STATE_SHIFT_DOWN     (1 << 5)        // Whether or not a shift key is down at the time the input event is handled.
#define DR_KEY_STATE_CTRL_DOWN      (1 << 6)        // Whether or not a ctrl key is down at the time the input event is handled.
#define DR_KEY_STATE_ALT_DOWN       (1 << 7)        // Whether or not an alt key is down at the time the input event is handled.
#define DR_KEY_STATE_AUTO_REPEATED  (1 << 31)       // Whether or not the key press is generated due to auto-repeating. Only used with key down events.

typedef struct dr_window dr_window;
typedef uint32_t dr_key;

typedef void (* dr_window_on_close_proc)             (dr_window* pWindow);
typedef bool (* dr_window_on_hide_proc)              (dr_window* pWindow, unsigned int flags);
typedef bool (* dr_window_on_show_proc)              (dr_window* pWindow);
typedef void (* dr_window_on_activate_proc)          (dr_window* pWindow);
typedef void (* dr_window_on_deactivate_proc)        (dr_window* pWindow);
typedef void (* dr_window_on_size_proc)              (dr_window* pWindow, unsigned int newWidth, unsigned int newHeight);
typedef void (* dr_window_on_move_proc)              (dr_window* pWindow, int newPosX, int newPosY);
typedef void (* dr_window_on_mouse_enter_proc)       (dr_window* pWindow);
typedef void (* dr_window_on_mouse_leave_proc)       (dr_window* pWindow);
typedef void (* dr_window_on_mouse_move_proc)        (dr_window* pWindow, int mousePosX, int mousePosY, unsigned int stateFlags);
typedef void (* dr_window_on_mouse_button_proc)      (dr_window* pWindow, int mouseButton, int relativeMousePosX, int relativeMousePosY, unsigned int stateFlags);
typedef void (* dr_window_on_mouse_wheel_proc)       (dr_window* pWindow, int delta, int relativeMousePosX, int relativeMousePosY, unsigned int stateFlags);
typedef void (* dr_window_on_key_down_proc)          (dr_window* pWindow, dr_key key, unsigned int stateFlags);
typedef void (* dr_window_on_key_up_proc)            (dr_window* pWindow, dr_key key, unsigned int stateFlags);
typedef void (* dr_window_on_printable_key_down_proc)(dr_window* pWindow, unsigned int character, unsigned int stateFlags);
typedef void (* dr_window_on_focus_proc)             (dr_window* pWindow);
typedef void (* dr_window_on_unfocus_proc)           (dr_window* pWindow);

typedef void (* dr_window_system_on_loop_iteration_proc)(void* pUserData);


// Main initialization routine. Must be called once, at the start of the program.
bool dr_init_window_system();

// Uninitializes the window system.
void dr_uninit_window_system();

// Runs an event driven application loop. Use this for regular desktop applications.
int dr_window_system_run();

// Runs a real-time application loop. Use this for games.
int dr_window_system_run_realtime(dr_window_system_on_loop_iteration_proc onLoopIteration, void* pUserData);

// Posts a quit message to main application loop to force it to break.
void dr_window_system_post_quit_message(int resultCode);


// The window object is transparent to make it easier to get access to things like the internal window handle.
struct dr_window
{
#ifdef _WIN32
    // The Win32 window handle.
    HWND hWnd;

    // The high-surrogate from a WM_CHAR message. This is used in order to build a surrogate pair from a couple of WM_CHAR messages. When
    // a WM_CHAR message is received when code point is not a high surrogate, this is set to 0.
    unsigned short utf16HighSurrogate;

    // Keeps track of whether or not the cursor is over this window.
    bool isCursorOver;
#endif

    // The user data associated with the window.
    void* pUserData;

    // Event handlers.
    dr_window_on_close_proc onClose;
    dr_window_on_hide_proc onHide;
    dr_window_on_show_proc onShow;
    dr_window_on_activate_proc onActivate;
    dr_window_on_deactivate_proc onDeactivate;
    dr_window_on_size_proc onSize;
    dr_window_on_move_proc onMove;
    dr_window_on_mouse_enter_proc onMouseEnter;
    dr_window_on_mouse_leave_proc onMouseLeave;
    dr_window_on_mouse_move_proc onMouseMove;
    dr_window_on_mouse_button_proc onMouseButtonDown;
    dr_window_on_mouse_button_proc onMouseButtonUp;
    dr_window_on_mouse_button_proc onMouseButtonDblClick;
    dr_window_on_mouse_wheel_proc onMouseWheel;
    dr_window_on_key_down_proc onKeyDown;
    dr_window_on_key_up_proc onKeyUp;
    dr_window_on_printable_key_down_proc onPrintableKeyDown;
    dr_window_on_focus_proc onFocus;
    dr_window_on_unfocus_proc onUnfocus;
};

// Creates a new window.
dr_window* dr_window_create(void* pUserData, const char* pTitle, unsigned int width, unsigned int height, unsigned int options);

// Deletes the given window.
void dr_window_delete(dr_window* pWindow);


// Sets the size of the window.
void dr_window_set_size(dr_window* pWindow, unsigned int newWidth, unsigned int newHeight);
void dr_window_get_size(dr_window* pWindow, unsigned int* pWidthOut, unsigned int* pHeightOut);

// Show/hide the window.
void dr_window_show(dr_window* pWindow);
void dr_window_show_maximized(dr_window* pWindow);
void dr_window_show_sized(dr_window* pWindow, unsigned int width, unsigned int height);
void dr_window_hide(dr_window* pWindow, unsigned int flags);


// Gives the given window the mouse capture.
//
// pWindow [in] The window that should receive the mouse capture. Can be null, in which case it's equivalent to dr_window_release_mouse()
void dr_window_capture_mouse(dr_window* pWindow);

// Releases the mouse.
void dr_window_release_mouse();


//// Platform Specific Public APIs ////

#ifdef _WIN32
// Retrieves a handle to the device context of the given window.
HDC dr_window_get_hdc(dr_window* pWindow);
#endif


#ifdef __cplusplus
}
#endif
#endif  //dr_wnd_h


///////////////////////////////////////////////////////////////////////////////
//
// IMPLEMENTATION
//
///////////////////////////////////////////////////////////////////////////////
#ifdef DR_WND_IMPLEMENTATION

// Event posting.
void dr_window_on_close(dr_window* pWindow)
{
    if (pWindow->onClose) {
        pWindow->onClose(pWindow);
    }
}

bool dr_window_on_hide(dr_window* pWindow, unsigned int flags)
{
    if (pWindow->onHide) {
        return pWindow->onHide(pWindow, flags);
    }

    return true;    // Returning true means to process the message as per normal.
}

bool dr_window_on_show(dr_window* pWindow)
{
    if (pWindow->onShow) {
        return pWindow->onShow(pWindow);
    }

    return true;    // Returning true means to process the message as per normal.
}

void dr_window_on_activate(dr_window* pWindow)
{
    if (pWindow->onActivate) {
        pWindow->onActivate(pWindow);
    }
}

void dr_window_on_deactivate(dr_window* pWindow)
{
    if (pWindow->onDeactivate) {
        pWindow->onDeactivate(pWindow);
    }
}

void dr_window_on_size(dr_window* pWindow, unsigned int newWidth, unsigned int newHeight)
{
    if (pWindow->onSize) {
        pWindow->onSize(pWindow, newWidth, newHeight);
    }
}

void dr_window_on_move(dr_window* pWindow, int newPosX, int newPosY)
{
    if (pWindow->onMove) {
        pWindow->onMove(pWindow, newPosX, newPosY);
    }
}

void dr_window_on_mouse_enter(dr_window* pWindow)
{
    if (pWindow->onActivate) {
        pWindow->onActivate(pWindow);
    }
}

void dr_window_on_mouse_leave(dr_window* pWindow)
{
    if (pWindow->onMouseLeave) {
        pWindow->onMouseLeave(pWindow);
    }
}

void dr_window_on_mouse_move(dr_window* pWindow, int mousePosX, int mousePosY, unsigned int stateFlags)
{
    if (pWindow->onMouseMove) {
        pWindow->onMouseMove(pWindow, mousePosX, mousePosY, stateFlags);
    }
}

void dr_window_on_mouse_button_down(dr_window* pWindow, int mouseButton, int mousePosX, int mousePosY, unsigned int stateFlags)
{
    if (pWindow->onMouseButtonDown) {
        pWindow->onMouseButtonDown(pWindow, mouseButton, mousePosX, mousePosY, stateFlags);
    }
}

void dr_window_on_mouse_button_up(dr_window* pWindow, int mouseButton, int mousePosX, int mousePosY, unsigned int stateFlags)
{
    if (pWindow->onMouseButtonUp) {
        pWindow->onMouseButtonUp(pWindow, mouseButton, mousePosX, mousePosY, stateFlags);
    }
}

void dr_window_on_mouse_button_dblclick(dr_window* pWindow, int mouseButton, int mousePosX, int mousePosY, unsigned int stateFlags)
{
    if (pWindow->onMouseButtonDblClick) {
        pWindow->onMouseButtonDblClick(pWindow, mouseButton, mousePosX, mousePosY, stateFlags);
    }
}

void dr_window_on_mouse_wheel(dr_window* pWindow, int delta, int mousePosX, int mousePosY, unsigned int stateFlags)
{
    if (pWindow->onMouseWheel) {
        pWindow->onMouseWheel(pWindow, delta, mousePosX, mousePosY, stateFlags);
    }
}

void dr_window_on_key_down(dr_window* pWindow, dr_key key, unsigned int stateFlags)
{
    if (pWindow->onKeyDown) {
        pWindow->onKeyDown(pWindow, key, stateFlags);
    }
}

void dr_window_on_key_up(dr_window* pWindow, dr_key key, unsigned int stateFlags)
{
    if (pWindow->onKeyUp) {
        pWindow->onKeyUp(pWindow, key, stateFlags);
    }
}

void dr_window_on_printable_key_down(dr_window* pWindow, unsigned int character, unsigned int stateFlags)
{
    if (pWindow->onPrintableKeyDown) {
        pWindow->onPrintableKeyDown(pWindow, character, stateFlags);
    }
}

void dr_window_on_focus(dr_window* pWindow)
{
    if (pWindow->onFocus) {
        pWindow->onFocus(pWindow);
    }
}

void dr_window_on_unfocus(dr_window* pWindow)
{
    if (pWindow->onUnfocus) {
        pWindow->onUnfocus(pWindow);
    }
}


///////////////////////////////////////////////////////////////////////////////
//
// Win32
//
///////////////////////////////////////////////////////////////////////////////
#ifdef _WIN32
static const char* g_DRWndClassName = "DRWindow";

#define GET_X_LPARAM(lp)    ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp)    ((int)(short)HIWORD(lp))

static void dr_win32_track_mouse_leave_event(HWND hWnd)
{
    TRACKMOUSEEVENT tme;
    ZeroMemory(&tme, sizeof(tme));
    tme.cbSize    = sizeof(tme);
    tme.dwFlags   = TME_LEAVE;
    tme.hwndTrack = hWnd;
    TrackMouseEvent(&tme);
}

bool dr_is_win32_mouse_button_key_code(WPARAM wParam)
{
    return wParam == VK_LBUTTON || wParam == VK_RBUTTON || wParam == VK_MBUTTON || wParam == VK_XBUTTON1 || wParam == VK_XBUTTON2;
}

dr_key dr_win32_to_dr_key(WPARAM wParam)
{
    switch (wParam)
    {
    case VK_BACK:   return DR_BACKSPACE;
    case VK_SHIFT:  return DR_SHIFT;
    case VK_ESCAPE: return DR_ESCAPE;
    case VK_PRIOR:  return DR_PAGE_UP;
    case VK_NEXT:   return DR_PAGE_DOWN;
    case VK_END:    return DR_END;
    case VK_HOME:   return DR_HOME;
    case VK_LEFT:   return DR_ARROW_LEFT;
    case VK_UP:     return DR_ARROW_UP;
    case VK_RIGHT:  return DR_ARROW_RIGHT;
    case VK_DOWN:   return DR_ARROW_DOWN;
    case VK_DELETE: return DR_DELETE;

    default: break;
    }

    return (dr_key)wParam;
}

unsigned int dr_win32_get_modifier_key_state_flags()
{
    unsigned int stateFlags = 0;

    SHORT keyState = GetAsyncKeyState(VK_SHIFT);
    if (keyState & 0x8000) {
        stateFlags |= DR_KEY_STATE_SHIFT_DOWN;
    }

    keyState = GetAsyncKeyState(VK_CONTROL);
    if (keyState & 0x8000) {
        stateFlags |= DR_KEY_STATE_CTRL_DOWN;
    }

    keyState = GetAsyncKeyState(VK_MENU);
    if (keyState & 0x8000) {
        stateFlags |= DR_KEY_STATE_ALT_DOWN;
    }

    return stateFlags;
}

unsigned int dr_win32_get_mouse_event_state_flags(WPARAM wParam)
{
    unsigned int stateFlags = 0;

    if ((wParam & MK_LBUTTON) != 0) {
        stateFlags |= DR_MOUSE_BUTTON_LEFT_DOWN;
    }

    if ((wParam & MK_RBUTTON) != 0) {
        stateFlags |= DR_MOUSE_BUTTON_RIGHT_DOWN;
    }

    if ((wParam & MK_MBUTTON) != 0) {
        stateFlags |= DR_MOUSE_BUTTON_MIDDLE_DOWN;
    }

    if ((wParam & MK_XBUTTON1) != 0) {
        stateFlags |= DR_MOUSE_BUTTON_4_DOWN;
    }

    if ((wParam & MK_XBUTTON2) != 0) {
        stateFlags |= DR_MOUSE_BUTTON_5_DOWN;
    }


    if ((wParam & MK_CONTROL) != 0) {
        stateFlags |= DR_KEY_STATE_CTRL_DOWN;
    }

    if ((wParam & MK_SHIFT) != 0) {
        stateFlags |= DR_KEY_STATE_SHIFT_DOWN;
    }


    SHORT keyState = GetAsyncKeyState(VK_MENU);
    if (keyState & 0x8000) {
        stateFlags |= DR_KEY_STATE_ALT_DOWN;
    }


    return stateFlags;
}


// This is the event handler for the main game window. The operating system will notify the application of events
// through this function, such as when the mouse is moved over a window, a key is pressed, etc.
static LRESULT DefaultWindowProcWin32(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    dr_window* pWindow = (dr_window*)GetWindowLongPtrA(hWnd, 0);
    if (pWindow != NULL)
    {
        switch (msg)
        {
            case WM_CREATE:
            {
                // This allows us to track mouse enter and leave events for the window.
                dr_win32_track_mouse_leave_event(hWnd);
                return 0;
            }

            case WM_CLOSE:
            {
                PostQuitMessage(0);
                return 0;
            }



            case WM_SIZE:
            {
                dr_window_on_size(pWindow, LOWORD(lParam), HIWORD(lParam));
                break;
            }



            case WM_LBUTTONDOWN:
            {
                dr_window_on_mouse_button_down(pWindow, DR_MOUSE_BUTTON_LEFT, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), dr_win32_get_mouse_event_state_flags(wParam) | DR_MOUSE_BUTTON_LEFT_DOWN);
                break;
            }
            case WM_LBUTTONUP:
            {
                dr_window_on_mouse_button_up(pWindow, DR_MOUSE_BUTTON_LEFT, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), dr_win32_get_mouse_event_state_flags(wParam));
                break;
            }
            case WM_LBUTTONDBLCLK:
            {
                dr_window_on_mouse_button_down(pWindow, DR_MOUSE_BUTTON_LEFT, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), dr_win32_get_mouse_event_state_flags(wParam) | DR_MOUSE_BUTTON_LEFT_DOWN);
                dr_window_on_mouse_button_dblclick(pWindow, DR_MOUSE_BUTTON_LEFT, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), dr_win32_get_mouse_event_state_flags(wParam) | DR_MOUSE_BUTTON_LEFT_DOWN);
                break;
            }


            case WM_RBUTTONDOWN:
            {
                dr_window_on_mouse_button_down(pWindow, DR_MOUSE_BUTTON_RIGHT, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), dr_win32_get_mouse_event_state_flags(wParam) | DR_MOUSE_BUTTON_RIGHT_DOWN);
                break;
            }
            case WM_RBUTTONUP:
            {
                dr_window_on_mouse_button_up(pWindow, DR_MOUSE_BUTTON_RIGHT, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), dr_win32_get_mouse_event_state_flags(wParam));
                break;
            }
            case WM_RBUTTONDBLCLK:
            {
                dr_window_on_mouse_button_down(pWindow, DR_MOUSE_BUTTON_RIGHT, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), dr_win32_get_mouse_event_state_flags(wParam) | DR_MOUSE_BUTTON_RIGHT_DOWN);
                dr_window_on_mouse_button_dblclick(pWindow, DR_MOUSE_BUTTON_RIGHT, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), dr_win32_get_mouse_event_state_flags(wParam) | DR_MOUSE_BUTTON_RIGHT_DOWN);
                break;
            }


            case WM_MBUTTONDOWN:
            {
                dr_window_on_mouse_button_down(pWindow, DR_MOUSE_BUTTON_MIDDLE, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), dr_win32_get_mouse_event_state_flags(wParam) | DR_MOUSE_BUTTON_MIDDLE_DOWN);
                break;
            }
            case WM_MBUTTONUP:
            {
                dr_window_on_mouse_button_up(pWindow, DR_MOUSE_BUTTON_MIDDLE, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), dr_win32_get_mouse_event_state_flags(wParam));
                break;
            }
            case WM_MBUTTONDBLCLK:
            {
                dr_window_on_mouse_button_down(pWindow, DR_MOUSE_BUTTON_MIDDLE, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), dr_win32_get_mouse_event_state_flags(wParam) | DR_MOUSE_BUTTON_MIDDLE_DOWN);
                dr_window_on_mouse_button_dblclick(pWindow, DR_MOUSE_BUTTON_MIDDLE, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), dr_win32_get_mouse_event_state_flags(wParam) | DR_MOUSE_BUTTON_MIDDLE_DOWN);
                break;
            }


            case WM_MOUSEWHEEL:
            {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;

                POINT p;
                p.x = GET_X_LPARAM(lParam);
                p.y = GET_Y_LPARAM(lParam);
                ScreenToClient(hWnd, &p);

                dr_window_on_mouse_wheel(pWindow, delta, p.x, p.y, dr_win32_get_mouse_event_state_flags(wParam));
                break;
            }


            case WM_MOUSELEAVE:
            {
                pWindow->isCursorOver = false;

                dr_window_on_mouse_leave(pWindow);
                break;
            }

            case WM_MOUSEMOVE:
            {
                // On Win32 we need to explicitly tell the operating system to post a WM_MOUSELEAVE event. The problem is that it needs to be re-issued when the
                // mouse re-enters the window. The easiest way to do this is to just call it in response to every WM_MOUSEMOVE event.
                if (!pWindow->isCursorOver)
                {
                    dr_win32_track_mouse_leave_event(hWnd);
                    pWindow->isCursorOver = true;

                    dr_window_on_mouse_enter(pWindow);
                }

                dr_window_on_mouse_move(pWindow, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), dr_win32_get_mouse_event_state_flags(wParam));
                break;
            }


            case WM_KEYDOWN:
            {
                if (!dr_is_win32_mouse_button_key_code(wParam))
                {
                    unsigned int stateFlags = dr_win32_get_modifier_key_state_flags();
                    if ((lParam & (1 << 30)) != 0) {
                        stateFlags |= DR_KEY_STATE_AUTO_REPEATED;
                    }

                    dr_window_on_key_down(pWindow, dr_win32_to_dr_key(wParam), stateFlags);
                }

                break;
            }

            case WM_KEYUP:
            {
                if (!dr_is_win32_mouse_button_key_code(wParam))
                {
                    unsigned int stateFlags = dr_win32_get_modifier_key_state_flags();
                    dr_window_on_key_up(pWindow, dr_win32_to_dr_key(wParam), stateFlags);
                }

                break;
            }

            // NOTE: WM_UNICHAR is not posted by Windows itself, but rather intended to be posted by applications. Thus, we need to use WM_CHAR. WM_CHAR
            //       posts events as UTF-16 code points. When the code point is a surrogate pair, we need to store it and wait for the next WM_CHAR event
            //       which will contain the other half of the pair.
            case WM_CHAR:
            {
                // Windows will post WM_CHAR events for keys we don't particularly want. We'll filter them out here (they will be processed by WM_KEYDOWN).
                if (wParam < 32 || wParam == 127)       // 127 = ASCII DEL (will be triggered by CTRL+Backspace)
                {
                    if (wParam != VK_TAB  &&
                        wParam != VK_RETURN)    // VK_RETURN = Enter Key.
                    {
                        break;
                    }
                }


                if ((lParam & (1U << 31)) == 0)     // Bit 31 will be 1 if the key was pressed, 0 if it was released.
                {
                    if (IS_HIGH_SURROGATE(wParam))
                    {
                        assert(pWindow->utf16HighSurrogate == 0);
                        pWindow->utf16HighSurrogate = (unsigned short)wParam;
                    }
                    else
                    {
                        unsigned int character = (unsigned int)wParam;
                        if (IS_LOW_SURROGATE(wParam))
                        {
                            assert(IS_HIGH_SURROGATE(pWindow->utf16HighSurrogate) != 0);
                            character = dr_utf16pair_to_utf32_ch(pWindow->utf16HighSurrogate, (unsigned short)wParam);
                        }

                        pWindow->utf16HighSurrogate = 0;


                        int repeatCount = lParam & 0x0000FFFF;
                        for (int i = 0; i < repeatCount; ++i)
                        {
                            unsigned int stateFlags = dr_win32_get_modifier_key_state_flags();
                            if ((lParam & (1 << 30)) != 0) {
                                stateFlags |= DR_KEY_STATE_AUTO_REPEATED;
                            }

                            dr_window_on_printable_key_down(pWindow, character, stateFlags);
                        }
                    }
                }

                break;
            }


            default: break;
        }
    }

    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

bool dr_init_window_system()
{
    // The Windows operating system likes to automatically change the size of the game window when DPI scaling is
    // used. For example, if the user has their DPI set to 200%, the operating system will try to be helpful and
    // automatically resize every window by 200%. The size of the window controls the resolution the game runs at,
    // but we want that resolution to be set explicitly via something like an options menu. Thus, we don't want the
    // operating system to be changing the size of the window to anything other than what we explicitly request. To
    // do this, we just tell the operation system that it shouldn't do DPI scaling and that we'll do it ourselves
    // manually.
    dr_win32_make_dpi_aware();

    WNDCLASSEXA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.cbWndExtra    = sizeof(void*);
    wc.lpfnWndProc   = (WNDPROC)DefaultWindowProcWin32;
    wc.lpszClassName = g_DRWndClassName;
    wc.hCursor       = LoadCursorA(NULL, MAKEINTRESOURCEA(32512));
    wc.style         = CS_OWNDC | CS_DBLCLKS;
    if (!RegisterClassExA(&wc)) {
        return false;
    }

    return true;
}

void dr_uninit_window_system()
{
    UnregisterClassA(g_DRWndClassName, GetModuleHandleA(NULL));
}

int dr_window_system_run()
{
    MSG msg;
    BOOL bRet;
    while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
    {
        if (bRet == -1) {
            return -42; // Unknown error.
        }

        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return 0;
}

int dr_window_system_run_realtime(dr_window_system_on_loop_iteration_proc onLoopIteration, void* pUserData)
{
    for (;;)
    {
        // Handle window events.
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) {
                return msg.wParam;  // Received a quit message.
            }

            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        // After handling the next event in the queue we let the game know it should do the next frame.
        onLoopIteration(pUserData);
    }

    return 0;
}

void dr_window_system_post_quit_message(int resultCode)
{
    PostQuitMessage(resultCode);
}


dr_window* dr_window_create(void* pUserData, const char* pTitle, unsigned int resolutionX, unsigned int resolutionY, unsigned int options)
{
    dr_window* pWindow = (dr_window*)calloc(1, sizeof(*pWindow));
    if (pWindow == NULL) {
        return NULL;
    }

    pWindow->pUserData = pUserData;


    DWORD dwExStyle = 0;
    DWORD dwStyle = WS_OVERLAPPEDWINDOW;
    pWindow->hWnd = CreateWindowExA(dwExStyle, g_DRWndClassName, pTitle, dwStyle, CW_USEDEFAULT, CW_USEDEFAULT, resolutionX, resolutionY, NULL, NULL, NULL, NULL);
    if (pWindow->hWnd == NULL) {
        goto on_error;
    }

    // We should have a window, but before showing it we need to make a few small adjustments to the position and size. First, we need to
    // honour the TA_WINDOW_CENTERED option. Second, we need to make a small change to the size of the window such that the client size
    // is equal to resolutionX and resolutionY. When we created the window, we specified resolutionX and resolutionY as the dimensions,
    // however this includes the size of the outer border. The outer border should not be included, so we need to stretch the window just
    // a little bit such that the area inside the borders are exactly equal to resolutionX and resolutionY.
    //
    // We use a single API to both move and resize the window.

    UINT swpflags = SWP_NOZORDER | SWP_NOMOVE;

    RECT windowRect;
    RECT clientRect;
    GetWindowRect(pWindow->hWnd, &windowRect);
    GetClientRect(pWindow->hWnd, &clientRect);

    int windowWidth  = (int)resolutionX + ((windowRect.right - windowRect.left) - (clientRect.right - clientRect.left));
    int windowHeight = (int)resolutionY + ((windowRect.bottom - windowRect.top) - (clientRect.bottom - clientRect.top));

    int windowPosX = 0;
    int windowPosY = 0;
    if ((options & DR_WINDOW_CENTERED) != 0)
    {
        // We need to center the window. To do this properly, we want to reposition based on the monitor it started on.
        MONITORINFO mi;
        ZeroMemory(&mi, sizeof(mi));
        mi.cbSize = sizeof(MONITORINFO);
        if (GetMonitorInfoA(MonitorFromWindow(pWindow->hWnd, 0), &mi))
        {
            windowPosX = ((mi.rcMonitor.right - mi.rcMonitor.left) - windowWidth)  / 2;
            windowPosY = ((mi.rcMonitor.bottom - mi.rcMonitor.top) - windowHeight) / 2;

            swpflags &= ~SWP_NOMOVE;
        }
    }

    SetWindowPos(pWindow->hWnd, NULL, windowPosX, windowPosY, windowWidth, windowHeight, swpflags);


    // The window has been position and sized correctly, but now we need to attach our own window object to the internal Win32 object.
    SetWindowLongPtrA(pWindow->hWnd, 0, (LONG_PTR)pWindow);

    // Finally we can show our window.
    ShowWindow(pWindow->hWnd, SW_SHOWNORMAL);


    return pWindow;

on_error:
    dr_window_delete(pWindow);
    return NULL;
}

void dr_window_delete(dr_window* pWindow)
{
    if (pWindow == NULL) {
        return;
    }

    DestroyWindow(pWindow->hWnd);
    free(pWindow);
}


void dr_window_set_size(dr_window* pWindow, unsigned int newWidth, unsigned int newHeight)
{
    RECT windowRect;
    RECT clientRect;
    GetWindowRect(pWindow->hWnd, &windowRect);
    GetClientRect(pWindow->hWnd, &clientRect);

    int windowFrameX = (windowRect.right - windowRect.left) - (clientRect.right - clientRect.left);
    int windowFrameY = (windowRect.bottom - windowRect.top) - (clientRect.bottom - clientRect.top);

    assert(windowFrameX >= 0);
    assert(windowFrameY >= 0);

    int scaledWidth  = newWidth  + windowFrameX;
    int scaledHeight = newHeight + windowFrameY;
    SetWindowPos(pWindow->hWnd, NULL, 0, 0, scaledWidth, scaledHeight, SWP_NOZORDER | SWP_NOMOVE);
}

void dr_window_get_size(dr_window* pWindow, unsigned int* pWidthOut, unsigned int* pHeightOut)
{
    RECT rect;
    GetClientRect(pWindow->hWnd, &rect);

    if (pWidthOut != NULL) {
        *pWidthOut = rect.right - rect.left;
    }
    if (pHeightOut != NULL) {
        *pHeightOut = rect.bottom - rect.top;
    }
}


void dr_window_show(dr_window* pWindow)
{
    ShowWindow(pWindow->hWnd, SW_SHOWNORMAL);
}

void dr_window_show_maximized(dr_window* pWindow)
{
    ShowWindow(pWindow->hWnd, SW_SHOWMAXIMIZED);
}

void dr_window_hide(dr_window* pWindow, unsigned int flags)
{
    ShowWindow(pWindow->hWnd, SW_HIDE);
}


void dr_window_capture_mouse(dr_window* pWindow)
{
    if (pWindow == NULL) {
        dr_window_release_mouse();
    }

    SetCapture(pWindow->hWnd);
}

void dr_window_release_mouse()
{
    ReleaseCapture();
}



HDC dr_window_get_hdc(dr_window* pWindow)
{
    if (pWindow == NULL) {
        return NULL;
    }

    return GetDC(pWindow->hWnd);
}
#endif  //_WIN32



///////////////////////////////////////////////////////////////////////////////
//
// Linux
//
///////////////////////////////////////////////////////////////////////////////
#ifdef __linux__
#endif  //__linux__



///////////////////////////////////////////////////////////////////////////////
//
// Cross Platform
//
///////////////////////////////////////////////////////////////////////////////
void dr_window_show_sized(dr_window* pWindow, unsigned int width, unsigned int height)
{
    dr_window_set_size(pWindow, width, height);
    dr_window_show(pWindow);
}

#endif  //DR_WND_IMPLEMENTATION


/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
*/
