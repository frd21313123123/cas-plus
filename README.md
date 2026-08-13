# cas+

`cas+` — единый Windows x64-проект, содержащий GUI-инжектор и payload для локальной смены `env_sky` в Counter-Strike 2.

## Состав

- `injector/` — GUI-инжектор на C++20, ImGui, Direct3D 9 и BlackBone.
- `payload/` — DLL с отдельным окном выбора skybox preset.
- `build.ps1` — воспроизводимая сборка обоих компонентов и подготовка пакета.
- `.github/workflows/build-release.yml` — CI и публикация GitHub Releases.

Готовый пакет имеет структуру:

```text
cas-plus.exe
dlls/
  cas-plus-payload.dll
```

Инжектор автоматически обнаруживает DLL из каталога `dlls`.

## Локальная сборка

Требования:

- Windows x64;
- Visual Studio 2026 либо Build Tools с компонентом Desktop development with C++;
- Windows 10/11 SDK;
- PowerShell 5.1 или новее.

```powershell
.\build.ps1 -Configuration Release
```

Результат появится в `artifacts/package`.

## GitHub Actions и Releases

Workflow собирает x64-пакет при каждом push/PR и сохраняет его как Actions artifact.

Публикация Release выполняется автоматически при отправке тега `v*`:

```powershell
git tag v1.0.0
git push origin v1.0.0
```

Также workflow можно запустить вручную через Actions → Build and Release, указав `release_tag`.

## Использование

1. Полностью закройте CS2, если была загружена старая DLL.
2. Запустите локальную/offline или demo-сессию с параметром `-insecure`.
3. Запустите `cas-plus.exe`, выберите `cas-plus-payload.dll` и дождитесь загрузки карты.
4. Нажмите `Insert`, выберите skybox и нажмите `Apply`.

Успешное применение подтверждается сообщением `applied; both CS2 renderer caches confirmed`.

## Ограничения и безопасность

Проект предназначен только для локальной разработки, offline/demo и разрешённых тестовых сред. Он не содержит функций обхода античита и не является VAC-safe. Не используйте его на защищённых или соревновательных серверах.

Внутренний ABI CS2 может меняться. Payload проверяет сигнатуры и layout во время выполнения и прекращает операцию, если текущая версия игры несовместима.

## Лицензии

Проект сохраняет MIT-лицензию исходного Potato Injector. ImGui, BlackBone и остальные сторонние компоненты распространяются на условиях своих лицензий. Подробности — в [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

