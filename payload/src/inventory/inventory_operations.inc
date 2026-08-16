// cas+ local-only virtual inventory operations.
// This module mutates InventoryStore only. It never touches Steam/GC state and
// never performs game memory writes; UpdateInventoryChanger projects the final
// selected state on the game thread.

static int InventoryClampStatTrak(int value)
{
    if (value < 0)
        return -1;
    // Keep the counter comfortably inside the historical fallback field range
    // and avoid integer overflow from repeated UI clicks.
    if (value > 999999)
        return 999999;
    return value;
}

static void InventoryUiAdjustStatTrak(int delta)
{
    if (!InventoryTryLock())
        return;
    VirtualInventoryItem* item = InventorySelectedLocked();
    if (item)
    {
        if (item->statTrak < 0)
            item->statTrak = delta > 0 ? InventoryClampStatTrak(delta) : 0;
        else
            item->statTrak = InventoryClampStatTrak(item->statTrak + delta);
        MarkInventoryDirty();
    }
    InventoryUnlock();
}

static void InventoryUiDisableStatTrak()
{
    if (!InventoryTryLock())
        return;
    VirtualInventoryItem* item = InventorySelectedLocked();
    if (item)
    {
        item->statTrak = -1;
        MarkInventoryDirty();
    }
    InventoryUnlock();
}

static void InventoryUiResetSelectedCosmetics()
{
    if (!InventoryTryLock())
        return;
    VirtualInventoryItem* item = InventorySelectedLocked();
    if (item)
    {
        item->paintKit = 0;
        item->seed = 0;
        item->wear = 0.0001f;
        item->statTrak = -1;
        item->quality = 0;
        item->customNameLength = 0;
        item->customName[0] = 0;
        MarkInventoryDirty();
    }
    InventoryUnlock();
}

static void InventoryUiCycleQuality(int direction)
{
    if (!InventoryTryLock())
        return;
    VirtualInventoryItem* item = InventorySelectedLocked();
    if (item && direction != 0)
    {
        // Values come from the current items_game qualities table. Restricting
        // the UI to a short allow-list avoids accidentally using sentinel or
        // unsupported values while preserving the useful item qualities.
        static const BYTE qualities[] = { 0, 3, 4, 9, 12 };
        int current = 0;
        for (int i = 0; i < static_cast<int>(sizeof(qualities)); ++i)
            if (qualities[i] == item->quality)
            {
                current = i;
                break;
            }
        current += direction > 0 ? 1 : -1;
        if (current < 0)
            current = static_cast<int>(sizeof(qualities)) - 1;
        if (current >= static_cast<int>(sizeof(qualities)))
            current = 0;
        item->quality = qualities[current];
        MarkInventoryDirty();
    }
    InventoryUnlock();
}

static const wchar_t* InventoryQualityName(BYTE quality)
{
    switch (quality)
    {
    case 3: return L"Unusual";
    case 4: return L"Unique";
    case 9: return L"StatTrak";
    case 12: return L"Souvenir";
    default: return L"Default";
    }
}
