# Bilinen Sorunlar

> Kurallar ve önem seviyeleri: `docs/21` §4.4. Önem: K = Kritik, Y = Yüksek, O = Orta, D = Düşük. Mekanik hatalar (MB-*) ayrıca `docs/03` §15'te.
> Son güncelleme: 2026-10-01 · Claude

| Kimlik | Başlık | Önem (K/Y/O/D) | Bileşen | İlk görüldüğü commit | Tekrar üretme | Geçici çözüm | Durum | İlgili test / commit |
|---|---|---|---|---|---|---|---|---|
| KI-001 | MAGIC.Etc=1 satırları quest 1 istiyor; yeni karakterde skill/pot çalışmıyor | Y | Veri | 0f52027 | start.md §4 | Etc=0 güncellemesi (yerelde uygulanmış) | AÇIK — kalıcı SQL betiği F1'de (ADR-0003) | T-DATA-06 |
| KI-002 | WIZ_WARP yalnızca harita sınırı kontrolüyle ışınlıyor (yalnızca GM) | D | Hareket | 0f52027 | GM warp | — | AÇIK | MEC-MOV-06 |
| KI-003 | Zones.tbl ve sunucu SMD adları zone 71/72 için ters | D | Veri | 0f52027 | start.md §9 | Sunucu SMD'si esas | AÇIK | Q-20 |
| KI-004 | Konsol çıktısı dosyaya yönlendirilince boş | D | Loglama | 0f52027 | start.md §9 | Bot telemetrisi ayrı yazıcı | AÇIK | — |
| KI-005 | Moradon plaza merdiveninde sıkışma | D | Harita/istemci | 0f52027 | start.md §9, issue #7 | — | AÇIK | — |
| KI-006 | Pot NPC sürümleri tüketilmiyor (MB-01) | O | Veri | 0f52027 | 03 §6.2 | Kural insan ve bot için aynı (K-5) | KABUL EDİLDİ (ADR-0009) | T-POT-01 |
| KI-007 | Elle açılmış sunucular `taskkill /PID` ile nazikçe kapanmıyor; `run-servers.sh stop` zaman aşımından sonra zorla (`/F /T`) kapatıyor `[V]` | D | Araçlar (`tools/run-servers.sh`) | 3314544 | Sunucuları betik dışında aç → `tools/run-servers.sh stop` → `force: timeout` (F0-02 Uygulayıcı Raporu adım 2) | Sunucuları `tools/run-servers.sh start` ile açmak; bu yolla nazik kapanış doğrulandı | AÇIK. Neden `[Ö]`: konsol penceresi süreç sahibi değil (ör. varsayılan terminal). Olası çözüm: AttachConsole + GenerateConsoleCtrlEvent(CTRL_BREAK); handler tüm ctrl türlerini kabul ediyor (`GameServer/main.cpp:62-69`) | F0-02 |
| KI-008 | `tools/run-servers.sh stop` nazik kapanış satırı her zaman `(nazik, 0 sn)` yazıyor; gerçek süre ~2 sn/sunucu (`elapsed` yazımdan sonra hesaplanıyor, `tools/run-servers.sh:274` ⇐ `:277`) `[V]` | D | Araçlar (`tools/run-servers.sh`) | 9ae6a87 | `tools/run-servers.sh stop` çıktısı + dış zaman damgası | Yok gerek: yalnızca ekrandaki sayı, eşik ve çıkış kodu doğru | AÇIK. Düzeltme: `elapsed=$((SECONDS - t0))` satırını `if ! proc_alive` bloğundan önce al | F0-02 Doğrulama Tur 2 |
