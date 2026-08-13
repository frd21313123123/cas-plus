cas+ payload 4.2.0
===================

Назначение
----------
x64 payload с отдельным Win32-окном для локальной смены материала env_sky
в CS2. Серверные команды не отправляются. DirectX-hook, aim/ESP, автоматизация
ввода и обход античита отсутствуют.

Что исправлено в 4.2.0
----------------------
- Вся работа с ResourceSystem, MaterialSystem, сущностями и renderer теперь
  выполняется только из Source2Client002::FrameStageNotify (stage 12), а не
  из потока интерфейса Windows.
- Перед использованием .vmat выполняется blocking precache через
  ResourceSystem013, затем материал разрешается через VMaterialSystem2_001.
- Оба поля C_EnvSky (обычное и lighting-only) меняются через проверенный
  ref-counted CStrongHandle assignment helper, с rollback при частичной ошибке.
- После ForceUpdateSkybox проверяются оба поля сущности и оба renderer-cache.
- Внутренние cache pointers больше не обнуляются. Именно этот старый путь мог
  давать зависание, утечки и ложный статус успешного применения.
- Entity, field, cache, assignment и frame-stage адреса находятся динамически.
  Hook ставится только при единственном совпадении сигнатуры с vtable[36].
  Несовместимое обновление CS2 приводит к безопасному отказу без записи.

Использование
-------------
1. Полностью закройте CS2, если в неё уже была загружена старая DLL.
2. Запустите локальную/offline или demo-сессию с параметром -insecure.
3. Загрузите только cas-plus-payload.dll в cs2.exe через cas+.
4. Зайдите на карту и дождитесь завершения загрузки.
5. Нажмите INSERT, выберите preset и нажмите Apply.
6. Успех подтверждается текстом:
   "applied; both CS2 renderer caches confirmed".
7. После смены карты нажмите Apply повторно. Restore возвращает сохранённые
   материалы текущей карты.

Встроенные presets
------------------
Cloudy, Anubis, Dust 2, Mirage, Nuke, Overpass, Train, Vertigo, Aztec, Italy.
Имена всех десяти ресурсов проверены в установленном pak01_dir.vpk от
2026-08-13; устаревших или отсутствующих путей в этом списке нет.

Совместимость и пределы гарантии
-------------------------------
Реализация проверена статически против установленного client.dll от
2026-08-13. Найденные отношения соответствуют текущим полям C_EnvSky:
m_hSkyMaterial, m_hSkyMaterialLightingOnly и двум renderer-cache.

Абсолютной гарантии для injected DLL после любого будущего обновления CS2 не
существует: это приватный ABI движка. Версия 4.2.0 специально работает
fail-closed и не продолжает запись, если проверка ABI не прошла.

Valve-native способ с наиболее сильной гарантией — Workshop/addon-карта, где
материалы упакованы вместе с картой, а заранее созданные env_sky переключаются
через Enable/Disable. Для stock-карт без контроля сервера этот payload использует
наиболее надёжный из подтверждённых на текущей сборке client-side путей.

Безопасность
------------
Injected DLL не является VAC-safe. Используйте только offline/demo с -insecure.
Горячая выгрузка не поддерживается; оставляйте DLL загруженной до закрытия CS2.

Техническая база
----------------
https://github.com/advancedfx/advancedfx/blob/main/AfxHookSource2/SceneSystem.cpp
https://github.com/advancedfx/advancedfx/blob/main/AfxHookSource2/main.cpp
https://github.com/a2x/cs2-dumper
https://github.com/srwiruwiru/SkyboxChanger-CS2
