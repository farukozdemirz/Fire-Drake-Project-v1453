# İnsan testleri: öncelik sırası ve test kartları (2026-10-03)

Okuyan: proje sahibi. Yazan: Claude planlayıcı yardımcısı (salt-okunur koşu).
Kaynak anı: `gece/2026-10-02` (okuma sırasında uç `e1fe7f1`/`f4daa27`'ya ilerledi: döngü çalışıyor `[V: git log]`), `main` @ `56fc6e4`. Satır numaraları `git show gece/2026-10-02:<yol>` görünümüne aittir; döngü dosyaları değiştirdikçe kayabilir.

**Bu koşunun sınırı (dürüstlük notu):** repo salt-okunurdu; sunucu, build, test, DB ve istemci çalıştırılmadı. Aşağıdaki "Claude'un yapacağı doğrulamalar" bu yüzden **tamamlanmış değil, hazır tariflerdir** (§3). Hiçbir faz `KABUL_EDILDI` yapılmadı; kabul kararı §6'da sana bırakıldı. Kanıt etiketleri: `[V]` repodan okundu, `[Ö]` benim çıkarımım/öneri, `[A]` varsayım, "doğrulanamadı" = bulunamadı.

---

## 1. Sırayla ne yapmalısın

Sıralama ölçütü: "mevcut ilerlemeyi gerçekten etkileyen" = bir sonraki işi ya da faz kapısını **fiilen** durduran test başta; yalnızca faz kabul kâğıdını dolduran test sonda. Gerçekten bloke eden tek kalem `T-NAV-02` (aşağıda 1); geri kalanların çoğu kabul/kanıt kalemidir ve otonom döngüyü durdurmaz (STATUS: "zorunlu yeni insan testi eklenmedi", `docs/STATUS.md:388`).

| Sıra | Ne | Kim | Neyi bloke ediyor | Neden bu sırada |
|---|---|---|---|---|
| 0 | **Ön adım:** otonom döngüyü durdur, bot kodlu sunucuyu derle, ini ayarla (§2) | sen | tüm sunucu testleri | Döngü her adımda sunucuları kapatır ve aynı build klasörünü/çalışma ağacını kullanır; çalışırken test yapılamaz (§5) |
| 1 | **Claude'a iste: `T-NAV-02` (eğim) ve `T-NAV-09` (su) protokolünü yazsın**, sonra **harita yürüyüş oturumu**: T-NAV-02 + T-NAV-09 + T-ENV-ARENA-02 (kart K-1) | Claude (protokol), sen (yürüme) | **F5-55 sunucu entegrasyonunun başlaması** ve G5 | STATUS: T-NAV-02 "sunucu entegrasyonundan önce gerekir", "protokolü yazılmadı" (`docs/STATUS.md:390`); `maxSlope 0,625 [A]` bu ölçümü bekliyor (`docs/12:201`). T-NAV-09 (su) F5-55 "açık sorular"daki su katmanı kararını belirler (F5-55 §4); sunucu entegrasyonu gerektirmez `[Ö]`, bu yüzden şimdi yapılabilir |
| 2 | **Tek paket-izleyici oturumu** (kart K-2): T-REGENE-01, T-PARTY-01..03, T-PERC-01, T-CAST-FLY-01 (a)(b), `war-move` kalanı + priest/mage zamanlaması (Q-25) | sen | G4 insan kalemleri (`F4-taslak.md` §10); Q-25 → F6 priest/mage kapıları (G6b/G6c); CLI-14..20 `[A]`→`[V]` | Tek `--packet-trace` derlemesi, tek oturum, 5-6 test birden. Botlar bu sabitleri **muhafazakâr** koyduğu için bot şu an güvenli (alt sınırlar `[A]`); yani acil blokaj yok, ama en çok test kapatan tek oturum bu |
| 3 | **T-ARCH-05** (K-3): GM `+bot` | sen | F3 faz kabulünün tek insan koşulu | En kısa test; F3'ü kâğıtta kapatır (F3 kabulünün diğer yarısı Claude'da: §3 madde 4) |
| 4 | **Quest değişikliği doğrulaması** `testing`/`testmage` (K-4) | sen | Q-28 kararı (`UseStanding` quest kimlikleri), `db/003` bot quest betiğinin uygulanıp uygulanmayacağı | Kararın girdisi bu gözlem; mevcut profillerde etkisi sınırlı (72+ skill puan yüzünden zaten erişilemez, `docs/04:71`) |
| 5 | **T-DATA-02/03** (+T-DATA-05, Q-03, Q-05) (K-5) | sen | F1 kabulü; 8v8 ekipman varsayımı (G8 öncesi) | Sırf kabul + veri güveni; El Morad ekipmanı istemcide kuşanılabilir mi |
| 6 | **T-MECH-POT-03/04/05** (K-6) | sen | F1 kabulü; F6 hayatta kalma politikası girdisi (`docs/11:92`) | Pot hareketi durduruyor mu sorusu F6 geri çekilme tasarımını etkiler, şu an değil |
| 7 | T-MECH-BUF-01..08, T-MECH-SKILL-W/P/M, T-POT-01..03, T-MECH-DMG kalan (K-7) | **büyük ölçüde Claude (bot)**; insan yalnızca isteğe bağlı teyit | F1 kabulü | Priest/warrior skill ölçümü botla yapıldı (`docs/05` §9.1-§9.5); insanın yapacağı kalan yok denecek kadar az (§3) |
| — | **Planlanıyor (henüz yapılamaz):** T-NAV-04/05/10/11/06, AC-NAV-03, T-NAV-LOS-01, T-NAV-12 (kartlar P-1..P-8), T-IGT-WAR-01 (P-9) | Claude koşusu + insan teyidi | G5, G6a | F5 sunucu entegrasyonu ve F6-06 yazılıp uygulanana kadar çalıştırılamaz |

Beklerken bağımsız ve yetkili iş (kart/oturum yokken durmaz): bkz. §5 "Test penceresi dışında çalışabilen işler".

---

## 2. Ortak hazırlık (kartlarda `H-n` olarak anılır)

Kaynak: `docs/reports/gece-2026-10-03.md:119-144`, `docs/reports/gece-2026-10-03-nav.md:139-156`, `tools/build.sh`, `tools/run-servers.sh`, `tools/trace-session.sh`. Komutlar bu raporlardan birebirdir; "bu kapanışta ayrıca denemedim `[Ö]`" notu `gece-2026-10-03.md:144`'te raporun kendisindedir.

**H-0 Döngüyü durdur** (ayrıntı §5): `touch plans/.supervisor-stop`, sonra `touch plans/.auto-loop-stop`, `plans/_logs/auto-loop.log`'da durma satırını bekle, `tools/run-servers.sh status` ile sunucuların kapalı olduğunu doğrula.

**H-1 Bot kodlu derleme.** Bot kodu yalnızca bot kodlu derlemede vardır; `/mnt/c/dev/fdp/server/GameServer.exe` botsuz olabilir (`gece-2026-10-03.md:129`). Dal: `main` yeterli: F4-39 (paket izleyici), F4-38, F4-34 `main`'in atası `[V: git merge-base --is-ancestor]`; `gece/2026-10-02` yalnızca F4-49'dan sonrasını (F4-49, F4-53 saf mantık, F4-60 planı) fazladan taşır ve bu testler için gerekli değildir `[Ö]`.
```bash
cd /mnt/c/Users/frkoz/OneDrive/Desktop/Fire-Drake-Project-v1453
git branch --show-current                  # main (ya da gece/2026-10-02)
./tools/build.sh Release                   # normal derleme
./tools/run-tests.sh                       # (rapordaki beklenen: 251 tests, 0 failed; dal ucuna göre sayı artar)
./tools/run-servers.sh stop
S=/mnt/c/dev/fdp/server
cp $S/GameServer.exe $S/GameServer.exe.main-yedek
cp $S/GameServer.ini $S/GameServer.ini.yedek
cp build/bin/x86-Release/Server/GameServer.exe $S/GameServer.exe
```
Bayrakli derlemeler (`tools/build.sh:3-14`): paket izleyici `./tools/build.sh Release --packet-trace`; hasar kaydı `./tools/build.sh Release --damage-trace`. İzleyici oturumunu `tools/trace-session.sh prepare` (durdur, `--packet-trace` derle, başlat), `collect <etiket>`, `finish` (izleyicisiz yeniden derler) yönetir (`tools/trace-session.sh:4-9`, `:78-174`). Not: `run-servers.sh` varsayılan olarak exe'yi `build/bin/x86-<Config>/Server` içinden, ini ve `Logs/`'u `/mnt/c/dev/fdp/server` içinden kullanır; `FDP_SERVER_BIN_DIR` verilirse exe oradan (`tools/run-servers.sh:402-411`). `prepare`'in kullandığı exe bu yüzden build klasörüdür; yukarıdaki `cp` yalnızca `FDP_SERVER_BIN_DIR=$S` yoluyla başlatırken önemlidir.

**H-2 ini** (`$S/GameServer.ini`, `[BOT]`): `ENABLED=1`, `MAX_BOTS=16`, `TELEMETRY=decisions`, `SPAWN_ON_START` boş (`gece-2026-10-03.md:144`; anahtar tablosu `docs/13:109-122`). Başlat: `FDP_SERVER_BIN_DIR=/mnt/c/dev/fdp/server ./tools/run-servers.sh start`, `status` 3/3 `UP`.

**H-3 Komutlar:** sunucu konsoluna `/bot ...`, ya da `BotCommands.txt` (geçici adla yazıp `mv`), ya da oyun içi GM `+bot ...` (`gece-2026-10-03.md:144`). Betikler: `/bot script run <ad>` `./Scripts/<ad>.txt` okur (`plans/F4-20-…md:92`); örnek betikler `bots/config/*.txt`, sunucu dizinindeki `Scripts/` içine kopyalanır (`plans/F4-20-…md:182`). Botlar zone 71 `(1274, 890)` civarında ~10 sn'de doğar; **botları ≥ 3 sn arayla doğur** (KI-DEG-01), komutlar arası ≥ 1,05 sn (`gece-2026-10-03-nav.md:156`).

**H-4 Hesaplar** (`db/002_bot_characters.sql:32-37`, `:126-137`): Karus `BotAccWPK, BotAccWGK, BotAccPHDK, BotAccPHBK, BotAccMFK, BotAccMIK` (karakterler `BotWP_K, BotWG_K, BotPHD_K, BotPHB_K, BotMF_K, BotMI_K`); El Morad `BotAccWPE, BotAccWGE, BotAccPHDE, BotAccPHBE, BotAccMFE, BotAccMIE` (karakterler `BotWP_E, ... BotMI_E`). Hesap adı yalnızca harf/rakam. **Şifre repoda yok:** hesap ilk girişte otomatik açılıyor ve verdiğin şifre kaydediliyor (`docs/04:69`); önceden girdiğin hesaplarda önceki şifreni kullan. **Aynı karaktere hem insan hem bot olarak girme:** istemcide oynayacağın bot hesabının karakterini sunucuda `/bot spawn` ile doğurma (`gece-2026-10-03.md:123`). Hedef/rakip olarak başka bir bot karakteri doğurabilirsin.

**H-5 Bot satırı durumu:** HP/MP/NP ve konum DB'de kalıcıdır; ölü ya da düşük HP'li doğabilir (ör. `BotMF_K` ölü kayıtlıydı, `BotMF_E` bir koşuda öldü; `gece-2026-10-03.md:124`, `docs/STATUS.md:373`). Ölü/NP'siz doğarsa Claude'a söyle ya da `pot <bot> 389015000 <n>` kullan. Hedefli geri yazım (yalnızca bot satırları): `Hp=Mp=32000`, `PX/PZ=127400/89000`, `Loyalty=1000`; DB yazımı Claude'un işidir, izin ister.

**H-6 Bitince geri al:** `tools/run-servers.sh stop`; `cp $S/GameServer.exe.main-yedek $S/GameServer.exe`; `cp $S/GameServer.ini.yedek $S/GameServer.ini`. Paket izleyicili derlemeden sonra `tools/trace-session.sh finish`. `plans/_logs/trace/` içindeki ham kayıtlar ad içerir: paylaşma, git'e ekleme (`docs/15:158`).

---

## 3. Claude'un kendi yapabileceği / yapacağı doğrulamalar (insana gerek bırakmıyor)

Hepsi sunucu açmayı gerektirir; bu koşu salt-okunur olduğundan **çalıştırılmadı**. İnsan oturumuyla çakışmamaları için ya insan penceresinden önce ya sonra, H-0 sonrası tek sunucu setiyle yapılır.

| # | Kalem | Insana gerek yok çünkü | Gerekli izin/önkoşul |
|---|---|---|---|
| 1 | **T-CAST-CANCEL-01 (b)** (`UseStanding` otomatik durdurma) | F4-24 planı bunu Claude'un işi olarak tanımladı: geçici `UPDATE MAGIC SET UseStanding = 1 WHERE MagicNum = <bir warrior Type1 skill>` (yalnızca oyun verisi tablosu), sunucu yeniden başlat, `/bot move BotWP_K <≥ 20 m>` ~1 sn sonra `/bot cast BotWP_K <skill> BotWP_E 1`; beklenen önce `Move speed:0`, ≥ 1 tick sonra `CastEffect effected`, `FAIRNESS_REJECT not_standing` yok (`plans/F4-24-…md:273`). STATUS bunu "insan istemcisi" diye listelese de adım sunucu tarafıdır `[Ö]` | DB yazma izni (`MAGIC`, önceki değeri not al, bitince geri yaz); sizin onayınız |
| 2 | **T-MECH-SKILL-M** (mage) botla | W ve P ölçüldü (`docs/05` §9.1-§9.5); mage dilimi F4-48 (`bots/config/skill_mage_k_single/area/area_heavy.txt`), çalışma zamanı K9/K10 "sabaha ertelendi" (`docs/STATUS.md:3`). `tools/skill-check.py` ile hüküm | sunucu açık, `BotMF_K` canlı/HP dolu |
| 3 | **T-MECH-BUF-01..08** (çakışma, debuff buff'ı siler, Confusion) | Type4 buff/debuff artık botla atılıyor (F4-28/F4-42/F4-44); sonuç kodları (`srv_fail -103` vb.) ve self buff listesi (`snap`) yeterli. Hedefin buff listesi için F4-60 sunucu bağlaması sonrası daha güçlü `[Ö]` | doğrulanamadı: her BUF numarasının (01..08) tam tanımı repoda ayrı listelenmiyor (yalnızca `docs/05:57` "01..07", `docs/15:119` "01..08") |
| 4 | **F3 kabul ölçümü: `decisions` seviyesinde bot telemetri ek maliyeti ve açık/kapalı tick farkı** | Karar/aksiyon olayları artık var; ölçüm Claude'da (`F3-taslak.md` §3, §5, §10). **Sınır:** yalnızca 12 sabit bot var (`BOT_TABLE`), "16 bot" için 16/20 karakter betiği (ek 4-8 karakter, `db/005`/`db/006`, `f4daa27`) ve DB yazma izni gerekir | izin; sunucular kapalıyken DB yazımı |
| 5 | **MET-ACT-02 / MET-FAIR-01 büyük örnek, 12-16 bot tick maliyeti** (G4) | `docs/17:301` G4 yöntem sınırı: "Çalışma zamanı doğrulamasını Claude yapar" | 4'tekiyle aynı |
| 6 | **T-MECH-DMG kalan:** buz/uçan skill, Prismatic, DoT toplamı, heal | Bot artık bunları atabilir (`gece-2026-10-03.md:161`, F4-26/F4-37); `--damage-trace` derlemesi ve `tools/damage-trace-summary.py` botla da çalışır (T-MECH-DMG-03 `/bot cast` ile yapıldı, `docs/STATUS.md:353`). Heal'ın hesabı sunucu tarafıdır `[Ö]`; gerçek istemci yalnızca isteğe bağlı teyit | `./tools/build.sh Release --damage-trace`, sonra bayraksız ikiliyi geri koy |
| 7 | **T-POT-01** bot politikası/cooldown | F4-04'te 2508-2528 ms ölçüldü `[V]` (`docs/11:40`); T-POT-02/03 karar katmanı (F6) gerektirir, şimdi yapılamaz | — |
| 8 | **T-ENV-ARENA-01** | zaten otomatik GEÇTİ (33/33 tarama, `docs/15:68`); insan gözlemi isteğe bağlı | — |
| 9 | **Faz taslaklarını güncelleme** (F1/F2/F4 taslakları eski) | Yalnızca doküman işi (§7) | — |
| 10 | **`python3 tools/bot-composition-check.py`, `bot-outcome-eval.py --selftest`, `bash tools/nav-regress.sh`** (istemcisiz isteğe bağlı kontrol) | `docs/STATUS.md:390`, `gece-2026-10-03-nav.md:172` | `g++` (nav-regress) |

Insana **gerçekten** gerekenler: gerçek 1453 istemcisinin paket davranışı (K-2), harita/eğim/su gözlemi (K-1), GM istemcisi (K-3), istemci arayüzü gözlemleri (K-4, K-5, K-6).

---

## 4. Test kartları

Kart alanları: engellediği iş/faz kapısı · build · hesap/karakter · hazırlık · beklenen/geçme · kanıt · tahmini süre ("belirtilmemiş" = docs'ta yok; "senaryo süresi" dokümandaki tekrar/süre, senin harcayacağın toplam değil).

### A. Şimdi yapılabilir

#### K-1 Harita yürüyüş oturumu: T-NAV-02 + T-NAV-09 + T-ENV-ARENA-02 (protokol yazılacak)
- **Engellediği:** F5-55 sunucu entegrasyonu (`docs/STATUS.md:390`: T-NAV-02 entegrasyondan önce gerekir), su katmanı kararı (`docs/18` Q-26, F5-55 §4), G5. T-ENV-ARENA-02: F1 kabulü (`docs/STATUS.md:395`).
- **Durum:** T-NAV-02 ve T-NAV-09 için **protokol/komut yazılmadı**; bu yüzden aşağıdaki hazırlık Claude'dan istenecek. `docs/12:167` yalnızca amacı verir ("istemcinin tırmanamadığı eğimlerin işaretlenmesi"); göl koordinatları repoda yok (doğrulanamadı).
- **Build:** `--packet-trace` önerilir `[Ö]` (WIZ_MOVE konum kaydı, `tools/trace-session.sh prepare`); T-ENV-ARENA-02 için normal Release yeter.
- **Hesap:** "herhangi bir bot hesabı (GM ile)" (`docs/STATUS.md:395`); GM hesabının adı repoda yok (doğrulanamadı).
- **Hazırlık:** H-0, H-1 (izleyicili), H-4. Claude'dan: (i) eğim denemesi için koordinat listesi (zone 71 `.navgrid`/`tools/nav-export.py` verisinden `[Ö]`), (ii) göl kıyısı koordinatları ve su içi yürüme adımları, (iii) T-ENV-ARENA-02 için arena A (1274, 890), yarıçap 60 m, ±35 m başlangıç noktaları (`docs/15:51-58`) üzerinde yürüme listesi.
- **Beklenen/geçme:** T-ENV-ARENA-02 "koordinat ve görüntü kaydı" (`docs/15:104`); T-NAV-02 tırmanılamayan eğimler işaretli, `maxSlope` `[A]` → `[V]` ya da düzeltme; T-NAV-09 (`docs/12:201`, `docs/15:168`): istemci suya/göl cebine giriyor mu, suda yavaşlıyor mu, kıyı olay ızgarasıyla uyumlu mu.
- **Kanıt:** `Logs/PacketTrace_<g>_<a>_<y>.log` (`GameServer/PacketTrace.cpp:106`), `tools/trace-session.sh collect <etiket>`, `python3 tools/packet-trace-summary.py <log> --sid <sid>`; ekran görüntüsü (arena sınırları, su kenarı).
- **Süre:** belirtilmemiş.

#### K-2 Tek paket-izleyici oturumu (T-REGENE-01, T-PARTY-01..03, T-PERC-01, T-CAST-FLY-01 (a)(b), war-move kalanı, Q-25)
- **Engellediği:** G4 insan kalemleri (`phase-reports/F4-taslak.md` §10), CLI-14..CLI-20 `[A]`→`[V]` (`docs/03:393-399`), MET-FAIR-01 gerçek istemci karşılaştırması; Q-25 → F6 priest/mage (docs/18:74); `war-move` → F1 (Q-02).
- **Build:** `./tools/build.sh Release --packet-trace` (veya `tools/trace-session.sh prepare`). İzleyici artık WIZ_PARTY/REGENE/REQ_USERIN/REQ_NPCIN/CHAT'i ve giden WIZ_DEAD/REGIONCHANGE/NPC_REGION/REGENE/PARTY'yi yazar (`GameServer/PacketTrace.cpp:18-60`, F4-39). Chat metni ve party adı yazılmaz.
- **Hesaplar:** iki gerçek istemci, karşı ulus gerekir (PK için): ör. `BotAccWPK` (karakter `BotWP_K`) ve `BotAccWPE` (`BotWP_E`). Üç kişilik party için üçüncü hesap (T-PARTY-03). Mage için `BotAccMFK` (`BotMF_K`); hedef olarak botlardan biri (`/bot spawn BotPHD_E`; (c) görsel testinde hedef `BotPHD_E` idi, `docs/STATUS.md:379`). İnsan oynadığı bot karakterini `/bot spawn`'lama (H-4).
- **Hazırlık komutları:**
  `tools/trace-session.sh prepare` · oyuna giriş · her senaryodan önce 3 sn bekle, bitince 3 sn bekle · `tools/trace-session.sh collect <etiket>` · oyundan çık · `tools/trace-session.sh finish` (`docs/15:125-162`). Özet: `python3 tools/packet-trace-summary.py <log> --cli --sid <sid>` (CLI-14..CLI-20 bölümleri).
- **Alt testler:**

| Alt test | Ne yapılır | Geçme ölçütü (`docs/STATUS.md:399-406`, `gece-2026-10-03.md:150-156`) | Senaryo süresi |
|---|---|---|---|
| T-REGENE-01 (CLI-14) | Biri diğerini öldürsün; ölen "yeniden doğ"a kendi temposunda bassın; NP > 0 olmalı (NP 0 ise doğuş sonunda ana zone'a ışınlama, KI-013, `docs/03:393`) | `WIZ_DEAD` → `WIZ_REGENE` ≥ 3 sn (kısaysa `kRegeneMinDeadMs = 3000`, `BotCore/BotCombat.h:749` ve CLI-14 güncellenir) | 5 tekrar |
| T-PARTY-01 (CLI-15) | 5 art arda davet (her biri farklı hedefe), davet edilen 5 kez kabul | davetler arası ve davet→kabul ≥ 1 sn (`kPartyInviteGapMs`/`kPartyAcceptMinMs`) | 5 tekrar |
| T-PARTY-02 (CLI-16) | 5 ret; sonra kabul + 5 ayrılma (üye 3, lider 2) | davet→`PARTY_PERMIT 0` ve giriş→ayrılma ≥ 1 sn; **liderin ayrılırken `PARTY_REMOVE` mi `PARTY_DELETE` mi** yolladığı kaydedilir | 5 + 5 |
| T-PARTY-03 (CLI-17) | 3 kişilik party; lider 5 art arda devir/atma | ardışık `PARTY_PROMOTE`/`PARTY_REMOVE` ≥ 1 sn (`kPartyManageGapMs`) | 5 tekrar |
| T-PERC-01 (CLI-19/20) | biri diğerinden 3+ bölge (> 144 m) uzakta başlasın; yaklaşan oyuncu görüş alanına girsin (bölge sınırını gidip gelerek de); NPC tarafı için arena A tower halkası içine yürü | `WIZ_REGIONCHANGE` → `WIZ_REQ_USERIN` ve ardışık istek arası ≥ 1,0 sn, istekte ≤ 32 kimlik; NPC için aynı (`kUserInMinGapMs`/`kUserInMaxIds`/`kNpcInMinGapMs`/`kNpcInMaxIds`); istemci hiç istek yollamıyorsa bunu da yaz | 5 tekrar |
| T-CAST-FLY-01 (a)(b) (F4-25) | Karus mage ile Fire ball `110515` (varsa Fire spear/Static orb) tek hedefe 10 kez; her atışta MP'yi not et | CASTING→FLYING ≈ 1540 ms, FLYING→EFFECTING ≈ 1000 ms (mesafeye bağlı); FLYING `sData[0..2]` ve `target`; atış başına MP düşüşü 2 × `Msp` = 100 (MEC-MAG-12). Sonuç `docs/03` CLI-03/MEC-MAG-12 `[A]`→`[V]` | 10 atış |
| `war-move` kalanı (Q-02) + Q-25 priest/mage | `docs/15` §4.2.1 etiketleri: `pri-cast`, `pri-cancel`, `mag-cast`, `mag-cancel` (priest 20 cast + 10 iptal, mage aynı), isteğe bağlı `idle` (5 dk), `target`, `pot` | `docs/03` §13.2'deki `[A]`'ların kalkması; Q-25: priest/mage insan aksiyon hızı ve cast döngüsü dağılımı (`docs/18:74`). **Heal/cure rotasyonu için tanımlı bir etiket yok** (doğrulanamadı; protokol Claude'dan) | belirtilmemiş (etiket başına: `idle` 5 dk; war-r 30 sn) |

- **Kanıt:** `Logs/PacketTrace_*.log` (ad içerir, paylaşma), `plans/_logs/trace/<etiket>.log` ve `<etiket>.summary.txt` (yalnızca adsız özetler rapora girer, `docs/15:158`), sunucu tarafında `Logs/bots/<tarih>/live-*.jsonl`, `Logs/Bot_<g>_<a>_<y>.log`; CAST için ekran görüntüsü isteğe bağlı.
- **Toplam süre:** belirtilmemiş.
- **Çakışma:** bu oturumda ne dönen döngü ne ikinci bir sunucu seti açık olmamalı (§5).

#### K-3 T-ARCH-05 (F3-04): oyun içi GM `+bot`
- **Engellediği:** F3 faz kabulü (insan koşulu, `F3-taslak.md` §10).
- **Build:** normal Release, bot kodlu (H-1).
- **Hesap:** **GM yetkili bir hesap** (adı repoda yok: doğrulanamadı; mevcut GM hesabın ya da yetkiyi senin vereceğin biri) + **GM olmayan** normal hesap (örn. herhangi bir bot hesabı olabilir `[Ö]`, ama o bot sunucuda doğurulmamış olmalı).
- **Hazırlık:** H-0, H-1, H-2 (`SPAWN_ON_START` boş); sunucu açık, GM hesabıyla zone 71'e gir.
- **Adımlar** (`docs/STATUS.md:398`): sohbete sırayla `+bot`, `+bot spawn BotWP_K,BotMF_K`, ~10 sn sonra `+bot list`, `+bot despawn all`, `+bot list`; sonra GM olmayan hesapla `+bot list`.
- **Beklenen:** `+bot` iki satırlık kullanım; `spawn` "Bot command queued", ~10 sn içinde iki bot görünür; `list` başlık `Bots: 2 session(s), pool free 14/16` + iki `in_game`; `despawn all` sonrası `list` iki `despawned`; GM olmayanda komut düz sohbet metni, yanıt yok.
- **Kanıt:** oyun içi sohbet ekran görüntüsü; `Logs/Bot_*.log` sonuç satırları; GM komutu sohbet günlüğüne de düşer (`plans/F3-04-…md` §5, `ChatHandler.cpp:104-117`).
- **Süre:** belirtilmemiş.

#### K-4 Quest değişikliği doğrulaması: `testing` ve `testmage`
- **Engellediği:** Q-28 kararı (51-54 quest kimlikleri `UseStanding`'de, `docs/18:77` Q-28 `f4daa27` numaralaması), `db/003` bot quest betiğinin DB'ye uygulanması kararı; KI-018 (istemci "?" kilidi).
- **Durum:** proje sahibi bildirimi (doğrulanmadı): `testing` ve `testmage` karakterlerinin `USERDATA.strQuest` alanına 18 skill quest'i (51-54, 510-523) durum 2 olarak işlendi, yedek `dbo.USERDATA_QUEST_FIX_BACKUP` (2 satır), **betikler depoda yok** (`docs/STATUS.md:6`). Ben DB satırını okumadım (kural). Hesap adları repoda yok, yalnızca karakter adları (doğrulanamadı).
- **Build:** normal Release yeterli (bot kodu gerekmez `[Ö]`); sunucu `Release` olmalı (quest kapısı yalnızca Release'de, `docs/03` MEC-MAG-14 / `MagicInstance.cpp:269-275`).
- **Adımlar** `[Ö]` (doc'ta adım listesi yok; MEC-MAG-14 ve `tools/client-tbl-quests.py` açıklamasından): iki karakterle gir; skill penceresinde 51-54 ve 510-523 quest kimlikli skill'lerin "?" ikonunun kalkıp kalkmadığına bak; kilitliydi denen birini at.
  Beklenen skill listesini görmek için salt-okunur: `python3 tools/client-tbl-quests.py --list` (`tools/client-tbl-quests.py:20-23`).
- **Beklenen:** istemci kilidi kalkmış ve skill atılabiliyor (hipotez `[A]`, istemci exe'sinden doğrulanmadı, `docs/03` MEC-MAG-14); sunucu `Etc` ≠ 0 olan 72 skill için `CheckExistEvent(sEtc, 2)` ister.
- **Kanıt:** skill penceresi ekran görüntüsü (kilitli/kilitsiz), `Logs/` içinde cast sonucu; sonuç Q-28/ADR'ye işlenir.
- **Geri alma:** yedek tablo `dbo.USERDATA_QUEST_FIX_BACKUP`; geri alma betiği repoda yok.
- **Süre:** belirtilmemiş.

#### K-5 T-DATA-02 / T-DATA-03 (+T-DATA-05, Q-03, Q-05): referans ekipmanın istemcide kuşanılabilirliği ve değerler
- **Engellediği:** F1 kabulü; Q-05 (sınıf/ırk kısıtı, CHR-05) ve Q-21 çalışma zamanı değeri (MB-12 maks ağırlık).
- **Build:** normal Release (bot kodu gerekmez; botları doğurma).
- **Hesaplar:** T-DATA-01 yalnızca birkaç karakteri girdi (`docs/04:75`: W-P, W-G, P-HD/P-HB, M-F, M-I; **ulus belirtilmemiş**, doğrulanamadı). Kalan ya da hiç girilmemiş olanlar için bot hesapları (H-4); özellikle El Morad hesapları `BotAccPHDE, BotAccPHBE, BotAccMFE, BotAccMIE` (ve `BotAccWPE/WGE` M-I gibi girilmediyse). Hangilerinin girildiği kayıtta yok: doğrulanamadı.
- **Hazırlık:** sunucu açık (bot gerekmez; `ENABLED=0` yeter `[Ö]`), karakterle gir, karakter penceresi.
- **Beklenen** (`docs/04:75`, `:284-292`): ekipman tam takılı; HP/MP/AC/saldırı modelle uyumlu (W-P `5650/5370/857/1947`, W-G `5650/5370/1488/1138`, P-HD/P-HB `3491/6392/923/418`, M-F `1541/6021/605/57`, M-I `2228/6021/612/57`; ulus farkı için El Morad değerleri doc'ta yok: doğrulanamadı); ağırlık: 100 × 1440 HP pot şablonu 8/12 botu ağırlık sınırının üstüne çıkarır (yavaşlama sebebi, `gece-2026-10-03.md:158`); T-DATA-05: istemcide ağırlık sınırı (MB-12 gerçek değer).
  Q-03 (karakter oluşturma sonrası seviye 51 gözlemi): yalnızca **kendi test hesabında** yeni karakter oluştur ve seviyeyi izle (`docs/18` Q-03).
- **Kanıt:** karakter penceresi ekran görüntüsü, her karakter için HP/MP/AC/saldırı ve ağırlık satırı.
- **Süre:** belirtilmemiş.

#### K-6 T-MECH-POT-03 / 04 / 05 (Q-06)
- **Engellediği:** F1 kabulü; F6 hayatta kalma/pot politikasının varsayımları (`docs/11:22`, `:42`, `:92`).
- **Durum:** CLI-06: HP ve MP pot aralıkları ~2,5 sn, HP→MP geçişi 2540 ms (`docs/03:427`): "ortak bekleme muhtemel, ayrıca kanıtlanmadı".
- **Build:** `--packet-trace` ile (opcode 0x31 `MAGIC_PROCESS` ve `WIZ_MOVE` aynı kayıtta) `[Ö]`; ayrıntı protokolü doc'ta yok.
- **Hesap:** herhangi bir bot hesabı (ör. `BotAccWPE`, çanta stoğu için H-4 ve gerekirse `tools/bot-refill.sh apply` sunucular kapalıyken, işin bitince `rollback`, `gece-2026-10-03.md:125`).
- **Beklenen:** (03) HP potundan hemen sonra MP potu `< 2,5 sn`'de kullanılabiliyor mu (ayrı zamanlayıcı?); (04) pot içerirken yürüme durur mu; (05) envanterde olmayan pot kullanılabiliyor mu. Sonuç `docs/11` ve CLI-06 `[A]`'yı günceller; ayrı zamanlayıcı çıkarsa politika "her grup 2,5 sn"e gevşetilebilir (yeni karar, `docs/11:42`).
- **Kanıt:** `PacketTrace` kaydı + `packet-trace-summary.py --cli`, `pot` etiketi (`docs/15:154`).
- **Süre:** `pot` etiketi: HP potu 20 sn, MP potu 20 sn (`docs/15:154`); toplam belirtilmemiş.

#### K-7 İsteğe bağlı: insan teyidi (BUF / SKILL / DMG kalanı)
- **Engellediği:** F1 kabulü (T-MECH-SKILL/BUF/POT "TEST_EDILDI" olmalı, `docs/17:79`); bot ölçümü bu kalemlerin çoğunu karşıladığından insan yalnızca "bot ölçümü kabul mü" kararını verir (§6).
- **Build:** `--damage-trace` (hasar/heal) ya da normal.
- **Hesap/komut:** `docs/STATUS.md:394`: iki insan istemcisi (ör. `BotAccMFK` vs `BotAccWPE`; heal için `BotAccPHDK`/`BotAccPHBK`); `python3 tools/damage-trace-summary.py Logs/DamageTrace_*.log --stat-model stat.txt --spell-model spell.txt` (`docs/15` / `gece-2026-10-03.md:157`; modeller `tools/stat-model.py`, `tools/spell-model.py` çıktısıdır, `tools/damage-trace-summary.py:30-35`).
- **Beklenen:** her grup ± %15 içinde `OK`; bayraksız ikiliyi geri koy.
- **Kanıt:** `Logs/DamageTrace_<g>_<a>_<y>.log` + özet çıktısı. **Süre:** belirtilmemiş.

### B. Planlanıyor (henüz çalıştırılamaz; taslak kartlar)

> Plan numaraları (F5-59..F5-66, F6-06) isteğinde geçiyor; **repoda bu numaralarda plan/satır yok** (`git grep` `gece/2026-10-02` ve `main`: yalnızca F5-55 `TASLAK` ve kendi dört dilimi, F5-55 §1). Eşleme dilimlerle yapıldı; numara eşleşmesi doğrulanamadı. Hepsi `degerlendirme-takip.md` mantığıyla "plan hazır → kod uygulandı → oyun içinde doğrulandı" ayrı izlenir.

Ortak build: **normal Release** + `[BOT] NAV=1` (varsayılan 0, `F5-55 §1`; ini anahtarı henüz yok); T-NAV-11 için MSVC **Release** zorunlu (WSL `g++` ölçümleri yeterli sayılmaz, `docs/12:191`).

| Kart | Test | Engellediği | Çalıştıran | İnsan teyidi gereken kısım (`[Ö]` öneri, doc'ta yok) | Sayısal ölçüt (kaynak) | Hesap/hazırlık | Kanıt | Süre |
|---|---|---|---|---|---|---|---|---|
| P-1 | **T-NAV-04** dar geçit/köprü 50 geçiş | G5 (AC-NAV-01, AC-NAV-03) | Claude | Botun geçişi istemcide doğal görünüyor mu, duvara/çıkıntıya sürtme var mı (izleme) | takılma ≤ 2/bot-saat, kurtarma p95 ≤ 5 sn; paket aralığı normalken yanlış alarm 0 (`F5-55 §1.4`, `docs/12:178`) | bot hesapları (H-4), `/bot goto` (F5-55 dilimi 3'te gelecek; şimdi yok); gözlemci: `BotAccWPK` gibi bir insan hesabı | `NAV_STUCK`/`NAV_RECOVERY` telemetrisi (`docs/16` §3.2), `Logs/bots/<tarih>/live-*.jsonl` | 50 geçiş; süre belirtilmemiş |
| P-2 | **T-NAV-05 / T-NAV-10** ölüm → `WIZ_REGENE` → doğuş → arena dönüşü, **Karus ve El Morad ayrı** | G5 (AC-NAV-01, MET-NAV-02) | Claude | Yolda kuleye/yasak halkaya yaklaşma, arena kenarında davranış gözlemi; ulus başına en az bir kez insan gözüyle izle | her ulus ≥ 10 tekrar; süre Karus ~52 sn, El Morad ~142 sn ± %20; takılma/`NodeLimit`/`InvalidGoal` 0; arena sınırı içinde kal (`F5-55 §1.4`, `docs/15:168`) | `BotWP_K`, `BotWP_E` vb.; NP > 0 (KI-013); insan rotası karşılaştırması: Karus 69,8 sn/323 m, El Morad 165,1 sn/731 m (`docs/15:59-67`) | ulus bazlı ayrı rapor (ADR-0033-DEG), `tools/bot-telemetry-report.py` | doc süreleri × tekrar: Karus 10 × ~52 sn ≈ 9 dk, El Morad 10 × ~142 sn ≈ 24 dk (**hesaplama**) |
| P-3 | **T-NAV-06** hareketli hedef takibi (kiting mage) | G5, F6 mage kiting girdisi | Claude + insan | **İnsan hareketli hedef olur** (istemciyle koşar/kaçar), bot takip eder: bayat hedefe sürme, gecikme gözlemi (gözlem aralığı ~1,5 sn, hız kestirimi F5-52/F5-56) | doc'ta sayısal ölçüt yok: doğrulanamadı | iki hesap, biri insan istemcisi, biri bot | `NAV_PATH` telemetrisi + ekran kaydı | belirtilmemiş |
| P-4 | **T-NAV-09** su ve göl kıyısı (insan gözlemi) | G5, su katmanı kararı (Q-26) | **insan** | tüm test insan | istemci suya giriyor mu, suda yavaşlıyor mu, kıyı olay ızgarasıyla uyum (`docs/12:201`) | K-1 oturumunda yapılabilir (sunucu entegrasyonu gerekmez `[Ö]`) | `PacketTrace` konum kaydı + ekran görüntüsü | belirtilmemiş |
| P-5 | **T-NAV-11** 16 botta oyun içi tick ve yol bulma bütçesi | G5 (AC-NAV-07, MET-PERF-02/03) | Claude | yok (isteğe bağlı: oyun içi takılma/gecikme hissi `[Ö]`) | `PERF_SAMPLE` nav payı p95 ≤ 1,5 ms; toplam tick p95 ≤ 5 ms; bayat planla sürme 0; ilk plan beklemesi ≤ 1,1 sn; gerçek `BotManager` tick'i, **MSVC Release** (`F5-55 §1.4`) | **16 bot** için 12 sabit botun ötesi gerekir (`db/005`/`db/006`, DB yazma izni) | `Logs/bots/<tarih>/` `PERF_SAMPLE` | 30 dk (`docs/15:168`) |
| P-6 | **AC-NAV-03** engelli hücreye giren hareket = 0; düz `/bot move` engel kesen adımı `FAIRNESS_REJECT CLI-08` ile durdurur | G5 | Claude | yok | 30 dk sunucu denetim logunda 0 ihlal, planlayıcı yollarında CLI-08 reddi 0 (`F5-55 §3`) | 3 bot, arena A | sunucu denetim logu | 30 dk |
| P-7 | **T-NAV-LOS-01** istemcinin engel arkasına skill kullanımı | F5-10 `Advisory`/`Enforce` kararı (`P-NAV-LOS-MODE`), `docs/12:79` | **insan** | tüm test insan; bot sunucusu bağlanınca (`gece-2026-10-03-nav.md:174`); protokol yazılmadı | istemci engel arkasındaki hedefe skill atabiliyor mu | iki istemci | `PacketTrace` + ekran kaydı | belirtilmemiş |
| P-8 | **T-NAV-12** bowl turu | F11 planlaması girdisi (G5'i bloke etmez `[Ö]`) | Claude + insan | canavar yoğun halka (≈ (1024, 1024), r ~150 m); proje sahibi gözlemi | 10 dk dolaşma: canavar saldırısı/dk, `danger_static` ile kaçınma, takılma/bot-saat (`docs/15:168`) | serbest Ronark; arena A dışı | telemetri | 10 dk (`docs/15:168`) |
| P-9 | **T-IGT-WAR-01** warrior solo uçtan uca (F6-06) | **G6a** (`docs/17:303`); F5 kabulü ve ADR-0018 m.4 sprint/Type4 self önkoşul | Claude (telemetri) + insan gözlemi | insan gözlemi: warrior kendi kararıyla hedefe ulaşıyor ve baskı kuruyor mu | MET-TGT-01 p50 ≤ 4 sn; MET-TGT-02 ≥ %70; MET-ACT-02 ≤ %1; MET-NAV-01 ≤ 2/bot-saat (`docs/15:237`; T-WAR-01..03: hareketli hedef, 60 sn × 10 tekrar, solo) | `BotWP_K` (solo, arena A) | `tools/bot-telemetry-report.py` + ekran kaydı | 60 sn × 10 tekrar = 10 dk (**hesaplama**) |

---

## 5. Önkoşul / çakışma notları

**Genel**
- **Aynı bot hesabı hem istemcide hem bot olarak açılamaz** (`gece-2026-10-03.md:123`, `docs/STATUS.md:373`). İstemciyle girdiğin karakteri sunucuda `/bot spawn` etme; hedef/rakip için başka bot karakteri doğur.
- **Botlar ≥ 3 sn arayla doğurulur** (KI-DEG-01: tek komutta doğan botlarda 6/6 `ObsTable` asimetri); komutlar arası ≥ 1,05 sn.
- Bot satırlarının HP/MP/NP/konumu DB'de kalıcı; testten önce `list` ile bak (H-5).
- Bayraklı derlemeden (`--packet-trace`/`--damage-trace`) sonra bayraksız ikiliyi geri koy (`finish`/`cp .main-yedek`); izleyicili ikiliyle normal oynama.
- İzleyici/hasar kayıtları ad içerir: paylaşma, git'e ekleme.

**Döngü ve sunucu çakışması** (`plans/OTONOM_DONGU.md:16-32`, `tools/auto-loop.sh`)
- Döngü her adımdan önce açık sunucuları kapatır (`auto-loop.sh:160-170`, `ensure_servers_stopped`); `run-servers.sh stop` bağlı istemci varsa reddeder (`[REFUSE] ... durdurmak için --force`, `run-servers.sh:38`, `:566`), bu durumda döngü yalnızca uyarı yazıp devam eder ve derleme kilitlenebilir (`auto-loop.sh:167`).
- Döngü aynı çalışma ağacında dal değiştirir (`git switch`), kirli çalışma ağacını `git stash push -u` ile kenara alır (`auto-loop.sh:172-179`): **döngü çalışırken çalışma ağacında elle değişiklik yapma ya da build alma**.
- Döngünün kendi doğrulama adımları sunucu açar; senin istemcin bağlıyken yarışırlar.
- Döngü `main`'e dokunmaz, push yapmaz (gece modu, `OTONOM_DONGU.md:20`); `CLAUDE.md` gereği `main` birleştirmesi interaktif oturumda yapılır.

**İnsan testi için döngüyü durdurma / duraklatma**
1. **Önce denetleyici** (yoksa döngüyü yeniden başlatır): ana klasörde `touch plans/.supervisor-stop` (`docs/reports/durum-ozeti-ve-dogrulama-listesi-2026-10-03.md:72`, `main`'de). Denetleyici betik `/tmp/claude-1000/supervisor/auto-supervisor.sh` repoda yok, davranışı (bayrağı silip silmediği, yeniden başlatma komutu) **doğrulanamadı**.
2. **Sonra döngü:** `touch plans/.auto-loop-stop`. Döngü bu dosyayı **her iterasyonun başında** kontrol eder, bulursa dosyayı silip temiz çıkar (`auto-loop.sh:147`, `:488`; `OTONOM_DONGU.md:147`). **Yani anında durmaz:** süren adım bitmeden (opencode zaman aşımı 7200 sn, Claude 3600 sn, `auto-loop.sh:36-37`) kapanmaz. İzle: `cat plans/_logs/auto-loop.state` (tek satır), `tail -f plans/_logs/auto-loop.log` ("elle durdurma" satırı). Aceleniz varsa süreci `kill` etmek de belgelenmiş yoldur (`OTONOM_DONGU.md:147`), ama süren planın dalı yarım kalır.
3. Doğrula: `tools/run-servers.sh status` (döngü sunucusu açıksa `stop`), ardından H-1.
4. **Yeniden başlatma:** `nohup ./tools/auto-loop.sh --run --branch gece/2026-10-02 --target <faz> > plans/_logs/auto-loop.out 2>&1 &` kalıbı `OTONOM_DONGU.md:141-146` ve `auto-loop.sh:10` üzerindendir; **denetleyicinin yeniden başlatılması için komut repoda yok** (doğrulanamadı). `.supervisor-stop` ve `.auto-loop-done` bayraklarını yeniden başlatmadan önce kontrol et.
5. "Duraklat" diye ayrı bir mekanizma yoktur; durdur-sonra-yeniden-başlat tek yoldur `[V]`.

**Test penceresi dışında çalışabilen işler (bağımsız ve yetkili)**
- Salt kod/saf mantık planları sunucu gerektirmeyenler (`BotCore`, `tools/`, `docs/`): F4-60'ın çalışma zamanı sınaması **sunucu gerektirir** (F4-53 notu), bu yüzden insan penceresiyle çakışır; F5-55 kod dilimleri doğrulamada sunucu ister.
- Paralel `--track nav` hattı **sunucuya dokunmaz** ve ayrı worktree/dalda çalışır (`OTONOM_DONGU.md:158-167`; `ensure_servers_stopped` boş işlem, `auto-loop.sh:162`); ana hat insan testi için durdurulurken saf mantık işleri bu hatta sürebilir. Not: durum özeti nav hattını "emekli" diyor (`durum-ozeti…md:§3`), yeniden etkinleştirmek senin kararın.
- Claude: T-NAV-02/T-NAV-09 protokolü, F1/F2/F4 faz raporu taslak güncellemesi, Q-28 ADR taslağı, K-2 oturum sonrası `docs/03`/`docs/18` etiket yükseltmeleri; hepsi dokümandır, sunucu istemez.

---

## 6. Faz kabulü için gereken test setleri (karar sende; ben hiçbirini `KABUL_EDILDI` yapmadım)

Kaynak: `docs/phase-reports/F1..F4-taslak.md` §10, `docs/17:282-312` (faz kapısı denetim listesi ve G4/G5/G6a), `docs/STATUS.md:569-584` "Faz kabul takibi". Ortak koşul (`docs/17:282-292`): iş kalemleri GELIŞTIRILDI, testler TEST_EDILDI ve kanıtlı, her kabul kriteri ölçülü, KI kayıtları, dokümanlar, `docs/20`, geri alma, faz raporu onayı, **oyun içi kanıt**.

| Faz | Mevcut durum | Kabul için hâlâ gereken insan testi | Kabul için gereken Claude/ölçüm kalemi | Karar/kapsam sorusu |
|---|---|---|---|---|
| F0 | `KABUL_EDILDI` (2026-10-02) | — | — | — |
| F1 | taslak kısmi (`F1-taslak.md:§10`) | **T-DATA-02/03** (K-5), **T-ENV-ARENA-02** (K-1) (01/03/04 GEÇTİ), **`war-move` ikinci priest/mage oturumu** (K-2), **T-MECH-POT-03/04/05** (K-6), Q-03 (K-5). T-MECH-DMG-01..03 GEÇTİ | T-MECH-SKILL/BUF botla (§3 madde 2-3); `docs/03` etiket yükseltmeleri; Q-03/Q-06 cevapları | T-MECH-SKILL/BUF/POT için **bot ölçümü insan testinin yerine geçsin mi?** (docs/17:79 "tüm T-MECH testlerinin TEST_EDILDI olması"); T-MECH-CLIENT kapsamı "tek karakter/sınıf" notuyla yeterli mi |
| F2 | taslak kısmi, ama **T-ARCH-01..04 GEÇTİ** (`docs/STATUS.md:355-357`) | **hiç kalmadı** | faz raporu taslağını güncelleme (taslak hâlâ "BEKLİYOR" yazıyor, `F2-taslak.md:§4`) | "16 bot" yerine 12 bot sapmasının (`F2-taslak.md:§6`) kabulü |
| F3 | taslak kısmi | **T-ARCH-05** (K-3) | `decisions` seviyesinde 16 bot telemetri ek maliyeti + açık/kapalı tick farkı (§3 madde 4); envanter doldurma (F4-40 ile karşılandı `[V: gece-2026-10-03.md:§2]`) ve "harita izi" (F5 ile) | bu iki ölçümün F3'te mi F4/F5'te mi kapanacağı (`F3-taslak.md:§10`) |
| F4 | `KAPANDI` planlar çok, faz **ilan edilmedi**; çıkış kararı önerisi **kısmi** (`F4-taslak.md:§10`; `gece-2026-10-03.md:§2`) | **T-REGENE-01, T-PARTY-01..03, T-PERC-01** (K-2) + **T-ARCH-05** (K-3). T-ARCH-02, 06..17 GEÇTİ (`docs/STATUS.md:350`, `:373-377`) | MET-ACT-02/MET-FAIR-01 büyük örnek, 12-16 bot tick maliyeti, T-MECH-SKILL botla (mage F4-48 çalışma zamanı; rogue/archer kapsamı), algı sözleşmesi çalışma zamanı (F4-60/61), `/bot snap` çapraz doğrulama (`docs/17:301` G4) | rogue/archer skill kapsamı daraltılsın mı (profillerde rogue/archer yok); G4 için "kısmi"ye razı mısın |
| F5 | **başlamadı** (`docs/STATUS.md:582`); yalnız `BotCore` saf mantık | **T-NAV-02/09** (K-1) önce; kabul için T-NAV-04/05/09/10/11 + AC-NAV-01..07 çalışma zamanında (`docs/12:240`, `docs/17:302` G5) | F5-55 dilimleri (kart P-1..P-8) | — |
| F6+ | plan yok | G6a: T-IGT-WAR-01 (P-9); Q-25 (K-2) | — | — |

**Taslak dosyaları eskimiş** (kabul öncesi güncellenmeli): `F1-taslak.md:§4` T-ENV-ARENA-01/03 ve T-MECH-DMG'yi hâlâ bekliyor gösteriyor; `F2-taslak.md:§4` T-ARCH-01..04'ü; `F4-taslak.md:§4/§10` T-ARCH-13..17'yi (üçü de `docs/STATUS.md:345-383`'te GEÇTİ kayıtlı).

---

## 7. Dikkat: tutarsızlıklar ve doğrulayamadıklarım

- **Plan numaraları F5-59..F5-66, F6-06:** repoda yok (`git grep` her iki dalda). Kartlar dilim tanımına göre yazıldı.
- **GM hesabı, `testing`/`testmage` hesap adları, şifreler:** repoda yok; uydurulmadı.
- **T-NAV-02 ve T-NAV-09 komut/protokolü, göl ve eğim koordinatları:** yok; kartlar "Claude yazar" olarak bırakıldı.
- **T-MECH-BUF-01..08** her birinin tam tanımı ayrı listelenmiyor (`docs/05:57` 01..07, `docs/15:119` 01..08).
- **Q-27/Q-28 numarası:** `f4daa27` (2026-10-03) quest sorusunu Q-28'e taşıdı, Q-27 yalnızca `ObsTable` (F4-54). `main`'deki durum özeti hâlâ eski numarayı ("Q-27 quest") kullanıyor; kartlar yeni numarayı kullandı.
- **`db/003`:** quest betiği. 16/20 karakter için `db/005`, `db/006` (`f4daa27` commit mesajı); içerikleri okunmadı.
- **Denetleyici betiği** ve yeniden başlatma komutu repoda yok.
- **`Scripts/cancel-test.txt`, `arena-npcs.txt`, `see-walk`** STATUS'ta kullanıldığı için geçiyor; depoda `bots/config/` altında bulunmuyorlar (doğrulanamadı: yalnızca sunucu dizininde olabilirler).
- **Süreler:** doc'ta yalnızca senaryo süreleri/tekrarları var; insan toplam süresi hiçbir testte belirtilmemiş.
- **Bu koşuda hiçbir sunucu/derleme/test çalıştırılmadı**; "Claude'un yapacağı doğrulamalar" (§3) tarif düzeyindedir.
