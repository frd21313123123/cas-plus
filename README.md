# cas+

`cas+` — единый Windows x64-проект, содержащий GUI-инжектор и payload с локальными визуальными инструментами для Counter-Strike 2.

## Состав

- `injector/` — GUI-инжектор на C++20, ImGui, Direct3D 9 и BlackBone.
- `payload/` — DLL с отдельным окном выбора skybox preset и обратимой подсветкой ботов своей команды.
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
4. Нажмите `Insert`.
5. Для подсветки включите `Highlight teammate bots (glow + green tint)`.
6. Для смены неба выберите skybox и нажмите `Apply`.

Успешное применение подтверждается строкой `Sky: applied` и счётчиками
`entities / writes / renderer`.

Подсветка выбирает только живых teammate-ботов по проверенной комбинации
`FL_FAKECLIENT`, pawn-флага `FL_BOT` либо нулевого Steam ID с допустимой
сложностью бота. Полный pawn handle проверяется вместе с serial; люди и враги
исключаются. Исходные glow/render/tint-поля сохраняются.
При выключении, смерти бота или смене команды исходное состояние восстанавливается.
Payload вызывает штатные setter-функции текущего renderer: client tint обновляет
scene-object color, а glow type 3 регистрируется в glow manager. Окно показывает
отдельные статусы Sky и Bots и диагностические счётчики.

## Ограничения и безопасность

Проект предназначен только для локальной разработки, offline/demo и разрешённых тестовых сред. Он не содержит функций обхода античита и не является VAC-safe. Не используйте его на защищённых или соревновательных серверах.

Внутренний ABI CS2 может меняться. Payload разрешает поля подсветки по именам через
`SchemaSystem_001`, проверяет сигнатуры и layout во время выполнения и прекращает
операцию, если текущая версия игры несовместима.

Версия 4.4.1 исправляет обновление CS2 от 13 августа 2026 года: размер записи
`CEntityIdentity` изменился с `0x78` на `0x70`. Stride теперь извлекается из
единственного совпадения полного handle-resolver текущего `client.dll`, без
вызова SchemaSystem во время inject. Bot schema runtime разрешается лениво на
game thread после включения подсветки.

## Лицензии

Проект сохраняет MIT-лицензию исходного Potato Injector. ImGui, BlackBone и остальные сторонние компоненты распространяются на условиях своих лицензий. Подробности — в [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
