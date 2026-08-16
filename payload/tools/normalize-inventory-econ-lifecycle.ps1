param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath
)

$ErrorActionPreference = 'Stop'

$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8

$anchor = @'
        void* typeCache = InventoryEconTypeCache(inventory);
        void* addObject = InventoryEconVtableFunction(typeCache, 1);
        if (!typeCache || !addObject)
            return false;
        using AddFn = bool (*)(void*, void*);
        if (!reinterpret_cast<AddFn>(addObject)(typeCache, object))
            return false;
        if (!InventoryEconNotify(inventory, 0, owner, object))
'@

$replacement = @'
        void* typeCache = InventoryEconTypeCache(inventory);
        void* addObject = InventoryEconVtableFunction(typeCache, 1);
        if (!typeCache || !addObject)
        {
            void* destructor = InventoryEconVtableFunction(object, 1);
            if (destructor)
                reinterpret_cast<void (*)(void*, bool)>(destructor)(object, true);
            return false;
        }
        using AddFn = bool (*)(void*, void*);
        if (!reinterpret_cast<AddFn>(addObject)(typeCache, object))
        {
            void* destructor = InventoryEconVtableFunction(object, 1);
            if (destructor)
                reinterpret_cast<void (*)(void*, bool)>(destructor)(object, true);
            return false;
        }
        if (!InventoryEconNotify(inventory, 0, owner, object))
'@

$count = ([regex]::Matches($source, [regex]::Escape($anchor))).Count
if ($count -ne 1) {
    throw "Expected exactly one CEconItem AddObject rollback anchor, found $count. Refusing to patch blindly."
}

$source = $source.Replace($anchor, $replacement)
Set-Content -LiteralPath $InputPath -Value $source -Encoding UTF8
Write-Host "Normalized CEconItem creation rollback lifecycle: $InputPath"
