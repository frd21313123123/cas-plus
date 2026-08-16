// cas+ shared UI interaction state.
//
// This prelude is injected before ui_redesign.inc so every redesigned control
// can query the same menu-local mouse state.  It never reads game input and it
// never hooks the renderer/game thread; state is fed only by MenuWindowProc.

static int g_casUiMouseX = -32768;
static int g_casUiMouseY = -32768;
static bool g_casUiMouseDown = false;
static const wchar_t* g_casUiTooltipText = nullptr;
static int g_casUiTooltipPriority = -1;

static bool CasUiMouseOver(int x, int y, int w, int h)
{
    return g_casUiMouseX >= x && g_casUiMouseX < x + w &&
        g_casUiMouseY >= y && g_casUiMouseY < y + h;
}

static void CasUiSetMouseFromMessage(LPARAM lParam)
{
    g_casUiMouseX = static_cast<int>(static_cast<short>(lParam & 0xFFFF));
    g_casUiMouseY = static_cast<int>(static_cast<short>((lParam >> 16) & 0xFFFF));
}

static void CasUiRefreshMouse(HWND wnd)
{
    if (!wnd)
        return;
    POINT p{};
    if (!GetCursorPos(&p) || !ScreenToClient(wnd, &p))
        return;
    g_casUiMouseX = static_cast<int>(p.x);
    g_casUiMouseY = static_cast<int>(p.y);
}

static void CasUiBeginInteractiveFrame()
{
    g_casUiTooltipText = nullptr;
    g_casUiTooltipPriority = -1;
}

static void CasUiTooltipCandidate(int x, int y, int w, int h,
    const wchar_t* text, int priority = 0)
{
    if (!text || !*text || priority < g_casUiTooltipPriority ||
        !CasUiMouseOver(x, y, w, h))
        return;
    g_casUiTooltipText = text;
    g_casUiTooltipPriority = priority;
}
