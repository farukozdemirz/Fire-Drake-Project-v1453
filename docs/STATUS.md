# Bot Projesi — Güncel Durum

Son güncelleme: 2026-10-02 · Güncelleyen: Claude (F2-02 DOĞRULANDI)
Sunucu commit: `0f52027` (upstream ile aynı) · Bot kodu: henüz yok · Doküman paketi: v1.0
Veritabanı: `FDP_kn_online` (yerel; MAGIC.Etc düzeltmesi elle uygulanmış, `MAGIC_BAK_etc` yedeği var)

## Çalışma düzeni

- **Planlayıcı ve denetçi:** Claude (Claude Code), `/plan-olustur` ve `/plan-dogrula`.
- **Uygulayıcı:** DeepSeek v4.1 Flash, opencode üzerinden (`AGENTS.md`, `opencode.json`).
- Planlar `plans/` altında; akış `plans/README.md`'de.
- Derleme: `./tools/build.sh Release` ve `Debug`. 2026-10-01'de ikisi de WSL'den başarıyla derlendi (Debug tam derleme ~21 sn).

## Aktif faz

F1 — Veri ve mekanik doğrulama — Durum: GELIŞTIRILDI (DeepSeek'in işleri bitti; F0 KABUL_EDILDI 2026-10-02; F1-01..F1-10 KAPANDI, F1-08..F1-10 `gece/2026-10-02`'de; kalan: insan istemcisi testleri, faz raporu taslağı `docs/phase-reports/F1-taslak.md`, kabul bekliyor). Gece modunda F2'ye geçildi: F2-01 KAPANDI; F2-02 (tick altyapısı) DOĞRULANDI (birleştirmeyi döngü betiği yapar).

## Faz tablosu

| Faz | Durum | Son rapor | Kabul commit |
|---|---|---|---|
| 2026-10-02 | F1-07 | DOĞRULANDI (Tur 1) | 7/7 kriter ✔; 417 satırlık çıktı; iki M satırı bağımsız yeniden hesaplandı (952,55 / 979,85), docs/04 §4 incineration örneği ±0,1 ile yeniden üretildi |
| 2026-10-02 | F1-06 | DOĞRULANDI (Tur 1) | 7/7 kriter ✔; model iki R satırında bağımsız yeniden hesaplandı (aynı), `docs/04` §4 değerleri 13/14'te aynen yeniden üretildi (M-I HP 1581, yuvarlama); referans ekipmanda proc/elemental yok; botlar Hp=Mp=32000 |
| 2026-10-02 | F1-05 | DOĞRULANDI (Tur 1) | 7/7 kriter ✔; araç bağımsız çalıştırıldı (fail_count=0, 269 satır), ağırlık elle yeniden hesaplandı. Bulgu: envanter şablonundaki 100 × 1440 HP pot (ağırlık 100) 8/12 botu ağırlık sınırının üstüne çıkarıyor; MB-12 kod düzeyinde doğrulandı |
| 2026-10-02 | F1-04 | DOĞRULANDI (Tur 2) | 8/8 kriter ✔; gerçek DB'de bağımsız doğrulandı: 12 bot, bayt çözümü planla birebir, `LOAD_USER_DATA` satırı dönüyor, idempotent, `Upgrade` 0/7/8, rollback temiz (bot olmayan satırlar 6/4/4 değişmedi) |
| 2026-10-02 | F1-04 | DÜZELTME GEREKLİ (Tur 1) | 6/8 kriter ✔. Betik gerçek DB'de 4 SQL hatasıyla çalışmıyor (`REVERSE`/`REPLICATE` `varchar` döndürür, `tinyint` toplamı taşar, `CROSS APPLY` toplama hatası). Düzeltilmiş prototip DB'de doğrulandı: 12 bot, bayt çözümü planla birebir, `LOAD_USER_DATA` satırı döndürüyor, idempotent, rollback temiz. Düzeltme talimatı plan dosyasının sonunda |
| 2026-10-02 | F1-03 | DOĞRULANDI (Tur 1) | 6/6 kriter ✔; betikler geçici kopya tabloda çalıştırıldı: uygula → tekrar uygula (idempotent) → geri al, `MAGIC_BAK_etc` ile satır satır aynı; elle değiştirilmiş satıra dokunulmuyor. Gerçek `MAGIC` değişmedi |
| 2026-10-02 | F1-02 | DOĞRULANDI (Tur 2) | 8/8 kriter ✔; Tur 1 bulgusu (iptal sonrası bayat CASTING) kapandı, ek kenar vakaları bağımsız doğrulandı. `prepare`/`finish` çalışma zamanı doğrulaması ilk gerçek oturumda |
| F0 Ortam | KABUL_EDILDI | `docs/phase-reports/F0.md` (2026-10-02) | `22786e2` |
| F1 Veri ve mekanik doğrulama | GELIŞTIRILDI (kabul bekliyor, insan testleri açık) | `docs/phase-reports/F1-taslak.md` (taslak) | — |
| F2 Bot oturumu | GELIŞTIRILIYOR (gece modu; F1 kabulü beklenmeden başlandı) | — | — |
| F3 Telemetri ve test altyapısı | PLANLANDI | — | — |
| F4 Aksiyon ve adalet | PLANLANDI | — | — |
| F5 Navigasyon | PLANLANDI | — | — |
| F6 Sınıf davranışları ve solo | PLANLANDI | — | — |
| F7 Party koordinasyonu | PLANLANDI | — | — |
| F8 Değerlendirme ve 8v8 | PLANLANDI | — | — |
| F9 L1 öğrenme | PLANLANDI | — | — |
| F10 L2 bandit (opsiyonel) | PLANLANDI | — | — |

## Planlar

Liste: `plans/README.md`.

| Plan | Durum | Not |
|---|---|---|
| F0-01 Ortam doğrulama araçları | KAPANDI | `main` @ `43d3500` (merge) |
| F0-02 Sunucu çalıştırma betiği (`tools/run-servers.sh`) | KAPANDI | `bot/F0-02` @ `9c16d5e` (+ Tur 2 doğrulama commit'i); `main`'e birleştirme ve push proje sahibinin onayında; `plans/.aktif-plan` bu plana işaret ediyor |
| F1-01 Paket izleyici (`FDP_PACKET_TRACE`) | KAPANDI | `bot/F1-01` @ `9197379` (taban: `bot/F0-02`); birleştirme sırası: önce F0-02, sonra F1-01; çalışma zamanı kaydı F1-02'de (insan istemcisi) |
| F1-02 Zamanlama oturumu araçları | KAPANDI | `bot/F1-02` (taban: `main`); `plans/F1-02-zamanlama-oturumu-araclari.md`; sonra insan oturumu `docs/15` §4.2.1 |
| F1-03 `MAGIC.Etc` SQL betiği | KAPANDI | `bot/F1-03` (taban: `main`); `plans/F1-03-magic-etc-sql-betigi.md`; DeepSeek yalnızca betik yazar, çalıştırma/doğrulama Claude'da |
| F1-04 Bot karakter kurulum betiği | KAPANDI | `bot/F1-04` (taban: `main`); `plans/F1-04-bot-karakter-kurulum-betigi.md`; DeepSeek yalnızca betik yazar, çalıştırma/doğrulama Claude'da |
| F1-05 Bot ekipman/ağırlık raporu | KAPANDI | `bot/F1-05` (taban: `main`); `plans/F1-05-bot-ekipman-agirlik-raporu.md`; DeepSeek botları DB'ye uygular ve raporu çalıştırır |
| F1-06 Hasar modeli ve başlangıç HP | KAPANDI | `bot/F1-06` (taban: `main`); `plans/F1-06-hasar-modeli-ve-baslangic-hp.md`; DeepSeek `tools/stat-model.py` yazar, `db/002` Hp/Mp = 32000 yeniden uygular |
| F1-07 Büyü ve heal modeli | KAPANDI | `bot/F1-07` (taban: `main`); `plans/F1-07-buyu-ve-heal-modeli.md`; DeepSeek `tools/spell-model.py` yazar, DB'ye yazılmaz |
| F1-08 Arena A veri doğrulaması | KAPANDI | `bot/F1-08` (taban: `main`); `plans/F1-08-arena-a-veri-dogrulamasi.md`; Tur 2'de doğrulandı, `gece/2026-10-02`'ye birleşti (2026-10-02, gece modu); `main`'e birleştirme proje sahibinde |
| F1-09 Sunucu tarafı hasar kaydı (`FDP_DAMAGE_TRACE`) | KAPANDI | `bot/F1-09` @ `3406555` (taban: `gece/2026-10-02`); `plans/F1-09-sunucu-hasar-kaydi.md`; Tur 1'de doğrulandı, `gece/2026-10-02`'ye birleşti (2026-10-02, gece modu); kanca yalnızca `--damage-trace` ile derlenir; log özet betiği F1-10'da; çalışma zamanı ölçümü insan oturumunda (T-MECH-DMG) |
| F1-10 Hasar logu özet betiği (`tools/damage-trace-summary.py`) | KAPANDI | `bot/F1-10` @ `b93309d` (taban: `gece/2026-10-02`); `plans/F1-10-hasar-logu-ozet-betigi.md` (Tur 2'de doğrulandı; `gece/2026-10-02`'ye birleşti, 2026-10-02, gece modu, merge `9148336`); DeepSeek yalnızca betik yazar (log + `stat-model`/`spell-model` çıktısını ± %15 karşılaştırır); F1'in DeepSeek'e düşen son işi |
| F2-01 Bot alıcısı (`m_botSink`) ve ayrılmış slot havuzu | KAPANDI | `bot/F2-01` (taban: `gece/2026-10-02`); `plans/F2-01-bot-alicisi-ve-slot-havuzu.md`; S1+S2: `KOSocketMgr` rezerve havuz, `CUser::Send` geçersiz kılma, `BotManager::Startup` + öz-sınama; `[BOT] ENABLED=0` varsayılan (davranış değişmez); çalışma zamanı doğrulaması yapıldı (`ENABLED=1`: `reserved 16 sessions (ids 2984-2999), pool self-test OK`; `ENABLED=0`: log yok); `bot/F2-01` @ `7703426` `gece/2026-10-02`'ye birleşti (2026-10-02, gece modu, merge `e7f8119`); `main`'e birleştirme proje sahibinde |
| F2-02 `BOT_TICK` IOCP olayı ve bot zamanlayıcı thread'i | DOĞRULANDI | `bot/F2-02` (taban: `gece/2026-10-02`); `plans/F2-02-bot-tick-iocp-olayi.md`; ADR-0005 (otonom döngüde Claude kararı): `SOCKET_IO_EVENT_BOT_TICK`, `SocketMgr::PostBotTick` (tek uçuşta), `BotManager` zamanlayıcı thread'i ve boş `Tick()` + öz-sınama logu; `ENABLED=0` iken thread/kanca yok; çalışma zamanı doğrulaması yapıldı (`ENABLED=1`: tick IOCP thread'inde, `TICK_MS=100` → ortalama 110,5 ms, `TICK_MS=20` → 31,7 ms, skipped 0; `ENABLED=0`: log yok, ini'ye `TICK_MS` yazılmaz); `bot/F2-02` @ `7c94ace`, `gece/2026-10-02`'ye birleştirme döngü betiğinde |

## Son doğrulamalar

| Tarih | Plan | Karar | Not |
|---|---|---|---|
| 2026-10-02 | F2-02 | DOĞRULANDI (Tur 1) | 11/11 kriter ✔ (tam yeniden derlemede Release/Debug rc=0, `Bot\`/`SocketMgr`/`SocketDefines` uyarısı 0; kalan uyarılar eski dosya/satırlarda). Çalışma zamanı bağımsız doğrulandı: `ENABLED=1, MAX_BOTS=16` → `tick OK on IOCP thread 41460 (timer thread 30248), period 100 ms`, `100 tick intervals in 11052 ms (avg 110.5 ms), skipped 0`; `TICK_MS=20` → avg 31,7 ms; `ENABLED=0` → log yok, ini'ye `TICK_MS` eklenmedi; 3/3 UP, nazik kapanış, çökme olayı yok; ini geri yüklendi (md5 aynı). Notlar: `Sleep` granülaritesi gerçek aralığı uzatıyor (MET-PERF-02/F3 için `dt` ölçümü); F2-01'den gelen `m_activeSessions` bulgusu F2-03/F2-04'te; uyarı listesi raporda eksik (artımlı derleme) |
| 2026-10-02 | F2-01 | DOĞRULANDI (Tur 1) | 10/10 kriter ✔ (Release/Debug rc=0, yeni uyarı yok; 5 Release uyarısı eski satırlarda). Çalışma zamanı bağımsız doğrulandı: `[BOT] ENABLED=1, MAX_BOTS=16` → `Bot_*.log`: `BotManager: reserved 16 sessions (ids 2984-2999), pool self-test OK`, sunucu 3/3 UP; `MAX_BOTS=500` → 100 (ids 2900-2999) kıskaç; anahtar yokken log yok (ini'ye `ENABLED=0` yazılır, beklenen). Ini geri yüklendi. Notlar: F2-02/03 `m_activeSessions`'taki bot oturumunu gezen zamanlayıcıları (`Update()`, `SendAll*`) ele almalı; uygulayıcı raporundaki "Release'de uyarı yok" ifadesi yanlış (K1 ölçütü yine karşılandı) |
| 2026-10-02 | F1-10 | DOĞRULANDI (Tur 2) | 8/8 kriter ✔ (selftest `_E` için `R`/`K`/`H` hükmü sınıyor; `ctx=R` profil geri düşüşü bağımsız denendi: `verdict=OK match=profile`; `requested=0` D satırı yok, `zero=` A+D; Release rc=0 uyarı 0). Tur 1 bulgularının tamamı giderildi |
| 2026-10-02 | F1-10 | DÜZELTME GEREKLİ (Tur 1) | 8/8 kriter lafzen ✔ (selftest, bölümler, dayanıklılık, stdlib/ASCII/LF, kapsam, Release derleme rc=0 uyarı 0). Engelleyen: `find_r` profil geri düşüşünde model türünü `None` döndürüyor, `_E` botlarının `R` (temel vuruş) grupları `NO_MODEL` çıkıyor (`K`/`H` doğru), selftest hükmü sınamıyor. Düşük: `primary=0`, `requested=0` satırı `kind=heal` etiketleniyor |
| 2026-10-02 | F1-09 | DOĞRULANDI (Tur 1) | 11/11 kriter ✔ (Release, `--damage-trace`, `--packet-trace --damage-trace`, Debug bağımsız derlendi; bayraksız exe'de `DamageTrace_` dizgesi 0, bayraklı 1; 16 uyarının tamamı eski satırlardan). Bayraksız fark: 3 dosyada +5/−0 (include + `#ifdef` bloğu). Bilgi: bağlam dışı satırlar (`ctx=-`, `primary=0`: DoT tikleri, yansıtılan hasar) F1-10 özetinde ayrı ele alınmalı |
| 2026-10-02 | F1-08 | DOĞRULANDI (Tur 2) | 8/8 kriter ✔. Tur 1 ve Tur 2 araç sürümleri yan yana çalıştırıldı: CHECK dışı çıktı birebir aynı. CHECK spawn payı artık `docs/15` tanımıyla: A 143,8 (doc 144), B 159,8 (doc 160), kule A 132,8 / B 145,5; fark ≤ 0,5 m. K5 atıfları depoda tek tek açıldı. Eksen önerisi: A için `angle=15` (dh 0,83; 70,6 m arena içi yol), B yedeği `angle=135`. Zone 71'e otomatik taşıma yolları: Bifrost bitişi (zone 31→71) ve Chaos Dungeon çıkışı seviye ≥ 70 (`GameServerDlg.cpp:2677`); MEC-ZON-03 doğrulandı |
| 2026-10-02 | F1-08 | DÜZELTME GEREKLİ (Tur 1) | 6/8 kriter ✔ (K4, K5 ✘). Araç bağımsız çalıştırıldı (67 satır, rapordakiyle aynı), PATH bağımsız Dijkstra ile aynı (Karus→A 259,8 m; El Morad (635,925)→A 678,1 m). Veri bulgusu: zone 71 `START_POSITION` `bRange=0`, respawn tam (1380,1090) / (630,920) (notlar dosyası düzeltildi). Engelleyen: CHECK spawn payı `docs/15`'teki tanımla (monster+soldier_npc+monument+gate) karşılaştırılmıyor, rapordaki "8 m ızgara" açıklaması yanlış (gerçek: Karus Commander 143,8); K5'te yanlış `dosya:satır` atıfları ve eksik `GameServerDlg.cpp:2677`. MEC-ZON-03 doğrulandı |
| 2026-10-02 | F1-02 | DÜZELTME GEREKLİ (Tur 1) | 7/8 kriter ✔. Engelleyen: CAST süresi eşleştirmesi iptal sonrası bayat CASTING kullanıyor (3300 ms ölçülür, doğrusu 300); ayrıca planda eksik olan CASTING→iptal süresi eklenecek. Kod Claude'un planındaki lafızdan kaynaklı; düzeltme talimatı plan dosyasında |
| 2026-10-02 | F1-01 | DOĞRULANDI (Tur 1) | 11/11 kriter ✔ (Release, Release `--packet-trace`, Debug bağımsız derlendi; bayraklı exe'de log dizgesi var, bayraksızda yok). Sapma: `WIZ_PARTY` kişisel ad okuduğu için izleme dışı (doğru). Yeni: KI-009 (düşük) |
| 2026-10-01 | F0-02 | DOĞRULANDI (Tur 2) | 14/14 kriter ✔; Tur 1'in 6 bulgusu kapandı (zaman aşımı yolu geçici kopyayla çalışma zamanında doğrulandı). Kalan: KI-008 (düşük, `stop` satırı hep `0 sn`) |
| 2026-10-01 | F0-02 | DÜZELTME GEREKLİ (Tur 1) | 14/14 kriter ✔ (§5.4 Claude tarafından bağımsız yeniden çalıştırıldı, K11 sahte istemciyle çalışma zamanında doğrulandı). Engelleyen: süre/zaman aşımı duvar saati değil (300 sn ≈ 480 sn), zaman aşımı mesajı son durumu kaybediyor; ayrıca `[DOWN]` satırı yok, `stop` etiketleri plandan farklı. Yeni: KI-007 |
| 2026-10-01 | F0-01 | DOĞRULANDI | 10/10 kriter; 2 düşük + 3 bilgi bulgusu (`plans/F0-01…` Doğrulama Raporu). Debug/Release tablosu → `docs/02` §2.1 |

## Verilen kararlar

K-1..K-10, 2026-10-01 (`docs/18` §1, `docs/adr/`). Önerilenden farklı seçilenler:

- K-5: tüketilmeyen potlar olduğu gibi kalır.
- K-6: yalnızca arena A.
- K-9: botlar sıralama/ödül/duyurulara tamamen dahil.
- K-10: PR #10 alınmaz.

- ADR-0005 (bot tick'i IOCP thread'inde, `BOT_TICK` olayı), 2026-10-02, gece modu: **otonom döngüde Claude kararı — gözden geçirilmeli** (`docs/adr/ADR-0005-bot-tick-thread-modeli.md`; öneri `docs/13` §3.1 aynen).

Açık teknik kararlar: ADR-0006..0008, ilgili fazda verilecek.

## Blokajlar

| Konu | Etki | Sahibi | Not |
|---|---|---|---|
| (açık blokaj yok) | | | T-ENV-01 kalanı kapandı, 2026-10-02 |

## Proje sahibi testleri (bekleyen)

İstemci (GUI) gerektiren, otonom döngünün yapamadığı testler. Gece döngüsü yeni maddeler ekler; sabah topluca yapılır.

| Test | Ne yapılacak | Hesap / komut | Beklenen |
|---|---|---|---|
| T-DATA-01 (M-I) | `BotAccMIK` ile gir, karakter penceresini oku | `BotAccMIK` (şifre ilk girişte belirlenir) | HP 2228, MP 6021, AC 612, saldırı 57, can/mana dolu |
| T-MECH-DMG-01..03 | İki istemciyle (Karus bot vs El Morad bot) birbirine R, Type1 skill ve büyü; sunucu hasar kaydı (F1-09 doğrulandı; ölçüm için `./tools/build.sh Release --damage-trace` ile derlenen GameServer gerekir, çıktı `Logs/DamageTrace_*.log`, 20 sütun: plan F1-09 §5.4) ile modelin karşılaştırılması; analiz: `python3 tools/damage-trace-summary.py Logs/DamageTrace_*.log --stat-model <stat.txt> --spell-model <spell.txt>` (F1-10; model çıktıları `tools/stat-model.py > stat.txt`, `tools/spell-model.py > spell.txt`) | ör. `BotAccWPK` vs `BotAccWGE`; `BotAccMFK` vs `BotAccWPE` | `tools/stat-model.py` / `tools/spell-model.py` ± %15 |
| T-ENV-ARENA-01 | Arena A'da (1274, 890) 30 dk bekleyip canavar/NPC geçiyor mu gözle | herhangi bir bot hesabı | 120 m içinde varlık 0 |
| T-ENV-ARENA-03 | El Morad karakteriyle Karus kapısı önüne git, tower saldırı mesafesini gözle | `BotAccWPE` | tower 20–30 m'de saldırır |
| T-ENV-ARENA-02, T-ENV-ARENA-04 | Arena koordinatlarının oyunda doğrulanması (yürüme, yükseklik, engeller); iki ulusun respawn noktasından arenaya yürüme süresi | herhangi bir bot hesabı (GM ile) | `docs/15` §2.4 / T-ENV-ARENA-02, -04: koordinat/süre kaydı; F1-08 hesabı: Karus→A 259,8 m, El Morad→A 678,1 m |
| T-MECH-SKILL-W/P/M, T-MECH-BUF-01..08, T-POT-01..03 / T-MECH-POT-03..05 | Çekirdek skill başına MP/recast/menzil/etki/fail sebebi; buff çakışmaları ve debuff'ın buff'ı silmesi; pot hareketi durdurur mu (Q-06) | bot hesapları (profil başına) | `docs/05` tablosuyla uyum; `docs/11` §8 |
| T-DATA-01 (M-I), T-DATA-02, T-DATA-03 | M-I girişinin teyidi; referans ekipmanın istemcide kuşanılabilirliği; maks HP/MP/saldırı oyunda | `BotAccMIK` ve diğer 11 bot hesabı | `docs/04` §7 |
| T-ARCH-01 (F2-01, AC-ARCH-04) | `[BOT] ENABLED=1` (`MAX_BOTS=16`) ile sunucuyu aç, gerçek istemciyle giriş yap; karakter normal giriyor/oynuyor mu, sunucu kapanıp açılınca tekrar giriş sorunsuz mu; sonra `ENABLED=0` ile aynı | herhangi bir bot hesabı | giriş sorunsuz; gerçek bağlantı 2984-2999 kimliklerini almaz (kapasite 3000 → 2984); `ENABLED=0` iken eskisi gibi |
| `war-move` (Q-02), ikinci priest/mage oturumu | Temiz koşu/yürüyüş hız ölçümü; `pri-cast`/`mag-cast` tekrarı | `tools/trace-session.sh` (`docs/15` §4.2.1) | `docs/03` §13.2'deki `[A]`'ların kalkması |

## Sıradaki adımlar

1. **Gece modu (2026-10-02):** F1'in DeepSeek işleri bitti; F1 faz raporu taslağı `docs/phase-reports/F1-taslak.md` yazıldı (çıkış kararı kısmi: insan testleri açık, aşağıdaki bekleyen testler). F2 başladı: F2-01 (KAPANDI), F2-02 (HAZIR, tick altyapısı) → sıradaki F2 planları: F2-03 bot girişi/spawn (S3: hesap/karakter ataması, `WIZ_SEL_CHAR` → `GameStart(1/2)` taklidi, tick durum makinesi, `[BOT] SPAWN_ON_START`), F2-04 bot çıkışı/despawn (S4, slotu DB kaydı bittikten sonra iade) + `Update()` ve zaman aşımı muafiyeti (S5, S8), F2-05 sabit IP (S7) ve 1000 spawn/despawn dayanıklılığı. `gece/2026-10-02`'nin `main`'e birleştirilmesi ve push proje sahibinde (`git switch main && git merge --no-ff gece/2026-10-02 && git push origin main`, sabah). Claude'un bekleyen doküman işi: `docs/15` §2.4 eksen güncellemesi (A için `angle=15`), §2.3 pay tanımı notu (spawn payı = monster + soldier_npc + monument + gate), `docs/03` MEC-ZON-03 doğrulandı notu ve zone 71 otomatik taşıma yolları; `docs/13` §4.1'e F2-01/F2-02 sonrası ini anahtarları (`[BOT] ENABLED`, `MAX_BOTS`, `TICK_MS`).
2. Push: `main` her plan DOĞRULANDI olduğunda otomatik birleştirilir ve push'lanır (kalıcı izin, 2026-10-02).
3. Proje sahibi, ikinci insan oturumu (priest/mage hazır olunca): `pri-cast`, `mag-cast` ve iptal senaryoları (CLI-03, Q-01), `war-combo` (CLI-02), `war-move` (Q-02), `tools/trace-session.sh prepare` … `finish` (`docs/15` §4.2.1).
4. Proje sahibi, arena doğrulaması (T-ENV-ARENA-01..04, Q-11): arena A'da canavar/tower gözlemi; protokolü Claude yazar.

## Otonom döngü

2026-10-01 19:00'da başlatıldı (`plans/_logs/auto-loop.log`); ilk iterasyonda F0-02 planı yazıldı. Tasarım: `plans/OTONOM_DONGU.md`, `tools/auto-loop.sh`. `--run` ön kontrolü: altyapı dosyaları commit'li + çalışma ağacı temiz. Varsayılanlar: `MAX_ITERATIONS=5`, faz sınırında dur, `claude` modeli `opus`, `opencode` modeli `opencode-go/deepseek-v4.1-flash`.

## Devralan için notlar

- Araştırma sırasında sunucu çalıştırılmadı. Tüm mekanik iddialar kod/veri okumasına dayanır; çalışma zamanı doğrulaması F1'in işi.
- 2026-10-01: üç sunucu ilk olarak proje sahibi tarafından 13:09–13:10'da `C:\dev\fdp\server`'dan elle açılmıştı. F0-02 testlerinden beri `tools/run-servers.sh` ile açılıp kapatılıyorlar. Claude'un F0-02 Tur 1 doğrulaması sonunda yine `C:\dev\fdp\server` exe'leriyle 3/3 `UP` bırakıldılar (`FDP_SERVER_BIN_DIR=/mnt/c/dev/fdp/server tools/run-servers.sh start`); bağlı istemci yok `[V]`. Elle açılan sunucular `stop` ile nazikçe kapanmıyor (KI-007). Aynı makinede yolu okunamayan ilgisiz bir `GameServer.exe` (pid 4336) var; betik süreçleri exe yoluyla tanıyor ve ona dokunmuyor `[V]`.
- `tools/build.sh` ve `tools/auto-loop.sh` git'te `100644` modunda (çalıştırılabilir değil). WSL/drvfs'te sorun çıkarmıyor; temiz bir klonda `bash tools/build.sh` gerekir. Küçük bir düzeltme planına veya proje sahibinin `git update-index --chmod=+x` commit'ine bırakıldı.
- Depo dosyaları CRLF + tab. Bazı dosyalar ISO-8859 (Korece yorumlu) veya UTF-8 BOM'lu; kodlama korunmalı (`AGENTS.md` §3).
- `.sh` dosyaları `.gitattributes` ile LF'e sabitlendi (`core.autocrlf=true` betikleri bozmasın diye).
