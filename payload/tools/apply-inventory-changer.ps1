param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8
$runtimePath = Join-Path $PSScriptRoot '..\src\inventory\inventory_changer.inc'
if (-not (Test-Path -LiteralPath $runtimePath)) {
    throw "Inventory changer module was not found: $runtimePath"
}
$runtime = Get-Content -LiteralPath $runtimePath -Raw -Encoding UTF8

# Keep the game-facing runtime isolated from dllmain.cpp. The generated source
# receives the module immediately before FrameStageNotifyHook, where all schema,
# entity and UI helpers it depends on are already defined.
$hookAnchor = 'static void FrameStageNotifyHook(void* client, int stage)'
$hookIndex = $source.IndexOf($hookAnchor)
if ($hookIndex -lt 0) {
    throw 'FrameStageNotifyHook anchor was not found. Refusing to patch blindly.'
}
$source = $source.Substring(0, $hookIndex) + $runtime + "`r`n`r`n" + $source.Substring($hookIndex)

$frameAnchor = @'
    g_originalFrameStageNotify(client, stage);
    if (stage == FRAME_RENDER_PASS)
    {
        const LONG botRequest = AtomicExchange(
'@
$frameReplacement = @'
    g_originalFrameStageNotify(client, stage);
    if (stage == FRAME_RENDER_PASS)
    {
        UpdateInventoryChanger();
        const LONG botRequest = AtomicExchange(
'@
if (-not $source.Contains($frameAnchor)) {
    throw 'Render-frame inventory update anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($frameAnchor, $frameReplacement)

# Inventory replaces the old Misc placeholder tab. This keeps the menu at five
# tabs and therefore does not disturb the existing hit-testing geometry.
$tabsAnchor = 'const wchar_t* tabs[] = { L"Ragebot", L"Anti-Aim", L"Visuals", L"Misc", L"Configs" };'
$tabsReplacement = 'const wchar_t* tabs[] = { L"Ragebot", L"Anti-Aim", L"Visuals", L"Inventory", L"Configs" };'
if (-not $source.Contains($tabsAnchor)) {
    throw 'Menu tab anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($tabsAnchor, $tabsReplacement)

$ui = @'
static void DrawInventoryNumber(HDC hdc, unsigned int value, RECT* rc,
    UINT flags = DT_CENTER | DT_VCENTER | DT_SINGLELINE)
{
    WideStatusBuilder text;
    text.length = 0;
    text.text[0] = 0;
    AppendStatusUnsigned(&text, value);
    DrawTextW(hdc, text.text, -1, rc, flags);
}

static void DrawInventoryButton(HDC hdc, int x, int y, int w, int h,
    const wchar_t* label, bool active = false)
{
    DrawRoundedCard(hdc, x, y, w, h,
        active ? RGB_COLOR(37, 99, 235) : RGB_COLOR(39, 39, 45),
        RGB_COLOR(60, 60, 70), 5);
    SetTextColor(hdc, active ? RGB_COLOR(255, 255, 255) :
        RGB_COLOR(212, 212, 216));
    RECT rc = { x, y, x + w, y + h };
    DrawTextW(hdc, label, -1, &rc,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void DrawInventoryItemRow(HDC hdc, int y,
    const VirtualInventoryItem& item, int absoluteIndex,
    bool selected)
{
    DrawRoundedCard(hdc, 35, y, 290, 38,
        selected ? RGB_COLOR(36, 48, 70) : RGB_COLOR(30, 30, 34),
        selected ? RGB_COLOR(59, 130, 246) : RGB_COLOR(50, 50, 58), 5);

    SetTextColor(hdc, RGB_COLOR(228, 228, 231));
    RECT idxRc = { 45, y, 76, y + 38 };
    DrawInventoryNumber(hdc, static_cast<unsigned int>(absoluteIndex + 1),
        &idxRc);

    RECT defRc = { 82, y + 3, 170, y + 20 };
    DrawTextW(hdc,
        item.slotDefinitionIndex == INVENTORY_SLOT_KNIFE ? L"Knife" : L"Weapon",
        -1, &defRc, DT_LEFT | DT_SINGLELINE);
    SetTextColor(hdc, RGB_COLOR(161, 161, 170));
    RECT idRc = { 82, y + 20, 170, y + 36 };
    DrawInventoryNumber(hdc,
        static_cast<unsigned int>(item.overrideDefinitionIndex), &idRc,
        DT_LEFT | DT_SINGLELINE);

    SetTextColor(hdc, RGB_COLOR(228, 228, 231));
    RECT paintRc = { 180, y + 3, 250, y + 20 };
    DrawTextW(hdc, L"Paint", -1, &paintRc, DT_LEFT | DT_SINGLELINE);
    SetTextColor(hdc, RGB_COLOR(161, 161, 170));
    RECT paintValue = { 180, y + 20, 250, y + 36 };
    DrawInventoryNumber(hdc,
        static_cast<unsigned int>(item.paintKit), &paintValue,
        DT_LEFT | DT_SINGLELINE);

    const wchar_t* team = L"-";
    if (item.equippedTeams == INVENTORY_TEAM_T) team = L"T";
    else if (item.equippedTeams == INVENTORY_TEAM_CT) team = L"CT";
    else if (item.equippedTeams == INVENTORY_TEAM_BOTH) team = L"T+CT";
    SetTextColor(hdc, item.equippedTeams ?
        RGB_COLOR(132, 204, 22) : RGB_COLOR(113, 113, 122));
    RECT teamRc = { 255, y, 315, y + 38 };
    DrawTextW(hdc, team, -1, &teamRc,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void DrawInventoryChangerPanel(HDC hdc)
{
    InventoryUiSnapshot snapshot;
    const bool haveSnapshot = InventoryGetUiSnapshot(&snapshot);

    DrawRoundedCard(hdc, 20, 50, 735, 430,
        RGB_COLOR(24, 24, 27), RGB_COLOR(39, 39, 42), 8);

    HFONT titleFont = CreateFontW(16, 0, 0, 0, 600, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HGDIOBJ oldFont = SelectObject(hdc, titleFont);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB_COLOR(255, 255, 255));
    RECT title = { 35, 64, 300, 88 };
    DrawTextW(hdc, L"VIRTUAL INVENTORY", -1, &title,
        DT_LEFT | DT_SINGLELINE);

    HFONT rowFont = CreateFontW(13, 0, 0, 0, 400, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SelectObject(hdc, rowFont);

    if (!haveSnapshot)
    {
        SetTextColor(hdc, RGB_COLOR(161, 161, 170));
        RECT busy = { 35, 100, 720, 130 };
        DrawTextW(hdc, L"Inventory is being updated on the game thread...",
            -1, &busy, DT_LEFT | DT_SINGLELINE);
        SelectObject(hdc, oldFont);
        DeleteObject(titleFont);
        DeleteObject(rowFont);
        return;
    }

    DrawToggleSwitch(hdc, 690, 65, snapshot.enabled);
    SetTextColor(hdc, RGB_COLOR(161, 161, 170));
    RECT stateRc = { 560, 66, 682, 86 };
    DrawTextW(hdc, snapshot.enabled ? L"Changer enabled" : L"Changer disabled",
        -1, &stateRc, DT_RIGHT | DT_SINGLELINE);

    // Left: persistent virtual item instances.
    SetTextColor(hdc, RGB_COLOR(161, 161, 170));
    RECT listTitle = { 35, 98, 320, 118 };
    DrawTextW(hdc, L"ITEMS", -1, &listTitle, DT_LEFT | DT_SINGLELINE);

    int rowY = 122;
    for (int i = 0; i < snapshot.visibleCount; ++i)
    {
        DrawInventoryItemRow(hdc, rowY,
            snapshot.visibleItems[i], snapshot.visibleIndices[i],
            snapshot.visibleIndices[i] == snapshot.selectedIndex);
        rowY += 42;
    }
    if (snapshot.visibleCount == 0)
    {
        SetTextColor(hdc, RGB_COLOR(113, 113, 122));
        RECT empty = { 45, 140, 315, 195 };
        DrawTextW(hdc,
            L"No virtual items yet. Equip a weapon in-game and press Add current.",
            -1, &empty, DT_LEFT);
    }

    DrawInventoryButton(hdc, 35, 425, 92, 30, L"< Page");
    DrawInventoryButton(hdc, 133, 425, 92, 30, L"Page >");
    DrawInventoryButton(hdc, 231, 425, 94, 30, L"Add current");

    // Right: editor / loadout assignment.
    DrawRoundedCard(hdc, 345, 98, 395, 357,
        RGB_COLOR(28, 28, 32), RGB_COLOR(45, 45, 52), 6);
    SetTextColor(hdc, RGB_COLOR(161, 161, 170));
    RECT editorTitle = { 360, 112, 720, 132 };
    DrawTextW(hdc, L"SELECTED ITEM / LOADOUT", -1, &editorTitle,
        DT_LEFT | DT_SINGLELINE);

    SetTextColor(hdc, RGB_COLOR(113, 113, 122));
    RECT runtime = { 360, 136, 720, 156 };
    DrawTextW(hdc, L"Active def / applied / owned:", -1, &runtime,
        DT_LEFT | DT_SINGLELINE);
    SetTextColor(hdc, RGB_COLOR(212, 212, 216));
    RECT activeDef = { 555, 136, 605, 156 };
    DrawInventoryNumber(hdc, snapshot.activeDefinitionIndex, &activeDef,
        DT_LEFT | DT_SINGLELINE);
    RECT applied = { 610, 136, 650, 156 };
    DrawInventoryNumber(hdc,
        static_cast<unsigned int>(snapshot.appliedCount), &applied,
        DT_LEFT | DT_SINGLELINE);
    RECT owned = { 660, 136, 705, 156 };
    DrawInventoryNumber(hdc,
        static_cast<unsigned int>(snapshot.ownedEconCount), &owned,
        DT_LEFT | DT_SINGLELINE);

    if (!snapshot.hasSelectedItem)
    {
        SetTextColor(hdc, RGB_COLOR(113, 113, 122));
        RECT none = { 360, 180, 720, 210 };
        DrawTextW(hdc, L"Select an item on the left.", -1, &none,
            DT_LEFT | DT_SINGLELINE);
    }
    else
    {
        const VirtualInventoryItem& item = snapshot.selectedItem;
        SetTextColor(hdc, RGB_COLOR(228, 228, 231));
        RECT defLabel = { 360, 168, 455, 188 };
        DrawTextW(hdc, L"Definition", -1, &defLabel,
            DT_LEFT | DT_SINGLELINE);
        RECT defValue = { 460, 168, 520, 188 };
        DrawInventoryNumber(hdc, item.overrideDefinitionIndex, &defValue,
            DT_LEFT | DT_SINGLELINE);
        if (item.slotDefinitionIndex == INVENTORY_SLOT_KNIFE)
        {
            DrawInventoryButton(hdc, 530, 163, 44, 28, L"Bay", item.overrideDefinitionIndex == 500);
            DrawInventoryButton(hdc, 578, 163, 44, 28, L"Flip", item.overrideDefinitionIndex == 505);
            DrawInventoryButton(hdc, 626, 163, 44, 28, L"Kara", item.overrideDefinitionIndex == 507);
            DrawInventoryButton(hdc, 674, 163, 50, 28, L"Butter", item.overrideDefinitionIndex == 515);
        }

        SetTextColor(hdc, RGB_COLOR(228, 228, 231));
        RECT paintLabel = { 360, 210, 430, 230 };
        DrawTextW(hdc, L"Paint kit", -1, &paintLabel,
            DT_LEFT | DT_SINGLELINE);
        RECT paintValue = { 435, 210, 500, 230 };
        DrawInventoryNumber(hdc,
            static_cast<unsigned int>(item.paintKit), &paintValue,
            DT_LEFT | DT_SINGLELINE);
        DrawInventoryButton(hdc, 510, 203, 48, 28, L"-100");
        DrawInventoryButton(hdc, 562, 203, 44, 28, L"-1");
        DrawInventoryButton(hdc, 610, 203, 44, 28, L"+1");
        DrawInventoryButton(hdc, 658, 203, 52, 28, L"+100");

        SetTextColor(hdc, RGB_COLOR(228, 228, 231));
        RECT seedLabel = { 360, 248, 430, 268 };
        DrawTextW(hdc, L"Seed", -1, &seedLabel,
            DT_LEFT | DT_SINGLELINE);
        RECT seedValue = { 435, 248, 500, 268 };
        DrawInventoryNumber(hdc,
            static_cast<unsigned int>(item.seed), &seedValue,
            DT_LEFT | DT_SINGLELINE);
        DrawInventoryButton(hdc, 510, 241, 48, 28, L"-10");
        DrawInventoryButton(hdc, 562, 241, 44, 28, L"-1");
        DrawInventoryButton(hdc, 610, 241, 44, 28, L"+1");
        DrawInventoryButton(hdc, 658, 241, 52, 28, L"+10");

        SetTextColor(hdc, RGB_COLOR(228, 228, 231));
        RECT wearLabel = { 360, 286, 430, 306 };
        DrawTextW(hdc, L"Wear", -1, &wearLabel,
            DT_LEFT | DT_SINGLELINE);
        DrawInventoryButton(hdc, 435, 279, 56, 28, L"FN", item.wear < 0.07f);
        DrawInventoryButton(hdc, 495, 279, 56, 28, L"MW", item.wear >= 0.07f && item.wear < 0.15f);
        DrawInventoryButton(hdc, 555, 279, 56, 28, L"FT", item.wear >= 0.15f && item.wear < 0.38f);
        DrawInventoryButton(hdc, 615, 279, 56, 28, L"WW", item.wear >= 0.38f && item.wear < 0.45f);
        DrawInventoryButton(hdc, 675, 279, 49, 28, L"BS", item.wear >= 0.45f);

        SetTextColor(hdc, RGB_COLOR(228, 228, 231));
        RECT stLabel = { 360, 326, 430, 346 };
        DrawTextW(hdc, L"StatTrak", -1, &stLabel,
            DT_LEFT | DT_SINGLELINE);
        DrawToggleSwitch(hdc, 440, 322, item.statTrak >= 0);

        SetTextColor(hdc, RGB_COLOR(228, 228, 231));
        RECT equipLabel = { 360, 366, 430, 386 };
        DrawTextW(hdc, L"Equip", -1, &equipLabel,
            DT_LEFT | DT_SINGLELINE);
        DrawInventoryButton(hdc, 435, 359, 58, 28, L"None", item.equippedTeams == INVENTORY_TEAM_NONE);
        DrawInventoryButton(hdc, 497, 359, 58, 28, L"T", item.equippedTeams == INVENTORY_TEAM_T);
        DrawInventoryButton(hdc, 559, 359, 58, 28, L"CT", item.equippedTeams == INVENTORY_TEAM_CT);
        DrawInventoryButton(hdc, 621, 359, 70, 28, L"T + CT", item.equippedTeams == INVENTORY_TEAM_BOTH);

        DrawInventoryButton(hdc, 360, 407, 128, 30, L"Delete item");
        SetTextColor(hdc, RGB_COLOR(113, 113, 122));
        RECT localNote = { 500, 405, 720, 440 };
        DrawTextW(hdc,
            L"Local cosmetic/loadout state only; Steam inventory is untouched.",
            -1, &localNote, DT_LEFT);
    }

    SelectObject(hdc, oldFont);
    DeleteObject(titleFont);
    DeleteObject(rowFont);
}

'@
$menuProcAnchor = 'static LRESULT CALLBACK MenuWindowProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)'
$menuProcIndex = $source.IndexOf($menuProcAnchor)
if ($menuProcIndex -lt 0) {
    throw 'MenuWindowProc anchor was not found. Refusing to patch blindly.'
}
$source = $source.Substring(0, $menuProcIndex) + $ui + $source.Substring($menuProcIndex)

$paintAnchor = @'
        // Left Card: "ESP"
        DrawRoundedCard(memDC, 20, 50, 360, 430, RGB_COLOR(24, 24, 27), RGB_COLOR(39, 39, 42), 8);
'@
$paintReplacement = @'
        if (g_espConfig.selectedTab == 3)
        {
            DrawInventoryChangerPanel(memDC);
            SelectObject(memDC, oldFont);
            DeleteObject(tabFont);
            BitBlt(hdc, 0, 0, w, h, memDC, 0, 0,
                0x00CC0020 /* SRCCOPY */);
            SelectObject(memDC, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDC);
            EndPaint(wnd, &ps);
            return 0;
        }

        // Left Card: "ESP"
        DrawRoundedCard(memDC, 20, 50, 360, 430, RGB_COLOR(24, 24, 27), RGB_COLOR(39, 39, 42), 8);
'@
if (-not $source.Contains($paintAnchor)) {
    throw 'Inventory paint anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($paintAnchor, $paintReplacement)

$clickAnchor = @'
        // Manage modal click check
        if (g_espConfig.showManageModal)
'@
$clickReplacement = @'
        if (g_espConfig.selectedTab == 3)
        {
            InventoryUiSnapshot inv;
            InventoryGetUiSnapshot(&inv);

            if (mouseX >= 684 && mouseX <= 735 &&
                mouseY >= 60 && mouseY <= 92)
            {
                InventoryUiToggleEnabled();
            }
            else if (mouseX >= 35 && mouseX <= 325 &&
                mouseY >= 122 && mouseY < 416)
            {
                const int row = (mouseY - 122) / 42;
                if (row >= 0 && row < inv.visibleCount &&
                    mouseY <= 122 + row * 42 + 38)
                    InventoryUiSelect(inv.visibleIndices[row]);
            }
            else if (mouseY >= 420 && mouseY <= 460)
            {
                if (mouseX >= 30 && mouseX < 130)
                    InventoryUiChangePage(-1);
                else if (mouseX >= 130 && mouseX < 230)
                    InventoryUiChangePage(1);
                else if (mouseX >= 230 && mouseX <= 330)
                    InventoryUiRequestAddCurrent();
            }
            else if (inv.hasSelectedItem &&
                mouseY >= 158 && mouseY <= 195 &&
                inv.selectedItem.slotDefinitionIndex == INVENTORY_SLOT_KNIFE)
            {
                if (mouseX >= 525 && mouseX < 576)
                    InventoryUiSetDefinition(500);
                else if (mouseX >= 576 && mouseX < 624)
                    InventoryUiSetDefinition(505);
                else if (mouseX >= 624 && mouseX < 672)
                    InventoryUiSetDefinition(507);
                else if (mouseX >= 672 && mouseX <= 730)
                    InventoryUiSetDefinition(515);
            }
            else if (inv.hasSelectedItem && mouseY >= 198 && mouseY <= 235)
            {
                if (mouseX >= 505 && mouseX < 560)
                    InventoryUiAdjustPaint(-100);
                else if (mouseX >= 560 && mouseX < 608)
                    InventoryUiAdjustPaint(-1);
                else if (mouseX >= 608 && mouseX < 656)
                    InventoryUiAdjustPaint(1);
                else if (mouseX >= 656 && mouseX <= 715)
                    InventoryUiAdjustPaint(100);
            }
            else if (inv.hasSelectedItem && mouseY >= 236 && mouseY <= 273)
            {
                if (mouseX >= 505 && mouseX < 560)
                    InventoryUiAdjustSeed(-10);
                else if (mouseX >= 560 && mouseX < 608)
                    InventoryUiAdjustSeed(-1);
                else if (mouseX >= 608 && mouseX < 656)
                    InventoryUiAdjustSeed(1);
                else if (mouseX >= 656 && mouseX <= 715)
                    InventoryUiAdjustSeed(10);
            }
            else if (inv.hasSelectedItem && mouseY >= 274 && mouseY <= 312)
            {
                if (mouseX >= 430 && mouseX < 493)
                    InventoryUiSetWear(0.0001f);
                else if (mouseX >= 493 && mouseX < 553)
                    InventoryUiSetWear(0.08f);
                else if (mouseX >= 553 && mouseX < 613)
                    InventoryUiSetWear(0.20f);
                else if (mouseX >= 613 && mouseX < 673)
                    InventoryUiSetWear(0.40f);
                else if (mouseX >= 673 && mouseX <= 730)
                    InventoryUiSetWear(0.60f);
            }
            else if (inv.hasSelectedItem && mouseX >= 430 && mouseX <= 490 &&
                mouseY >= 316 && mouseY <= 352)
            {
                InventoryUiToggleStatTrak();
            }
            else if (inv.hasSelectedItem && mouseY >= 354 && mouseY <= 392)
            {
                if (mouseX >= 430 && mouseX < 495)
                    InventoryUiSetEquipMask(INVENTORY_TEAM_NONE);
                else if (mouseX >= 495 && mouseX < 557)
                    InventoryUiSetEquipMask(INVENTORY_TEAM_T);
                else if (mouseX >= 557 && mouseX < 619)
                    InventoryUiSetEquipMask(INVENTORY_TEAM_CT);
                else if (mouseX >= 619 && mouseX <= 698)
                    InventoryUiSetEquipMask(INVENTORY_TEAM_BOTH);
            }
            else if (inv.hasSelectedItem && mouseX >= 355 && mouseX <= 493 &&
                mouseY >= 400 && mouseY <= 442)
            {
                InventoryUiDeleteSelected();
            }
            else
            {
                return 0;
            }

            InvalidateRect(wnd, nullptr, FALSE);
            return 0;
        }

        // Manage modal click check
        if (g_espConfig.showManageModal)
'@
if (-not $source.Contains($clickAnchor)) {
    throw 'Inventory click anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($clickAnchor, $clickReplacement)

# Load persistent virtual items on the payload worker before the frame bridge is
# installed. File I/O never runs from CS2's render callback.
$loadAnchor = @'
    g_menuWindow = CreateMenuWindow(instance, g_gameWindow);
    if (!g_menuWindow)
        return 0;
    if (!InstallFrameStageBridge())
'@
$loadReplacement = @'
    g_menuWindow = CreateMenuWindow(instance, g_gameWindow);
    if (!g_menuWindow)
        return 0;
    LoadInventoryStore();
    if (!InstallFrameStageBridge())
'@
if (-not $source.Contains($loadAnchor)) {
    throw 'Inventory persistence load anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($loadAnchor, $loadReplacement)

# Persist dirty inventory state from the payload worker loop. This intentionally
# happens outside FrameStageNotifyHook so disk latency cannot stall rendering.
$sleepAnchor = @'
        Sleep(8);
    }
    RemoveFrameStageBridge();
'@
$sleepReplacement = @'
        FlushInventoryPersistenceIfNeeded();
        Sleep(8);
    }
    ShutdownInventoryChanger();
    RemoveFrameStageBridge();
'@
if (-not $source.Contains($sleepAnchor)) {
    throw 'Inventory persistence/shutdown anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($sleepAnchor, $sleepReplacement)

Set-Content -LiteralPath $OutputPath -Value $source -Encoding UTF8
