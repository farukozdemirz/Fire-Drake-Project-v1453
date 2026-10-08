# D — 1534 protokol farkı envanteri (OURS / ALPHA / KOD + 1534 istemci ikilisi)

Tarih: 2026-10-08. Salt-okunur analiz. İndirilen hiçbir dosya çalıştırılmadı. İstemci ikilileri yalnızca `objdump` ile statik olarak okundu. Python yalnız `python3 -I` ile ve scratchpad'deki betiklerle çalıştırıldı.

## 0. Özet

1. **İstemci ikilisi belirleyici kanıt oldu.** Hedef istemci `KnightOnLine.exe` ve bizim 1453 istemcimiz statik olarak çözüldü (bkz. §1.2). Her opcode için iki istemcinin paket okuma dizisi karşılaştırıldı. Bu yüzden üçlü kural yerine çoğu satırda **[C]** (istemci) kanıtı var.
2. **KOD, 1534 istemcisi için güvenilir bir oy değil.** Paket düzenlerinde KOD = OURS (1453). Ancak hedef istemci, KOD'un `WIZ_MYINFO`'sunu (`m_bCity`, u16 ağırlık, 44 eşya) doğru okuyamaz [C]. KOD başka bir "1534" yapısını hedeflemiş olmalı. Üçlü kuralın "KOD = OURS ise belirsiz" hükmü, istemci izlenen her satırda [C] ile geçersiz kılındı.
3. **ALPHA, istemcinin büyük bölümüne uyuyor ama birebir değil.** Uyan noktalar [C]:
   - UserInfo
   - NpcInfo (ad yok)
   - REGIONCHANGE (3 parça)
   - MyInfo başlığı
   - pelerin RGB
   - ITEM_REPAIR, WARP_LIST, CAPE, RANK

   Uymayan noktalar [C]:
   - MyInfo eşya listesi: istemci 72 kayıt okur, ALPHA 74 gönderir. Sihirli çanta 2 kayıt kayar ve kuyruk alanları bozulur.
   - LEVEL_CHANGE ve POINT_CHANGE ağırlıkları: ALPHA u16'ya kırpıyor, istemci u32 okuyor.
   - QUEST 9/1 sayaçları: istemci u16 okuyor, ALPHA u8 gönderiyor.
4. **Envanter modeli:** OURS'un 73 yuvalı modeli (COSP_MAX 5, BAG1 47, BAG2 48, MBAG 49..72) 1534 istemcisiyle uyumlu [C].
   - ALPHA'nın 8 cospre / CFAIRY / 74 yuva modeli daha yeni sürümlerden alınmış; bu istemcide yok.
   - KOD da OURS sabitlerini kullanıyor.
5. **U1-01 analiz sırasında `main`'e birleşti** (`cb50146c`, şimdiki `main` = `d79aeec5`). Şunlar artık çalışma zamanı profiliyle çözülüyor:
   - kripto anahtarı
   - `WIZ_VERSION_CHECK` sürüm değeri
   - LS sürüm yanıtı
   - `LS_SERVERLIST` echo alanı

   Login sunucusunda başka düzen farkı yok [C].
6. **Zorunlu oyun sunucusu değişiklikleri** (giriş, kasabada yürüme, oyuncu/NPC görme, dövüş):
   - `WIZ_MYINFO`
   - UserInfo (`WIZ_USER_INOUT`, `WIZ_REQ_USERIN`)
   - NpcInfo (`WIZ_NPC_INOUT`, `WIZ_REQ_NPCIN`)
   - `WIZ_REGIONCHANGE`
   - ağırlık alanlarının u32 olması (`LEVEL_CHANGE`, `POINT_CHANGE`, `ITEM_MOVE` yanıtı, `WEIGHT_CHANGE`, `CLASS_CHANGE`/ALL_POINT)

   Hareket, saldırı, büyü, HP/MP, durum, ölüm/diriliş, parti ve sohbet paketleri iki istemcide aynı okunuyor [C].

Etiketler:
- **[C]** iki istemci ikilisinin statik çözümü (bu oturum)
- **[O]/[A]/[K]** kaynak kod (OURS/ALPHA/KOD)
- **[DB]** yerel DB SELECT (yalnız `K_NPC`, `K_MONSTER`)
- **[I]** çıkarım

Hüküm sözlüğü:

| Hüküm | Anlamı |
|---|---|
| GÜÇLÜ | KOD = ALPHA ≠ OURS ve [C] uyumlu |
| GÜÇLÜ-C | [C] OURS'u çürütüyor; ALPHA uyumlu (KOD = OURS olsa da) |
| C-YENİ | Ne ALPHA ne KOD istemciye uyuyor; düzen yalnız [C]'den |
| AYNI | Üç sunucu aynı ve iki istemci aynı okuyor |
| BELİRSİZ | [C] yok ya da yetersiz |
| ALPHA-ÖZGÜ | İstemci işlemiyor; gerek yok |

## 1. Kaynaklar ve yöntem

### 1.1 Kaynak ağaçları

- **OURS** `/mnt/c/dev/fdp-merge-final`, `main` @ `d79aeec5`. Sunucu oluşturucuları UP 1453 ile aynıdır; yalnız kanca farkları var. Satır numaraları CRLF kaldırılarak sayıldı.
- **Bot kodu:** dal `gece/2026-10-08-kalabalik` @ `2a587033`. `git show` ile okundu, çalışma ağacına dokunulmadı. Bot satır numaraları bu dala aittir.
- **ALPHA** `…/ALPHA KO 1534 PROJE/1-Game Source` (`__VERSION 1534`).
- **KOD** `scratchpad/kod1534_full/KODevelopers-1534-master` (`__VERSION 1534`). `#if __VERSION` bekçileri 1534 için değerlendirildi; KOD'da etkin olan tek fark LS echo'dur.
- Önceki analizler yeniden kullanıldı: `docs/reports/u0-1534/A-kaynak-kod-karsilastirmasi.md` §3 ve Ek B, `scratchpad/k1534/b/packets.md`.

### 1.2 İstemci kanıtı (statik)

| | Yeni istemci (1534) | Bizim istemci (1453) |
|---|---|---|
| Dosya | `/mnt/c/dev/fdp1534/client/Knight Online/KnightOnLine.exe` (`.nero`) | `/mnt/c/dev/fdp/Client/KnightOnLine.exe` (`.text`) |
| Ana paket anahtarı | `0x6f2d23`: önce taban çağrı, sonra `opcode-6 ≤ 0x8a`, tablo `0x6f4b18`/`0x6f4980` | `0x6a0e0e`: `opcode-6 ≤ 0x7b`, tablo `0x6a2560` |
| Taban anahtar (`CGameProcedure`) | `0x7c02b0` (vtable +0xC/+0x10/+0x14 = VersionCheck/Login/SelChar) | `0x764d70` |
| Login prosedürü | `0x737e93` (0x42..0xF6) | `0x6e3d03` |
| Karakter seçimi | `0x73f858` | `0x6eb848` |
| Socket Send | `0x470000` | `0x4633d0` |

Okuyucu desenleri satır içidir ve sınır denetimlidir: `off += N`, `off > size` ise `0`, değilse `[buf+off-N]`. Paket dışına okuma çökme yaratmaz, `0` döner. Yardımcılar: u16 `0x675250`, u8 `0x558830`, u32 `0x60cdd0`, dize `0x4701d0` (yeni); dize `0x4635b0` (bizim).

Betikler `scratchpad/k1534/d/`:

| Betik | Ne yapar |
|---|---|
| `sw.py` | anahtar tablosu çözücü |
| `cases.py`, `tr3.py` | özyinelemeli paket okuma izleyicisi |
| `final_cases.txt` | her opcode için iki istemcinin izi |
| `swcmp.py`, `ks_cmp.txt` | `KNIGHTS_*` alt kodları |
| `sends.py`, `sends_*.txt` | sabit düzenli istemci→sunucu oluşturucuları (yeni 336, bizim 297 çağrı) |
| `fn3.py`, `batch.sh`, `rd.py` | 3 ağaçlı fonksiyon ve paket-okuma farkı |

Sınırlar:
- İzleyici okuma *dizisini* karşılaştırır. Kontrol akışı farkını kaçırabilir: NpcInfo'daki ad ilk izde "AYNI" görünmüştü; elle düzeltildi. Kritik satırların akışı elle doğrulandı (§2).
- Değişken uzunluklu istemci→sunucu oluşturucuları çıkarılamadı (login, sohbet, hareket, saldırı, büyü). Bunlar için kanıt ALPHA = OURS ayrıştırıcısıdır; ALPHA bu istemciyle birlikte dağıtıldı.

## 2. Ana tablo

Yön: S→C sunucudan istemciye, C→S istemciden sunucuya. "Bayt" sütunu 1534'ün OURS'a göre farkıdır.

### 2.1 Login sunucusu

| Paket | Yön | OURS | ALPHA | KOD | 1534 istemci [C] | Hüküm | file:line O / A / K | Bayt |
|---|---|---|---|---|---|---|---|---|
| LS_VERSION_REQ 0x01 | S→C | `LoginVersionReply(cfg, DB)` (U1-01) | DB/`__VERSION` | DB/`__VERSION` | exe işlemez (Launcher işi) | AYNI düzen, değer U1-01 | `LoginSession.cpp:33-38` / `:32-37` / `:35-40` | 0 |
| LS_DOWNLOADINFO_REQ 0x02 | iki yön | aynı | aynı | aynı | exe işlemez | AYNI | `LoginSession.cpp:40-61` / `:39-60` / `:42-63` | 0 |
| LS_CRYPTION 0xF2 | S→C | u64; özel anahtar U1-01 ile ini'den | `0x1257091582190465` | aynı | aynı anahtar (`0x7c0f7d` push) | GÜÇLÜ (değer), **yapıldı (U1-01)** | `shared/JvCryption.cpp:7` / `:6-12` / `:5` | 0 |
| LS_LOGIN_REQ 0xF3 | C→S | str hesap, str şifre | +ltrim/rtrim | +IP DB'ye | oluşturucu değişken; yanıt ayrıştırması aynı | AYNI | `LoginSession.cpp:63-143` / `:62-147` / `:65-147` | 0 |
| LS_SERVERLIST 0xF5 | C→S | U1-01: 1534'te u16 echo okunur | her zaman echo | `>=1500` → echo | **u16 echo gönderir** (sabit oluşturucu uzunluk 3; bizimki 1) | GÜÇLÜ, **yapıldı (U1-01)** | `LoginSession.cpp:163-177` / `:166-176` / `:165-177` | +2 |
| LS_SERVERLIST 0xF5 | S→C | U1-01: echo + kayıtlar; bilinmeyen u8 = 1 | echo, u8 0 | echo, u8 1 | echo okur; bilinmeyen u8'i **atlar** (`0x7367a7 inc eax`) | GÜÇLÜ (echo); u8 değeri önemsiz | `LoginServer.cpp:79-116` / `:96-130` / `:78-117` | +2 |
| LS_NEWS 0xF6 | S→C | str başlık, str içerik | yalnız içerik kutu biçimi farklı | = OURS | `{u16 STR u16 STR}` iki istemcide aynı | AYNI (kozmetik) | `LoginSession.cpp:179-194`, `LoginServer.cpp:198-235` / `:178-193`, `:195-235` / `:179-194`, `:182-220` | 0 |
| LS_UNKF7 0xF7 | S→C | u16 0 | aynı | aynı | exe işlemez | AYNI | `LoginSession.cpp:204-209` / `:203-208` / `:204-209` | 0 |

### 2.2 El sıkışma ve karakter seçimi

| Paket | Yön | OURS | ALPHA | KOD | 1534 istemci [C] | Hüküm | file:line O / A / K | Bayt |
|---|---|---|---|---|---|---|---|---|
| WIZ_VERSION_CHECK 0x2B | C→S | yükü okumaz | okumaz | okumaz | **yalnız opcode** (uzunluk 1; 1453 istemci +u16) | AYNI (sunucu zaten okumuyor) | `LoginHandler.cpp:4-20` / `:3-12` / `:3-19` | −2 C→S, zararsız |
| WIZ_VERSION_CHECK 0x2B | S→C | u16 sürüm (U1-01 profil), u64 anahtar | u16 1534, u64 | aynı | `cmp ecx,0x5fe` (1534) @`0x7c0fc4`; okuma u16, u64, (+u8 → 0) | **yapıldı (U1-01)** | `LoginHandler.cpp:15` | 0 |
| WIZ_LOGIN 0x01 | iki yön | str hesap / u8 | aynı (WordGuard kaldırılmış) | aynı (WordGuard yorumda) | yanıt u8, iki istemcide aynı | AYNI (davranış: A ve K WordGuard'ı kaldırmış) | `LoginHandler.cpp:22-56` / `:14-44` / `:21-55` | 0 |
| WIZ_ALLCHAR_INFO_REQ 0x0C | S→C | u8 1, 3×[str16 ad, u8 ırk, u16 sınıf, u8 sv, u8 yüz, u8 saç, u8 bölge, 8×(u32, u16)] | aynı (DB'den `strItemTime` de çekiyor) | aynı | aynı (`0x73f85f`) | AYNI | `DatabaseThread.cpp:184-199`, `DBAgent.cpp:142-187` / `:199-214`, `:141-189` / `:184-199`, `:141-186` | 0 |
| WIZ_SEL_NATION 0x05 | iki yön | u8 / u8 | aynı | aynı | aynı (uzunluk 2) | AYNI | `CharacterSelectionHandler.cpp:4-17` | 0 |
| WIZ_NEW_CHAR 0x02 | iki yön | u8 saç / u8 | aynı (+ırk 0 → hata) | aynı | yanıt u8 aynı | AYNI | `:46-78` / `:45-79` / `:48-80`; `DatabaseThread.cpp:213-225` | 0 |
| WIZ_DEL_CHAR 0x03 | iki yön | aynı | aynı | aynı | C→S aynı; S→C 1534 `{u16 u8}`, 1453 izi boş | AYNI (sunucu) | `:80-100`; `DatabaseThread.cpp:227-245` | 0 |
| WIZ_SEL_CHAR 0x04 | iki yön | u8, u8 bölge, u16 x, z, y, u8 zafer | aynı | aynı | taban işleyici aynı (`0x7c12c0` ↔ `0x765f00`) | AYNI | `CharacterSelectionHandler.cpp:134-225` / `:135-229` / `:135-237` | 0 |
| WIZ_CHANGE_HAIR 0x89 | C→S | u8 saç | **u32 saç** | u8 | oluşturucu bulunamadı | BELİRSİZ (ALPHA-özgü) | `:25-44` / `:25-43` / `:27-46` | ? |
| WIZ_GAMESTART 0x0D | iki yön | u8 1 veya 2 / boş | aynı; faz 1'de ek PREMIUM ve TopSendNotice | aynı | aynı | AYNI | `:259-337` / `:263-331` / `:271-354` | 0 |

### 2.3 Giriş paketleri

| Paket | Yön | OURS | ALPHA | KOD | 1534 istemci [C] | Hüküm | file:line O / A / K | Bayt |
|---|---|---|---|---|---|---|---|---|
| **WIZ_MYINFO 0x0E** | S→C | `m_bCity` + 11 B klan + u16 ağırlık + 14+2+28 eşya (2 cospre 15 B) | `m_bCity` yok, klan+RGB, u8 2,3,4,5, u32 ağırlık, **74** × 19 B | = OURS | §3.1: klan+RGB, 4 bilinmeyen bayt, u32 ağırlık, **72** × 19 B (14, 28, cospre 43..46, çanta 47..48, sihirli çanta 49..72) | **C-YENİ** (başlık = ALPHA, eşyalar = OURS modeli) | `User.cpp:947-1084` (`m_bCity` 988, klan 990-1016, ağırlık 1021, eşya 1036-1068, kuyruk 1071-1075) / `User.cpp:1047-1186` / `User.cpp:965-1116` | +551 |
| WIZ_SKILLDATA 0x79 | S→C | u8 2, u16 n, u32 × n | aynı | aynı | aynı | AYNI. **OURS hatası:** `ReqSkillDataLoad` yalnız yükleme başarısız olunca gönderiyor (yorumdaki satır `Send`'i if gövdesi yapıyor). ALPHA düzeltmiş; 1534'e özgü değil. | `DatabaseThread.cpp:292-299` / `:299-306` / `:339-346` | 0 |
| WIZ_QUEST 0x64 alt 1 (liste) | S→C | u16 n, (u16 kimlik, u8 durum) × n | aynı | aynı | aynı | AYNI | `QuestHandler.cpp:4-12` / `:5-27` / `:4-12` | 0 |
| **WIZ_QUEST 0x64 alt 9** (öldürme sayacı) | S→C | 9/1: u16 idx, u8 × 4; 9/2: u8 grup, u8 sayı | 9/1: u16 görev, **u8** × 4; 9/2: u16 görev, u8 grup, sayı | = OURS | 9/1: u16 görev, **u16** × 4; 9/2: u16 görev, u8 grup, **u16** sayı (`0x71f49d`-) | C-YENİ (OURS'taki yorumlu "18xx" biçimi) | `QuestHandler.cpp:151-187`, `:210-232` / `:157-208`, `:228-243` / `:168-204`, `:227-249` | 9/1 +4, 9/2 +3 |
| WIZ_NOTICE 0x2E | S→C | aynı | +flaş girdileri (içerik) | aynı | aynı | AYNI | `User.cpp:2961-2978` | 0 |
| WIZ_TIME 0x13 / WIZ_WEATHER 0x14 | S→C | aynı | aynı | aynı | aynı | AYNI | `User.cpp:1172-1188` | 0 |
| WIZ_ZONEABILITY 0x5E | S→C | u8 1, u8, u8, u8, u16 | mantık farkı | aynı | aynı | AYNI (OURS kalsın) | `User.cpp:1195-1247` / `:1341-1412` | 0 |
| WIZ_PREMIUM 0x71 | S→C | u8 durum, u8 tür, u32 süre | aynı (GameStart'ta da gönderir) | aynı | durum 1 için tam bu (1453 fazladan u32 okuyordu) | AYNI | `User.cpp:1252-1257` / `:1417-1422` / `:1284-1289` | 0 |
| WIZ_LOYALTY_CHANGE 0x2A | S→C | alt 1: u32 × 4 | aynı | aynı | alt 1'de 4 u32 okur (1453: 3) | AYNI | `User.cpp:650-776` | 0 |
| WIZ_SERVER_INDEX 0x6B | S→C | aynı | aynı | 5 satır farklı | aynı | AYNI | `User.cpp:859-864` | 0 |

### 2.4 Görünürlük (oyuncu ve NPC görme)

| Paket | Yön | OURS | ALPHA | KOD | 1534 istemci [C] | Hüküm | file:line O / A / K | Bayt |
|---|---|---|---|---|---|---|---|---|
| WIZ_USER_INOUT 0x07 çerçevesi | S→C | u16 tür, u16 kimlik, UserInfo | aynı | aynı | aynı | AYNI | `CharacterMovementHandler.cpp:63-69` | 0 |
| **UserInfo kaydı** (0x07, 0x16) | S→C | §3.2 | +5 klan, 12 eşya | = OURS (yalnız ittifak pelerin kimliği) | +u32 RGB, +u8 bayrak; 8 + 4 cospre (parça dizini 0, 2, 3, 4) (`0x6fc221`/`0x6fc328`) | **GÜÇLÜ-C** | `CharacterMovementHandler.cpp:99-161` (klansız 111, pelerin 119, kask baytı 139, eşyalar 145-158) / `:103-283` / `:154-218` | +19 / kayıt |
| WIZ_REQ_USERIN 0x16 | C→S | u16 n, u16 kimlik × n | aynı (+olay odası filtresi) | aynı | — | AYNI | `User.cpp:1264-1287` / `:1429-1459` / `:1296-1319` | 0 |
| WIZ_REQ_USERIN 0x16 yanıtı | S→C | u16 n, (u8 0, u16 kimlik, UserInfo) × n | aynı | aynı | çerçeve aynı | AYNI (kayıt farklı) | `GameServerDlg.cpp:1382-1411` | +19 / kayıt |
| **WIZ_REGIONCHANGE 0x15** | S→C | u16 n, u16 kimlik × n | `[u8 0]`, `[u8 1, u16 n, ids]`, `[u8 2]` | = OURS | u8 alt: 0 listeyi temizle (`0x72b060`), 1 liste (n < 1000), 2 bitir (`0x6fdce0`) | **GÜÇLÜ-C** | `GameServerDlg.cpp:1362-1380` / `:1520-1544` / `:1968-1986` | +1, +2 paket |
| **NpcInfo kaydı** (0x0A, 0x1D) | S→C | adı içerir (u8 + dize) | **ad yok** | = OURS | ad **yalnız `npcType == 15` ise** (o zaman 2 dize) okunur; diğerlerinde ad istemci tablosundan gelir (`0x6fe3b3 cmp edx,0xf`; 1453 istemci adı her zaman okur `0x6ab74c`) | **GÜÇLÜ-C** | `Npc.cpp:138-155` (ad 147) / `:144-163` / `:138-155` | −(1+n) |
| Tür 15 NPC'ler | [DB] | `K_NPC` ve `K_MONSTER`'da `byType=15` sayısı **0** | | | | Adı tümden atlamak güvenli. U2-02 içe aktarımından sonra yeniden kontrol edilmeli. | | |
| WIZ_NPC_REGION 0x1C | S→C | u16 n, ids | aynı | aynı | aynı | AYNI | `GameServerDlg.cpp:1563-1580` | 0 |
| WIZ_REQ_NPCIN 0x1D | C→S | u16 n, ids | aynı (`id < NPC_BAND` reddi) | aynı | aynı | AYNI | `User.cpp:1294-1333` / `:1466-1521` | 0 |
| WIZ_REQ_NPCIN 0x1D yanıtı | S→C | u16 n, (u16 id, NpcInfo) | aynı | aynı | aynı | AYNI (kayıt farklı) | `GameServerDlg.cpp:1519-1561` | −(1+n) / NPC |
| WIZ_MERCHANT_INOUT 0x69 | S→C | aynı | | | aynı | AYNI | | 0 |

### 2.5 Hareket ve dövüş

| Paket | Yön | OURS | ALPHA | KOD | 1534 istemci [C] | Hüküm | file:line | Bayt |
|---|---|---|---|---|---|---|---|---|
| WIZ_MOVE 0x06 | iki yön | u16 kimlik, x, z, y, hız, u8 echo | aynı (`type` yalnız AI kopyasında) | aynı | aynı (`0x6fb570`) | AYNI | `CharacterMovementHandler.cpp:4-54` | 0 |
| WIZ_ROTATE 0x09 | iki yön | aynı | aynı | aynı | aynı | AYNI | | 0 |
| WIZ_NPC_MOVE 0x0B | S→C | u16 × 5 | aynı | son u16 değeri farklı | aynı | AYNI | `Npc.cpp:73-82` | 0 |
| WIZ_ATTACK 0x08 | iki yön | u8, u8, u16, u16 | aynı | aynı | aynı | AYNI | `AttackHandler.cpp` | 0 |
| WIZ_MAGIC_PROCESS 0x31 | iki yön | 1..12 alt kod | aynı | aynı | alt kod kümesi aynı; bir işleyicide fazladan `{u32 u16 u16 döngü}`; C→S alt 8 uzunluk 22 aynı | AYNI (botlar için) | `MagicProcess.cpp:16-53` | 0 |
| WIZ_TARGET_HP 0x22 | iki yön | u16, u8, u32, u32, u16 / u16, u8 | aynı | aynı | aynı | AYNI | `User.cpp:2390-2421` | 0 |
| WIZ_HP_CHANGE 0x17 / WIZ_MSP_CHANGE 0x18 | S→C | aynı | aynı | aynı | aynı | AYNI | | 0 |
| WIZ_EXP_CHANGE 0x1A | S→C | aynı | | | aynı | AYNI | | 0 |
| **WIZ_LEVEL_CHANGE 0x1B** | S→C | … u16 maxW, u16 W | `uint16()` kırpması | = OURS | **u32 maxW, u32 W** (`0x704dd0`) | C-YENİ | `User.cpp:1866-1872` / `:2046-2106` / `:1938-1999` | +4 |
| **WIZ_POINT_CHANGE 0x28** | S→C | … u16 maxW | `uint16()` | = OURS | **u32** (`0x705a80`) | C-YENİ | `User.cpp:1897-1915` / `:2113-2131` / `:2006-2024` | +2 |
| WIZ_STATE_CHANGE 0x29 | iki yön | u16, u8, u32 | aynı (+tür 11) | aynı | aynı | AYNI | `User.cpp:2744-2802` | 0 |
| WIZ_DEAD 0x11 / WIZ_REGENE 0x12 | iki yön | aynı | aynı | aynı | aynı | AYNI | | 0 |
| WIZ_ZONE_CHANGE 0x27 | S→C | aynı | mantık farkı | | aynı | AYNI | `CharacterMovementHandler.cpp:309-500` | 0 |
| WIZ_WARP 0x1E | S→C | u16 x, u16 z | aynı | aynı | **isteğe bağlı 3. u16 y** okur; 0 → arazi yüksekliği (`0x6f2e8b`) | AYNI (uyumlu); +y isteğe bağlı | `CharacterMovementHandler.cpp:626-659` | 0 / +2 |
| WIZ_PARTY 0x2F | iki yön | aynı | aynı | aynı | S→C aynı; C→S yeni alt kodlar 0x0c, 0x0e, 0x10, 0x12, 0x1b | AYNI (yeni alt kodlar isteğe bağlı) | `PartyHandler.cpp` | 0 |
| WIZ_CHAT 0x10 | iki yön | aynı | aynı | aynı | aynı (`0x6fa6f0`) | AYNI | `ChatHandler.cpp` | 0 |
| WIZ_CHAT_TARGET 0x35 | C→S | u8, str, u16 len, msg | SByte (u8 len) mesaj | = OURS | yalnız akış farklı | BELİRSİZ | `ChatHandler.cpp:281-320` / `:296-340` | ? |
| WIZ_SPEEDHACK_CHECK 0x41 | C→S | aynı | aynı | aynı | aynı (uzunluk 6) | AYNI | | 0 |

### 2.6 Eşya ve envanter

| Paket | Yön | OURS | ALPHA | KOD | 1534 istemci [C] | Hüküm | file:line O / A / K | Bayt |
|---|---|---|---|---|---|---|---|---|
| WIZ_ITEM_MOVE 0x1F | C→S | u8 yön, u32, u8 kaynak, u8 hedef | aynı; cospre indeksi **−1 kaymalı** | aynı | aynı düzen; yeni yönler 9..0x0d (OURS 9..11 işliyor; 12/13 yeni) | AYNI düzen. Yuva indeksi: OURS (`pos = slot − 42`) muhtemelen doğru, ALPHA'nın −1'i kendi kaymış depolamasına göre [I] | `ItemHandler.cpp:541-743` / `:593-795` / `:603-815` | 0 |
| **WIZ_ITEM_MOVE yanıtı** | S→C | … u16 maxW | `m_sMaxWeight` (u32) | = OURS | **u32** (`0x701140`) | GÜÇLÜ-C | `User.cpp:3366-3391` (3378) / `:3849-3873` / `:3565-3590` | +2 |
| **WIZ_WEIGHT_CHANGE 0x54** | S→C | u16 | u32 (üye tipi) | u16 | **u32** (`0x7015c0`) | GÜÇLÜ-C | `ItemHandler.cpp:520-525` / `:512-517` / `:582-587` | +2 |
| **WIZ_CLASS_CHANGE 0x34** ALL_POINT yanıtı | S→C | … u16 maxW | u32 | u16 | **u32** (`0x714510`) | GÜÇLÜ-C | `User.cpp:3976-4226` (4216-4219) / `:4530-4780` / `:4213-4472` | +2 |
| WIZ_ITEM_COUNT_CHANGE 0x3D | S→C | aynı | | farklı ama eşdeğer | aynı | AYNI | `ItemHandler.cpp:1207-1226` | 0 |
| WIZ_ITEM_GET 0x26 | iki yön | aynı | aynı | aynı | aynı | AYNI | `User.cpp:2464-2681` | 0 |
| WIZ_BUNDLE_OPEN_REQ 0x24 | iki yön | 6 × (u32, u16) | aynı (`>=1950` kapalı) | aynı | aynı | AYNI | `User.cpp:2428-2457` | 0 |
| WIZ_ITEM_DROP 0x23 | S→C | | | | okumalar aynı, akış farklı | BELİRSİZ (düşük öncelik) | | ? |
| **WIZ_ITEM_REPAIR 0x3B** | C→S | u8, u8, `/*u16 npc*/`, u32 | **u16 npc okur** | = OURS | uzunluk 9: u8, u8, u8, **u16**, u32 (1453: 7) | GÜÇLÜ-C | `NPCHandler.cpp:8-70` / `:7-79` / `:8-70` | +2 C→S |
| WIZ_ITEM_TRADE, WIZ_WAREHOUSE, WIZ_EXCHANGE, WIZ_MERCHANT, WIZ_SHOPPING_MALL, WIZ_RENTAL | iki yön | | | | aynı | AYNI (ALPHA'nın VIP depo ve Menisia eklentileri gereksiz) | | 0 |

### 2.7 Klan, pelerin ve PK sıralaması

`WIZ_KNIGHTS_PROCESS 0x3C` alt kodları:

| Alt kod | Yön | OURS | ALPHA | KOD | 1534 istemci [C] | Hüküm | file:line O / A / K | Bayt |
|---|---|---|---|---|---|---|---|---|
| JOIN 0x02 (bölgeye kayıt) | S→C | … pelerin, mark | +R, G, B, 0 | = OURS | +u32 | GÜÇLÜ-C | `KnightsManager.cpp:606-644` / `:634-671` / `:602-640` | +4 |
| 0x1C..0x22, UPDATE 0x24 (ortak işleyici) | S→C | UPDATE: id, bayrak, pelerin | +RGB (ittifak kuralları) | = OURS | klan girdisi başına +u32 | GÜÇLÜ-C (UPDATE); ALLY_* kayıtları için oluşturucu bazlı kontrol gerekli | `Knights.cpp:321-328` / `:456-491` / `:324-331`; ALLY: `DatabaseThread.cpp:548,576`, `KnightsManager.cpp:1015,1041,1261` | +4 / klan |
| 0x01, 0x03, 0x04, 0x05, 0x0A, 0x0C (liste), 0x0D (üye listesi), 0x10, 0x11, 0x13, 0x16..0x1B, 0x1D, 0x23, 0x27, 0x28 | S→C | | | | aynı | AYNI | | 0 |
| 0x3B..0x41, 0x4F, 0x51 (puan, bağış, devir, top10) | iki yön | OURS'ta 0x51 yok | | | yeni istemci işliyor ve gönderiyor | isteğe bağlı | | — |

Diğer paketler:

| Paket | Yön | OURS | ALPHA | KOD | 1534 istemci [C] | Hüküm | file:line O / A / K | Bayt |
|---|---|---|---|---|---|---|---|---|
| WIZ_KNIGHTS_LIST 0x3E | S→C | | | | aynı | AYNI | | 0 |
| **WIZ_CAPE 0x70** | C→S | u16 pelerin (`/*r,g,b*/`) | u16, r, g, b | = OURS | uzunluk 7: u16, **u32** (r, g, b, pad) | GÜÇLÜ-C | `NPCHandler.cpp:806-943` (816) / `:1041-1209` / `:845-982` | +4 C→S |
| WIZ_CAPE 0x70 | S→C | u16 1, ittifak, klan, pelerin | +R, G, B, 0 | = OURS | +u32 | GÜÇLÜ-C | `NPCHandler.cpp:920-925` / `:1171-1185` | +4 |
| **WIZ_RANK 0x80** | C→S | u8 tür bekler | tür = 0 kabul | = OURS | **yalnız opcode** (uzunluk 1) | GÜÇLÜ-C | `User.cpp:5355-5505` / `:6217-6369` / `:5640-5790` | −1 |
| WIZ_RANK 0x80 | S→C | `WIZ_RANK, tür` + girdi başı u32 (+PK'da premium u32) | tür baytı yok; tür 0'da premium yok | = OURS | tür baytı yok; girdi başı tek u32; kuyruk u16 + u32 (`0x5e7f00`) | GÜÇLÜ-C (Ronark PK sıralama arayüzü) | | −1, −4/girdi |
| **WIZ_WARP_LIST 0x4B** | C→S | u16 warp (`/*npcid*/`) | u16 npc, u16 warp | = OURS | uzunluk 5: u16, u16 | GÜÇLÜ-C | `User.cpp:4303-4346` (4309) / `:4865-4908` / `:4549-4592` | +2 C→S |
| WIZ_LOGOSSHOUT 0x7D | C→S | u8, str | SByte, u8, R, G, B, C, str | = OURS | S→C aynı; C→S bulunamadı | BELİRSİZ | `User.cpp:6073-6083` / `:7597-7634` | ? |
| WIZ_HELMET | C→S | u8 | 2 bool | u8 | iki istemcinin alıcı anahtarında yok | BELİRSİZ | `User.cpp:5063-5079` / `:5926-5939` | ? |
| WIZ_MAP_EVENT 0x53, WIZ_CONCURRENTUSER 0x36, WIZ_CORPSE 0x4E | S→C | | | | bir dalda fazladan alan | BELİRSİZ, düşük | | ? |
| WIZ_KING, WIZ_FRIEND_PROCESS, WIZ_SELECT_MSG, WIZ_EDIT_BOX, WIZ_OBJECT_EVENT, WIZ_EVENT, WIZ_STEALTH, WIZ_ROOM, WIZ_NAME_CHANGE, WIZ_SIEGE, WIZ_CHALLENGE | S→C | | | | aynı | AYNI | | 0 |

### 2.8 Opcode kümesi farkları

- **1534 istemcinin yeni işlediği opcode'lar** [C]:
  - **0x19**: sohbet odası. Alır ve alt 1/2/3/5/9 gönderir. ALPHA `ChatRoomHandle.cpp` `WIZ_NATION_CHAT 0x19` kullanıyor ama derlenmiyor. KOD'da yok.
  - **0x57 WIZ_BATTLE_EVENT**: OURS zaten gönderiyor (`AISocket.cpp:562`, `GameServerDlg.cpp:1996`), ancak düzeni hiç doğrulanmadı.
  - **0x80 WIZ_RANK**: §2.7.
  - **0x90 WIZ_DEATH_LIST**: `{u8 u32 u16}`. Hiçbir sunucu göndermiyor; ALPHA tanımı bile silmiş.
- **ALPHA'nın işleyip istemcinin işlemediği opcode'lar:** `WIZ_MOVING_TOWER 0x84`, `WIZ_CAPTURE 0x85`, `WIZ_VIPWAREHOUSE 0x8B`, `WIZ_GENIE 0x97`, `0x98`, `WIZ_ACHIEVE 0x99`, `WIZ_EXP_SEAL 0x9A`, `WIZ_SP_CHANGE 0x9B`, `WIZ_HACKSHIELD_GUARD 0xA1`.
  - Ana anahtar 0x06..0x90 aralığında; `default` → `false` (`0x6f4978`). Taban anahtarda yalnız 0x01, 0x04, 0x2B, 0x46, 0x72, 0xFE var [C].
  - KOD'da da yok.
  - → **ALPHA-ÖZGÜ, alınmayacak.**

## 3. Bayt düzenleri

### 3.1 WIZ_MYINFO — 1534 (istemciden, `0x6f7520`)

```
u16 sid, str8 name, u16 x, u16 z, u16 y, u8 nation, u8 race, u16 class, u8 face, u8 hair,
u8 rank, u8 title, u8 level, u16 points, u32 maxExp, u32 exp, u32 loyalty, u32 loyaltyMonthly,
u16 clanId, u8 fame,
  u16 allianceId, u8 flag, str8 clanName, u8 grade, u8 ranking, u16 markVer, u16 capeId,
  u8 R, u8 G, u8 B, u8 0                  (no clan: u64 0, u16 0xFFFF, u32 0)
u8 2, u8 3, u8 4, u8 5                    (read as one u32; ALPHA's "unknown")
u16 maxHp, u16 hp, u16 maxMp, u16 mp, u32 maxWeight, u32 itemWeight,
5 x (u8 stat, u8 bonus), u16 hit, u16 ac, 6 x u8 resist, u32 gold, u8 authority, u8 knightsRank, u8 personalRank,
9 B skill bar,
item19 x 14  (slots 0..13)
item19 x 28  (14..41)
item19 x 4   (client cospre index 1..4 = slots 43 CHELMET, 44 CLEFT, 45 CRIGHT, 46 CTOP; index 0 = 42 CWING is NOT read)
item19 x 2   (47 BAG1, 48 BAG2)
item19 x 24  (49..72 magic bags)
u8 accountStatus, u8 premiumType, u16 premiumTime, u8 chicken, u32 manner
item19 = u32 id, u16 dur, u16 count, u8 flag, u16 rentalTime, u32 sealSerial (!=0 -> seal block {u16 str u8 u8 u16 u16}), u32 expiry
```

Kanıtlar:
- 1453 istemcinin cospre döngüsü yalnız indeks 1 ve 4'ü okur (`0x6a61c8`). Bu, OURS'taki `for i=43; i<=46; i+=3` ile birebir uyar; demek ki dizin = yuva − 42.
- 1534 döngüsü 1, 2, 3 ve 4'ü okur (`0x6f8cdc`), ardından `esi=2` (çantalar) ve `esi=24` (sihirli çantalar) döngüleri gelir.
- OURS'un 1453 düzeni: 14 × 19 + 2 × 15 + 28 × 19.
- ALPHA: 74 × 19 B, yuva sırası 42..49 (CFAIRY dahil). Sonuç: 2 kayıt fazla, sihirli çantalar ve kuyruk 38 bayt kayar.

### 3.2 UserInfo kaydı — 1534 (istemciden, `0x6fbb50`)

```
str8 name, u8 nation, u16 clanId, u8 fame,
u16 alliance, str8 clanName, u8 grade, u8 ranking, u16 markVer, u16 capeId, u8 R, u8 G, u8 B, u8 0, u8 capeFlag(=2)
  (no clan: u16 0, u8 0, u32 0, u16 0xFFFF, u16 0, u8 0, u16 0  = 14 B)
u8 level, u8 race, u16 class, u16 x, u16 z, u16 y, u8 face, u8 hair, u8 resHpType, u32 abnormal,
u8 needParty, u8 authority, u8 partyLeader, u8 invis, u8 (helmet-hidden | team colour), i16 dir,
u8 chicken, u8 rank, i8 knightsRank, i8 personalRank,
(u32, i16, u8) x 8  : BREAST, LEG, HEAD, GLOVE, FOOT, SHOULDER, RIGHTHAND, LEFTHAND   (client part index 0..7)
(u32, i16, u8) x 4  : client part index 0, 2, 3, 4 (1453: only 0 and 2)
u8 zone
```

Cospre sırası [C + I]:
- 1453 istemci: OURS `CTOP, CHELMET` gönderir; istemci bunları parça dizini 0 (üst) ve 2'ye (kask) koyar.
- 1534'te dizinler 0, 2, 3 ve 4 olduğundan sıra **`CTOP, CHELMET, CLEFT, CRIGHT`** olmalı.
- ALPHA kâğıt üstünde `CRIGHT, CWING, CHELMET, CLEFT` gönderir. Ama `ItemMove` içinde cospre'yi `INVENTORY_COSP + pos − 1` ile sakladığı için yuva 45 üstü, 42 kaskı, 43 ve 44 eldivenleri tutar. Yani ALPHA dolaylı olarak aynı görsel sırayı üretir.
- **ALPHA'nın yuva listesini harfiyen kopyalamayın.**
- Kask/takım baytı yalnız değer farkıdır; anlamı belirsiz.

### 3.3 Diğerleri

- **NpcInfo (1534):** OURS'tan `str8 name` çıkarılır. `npcType == 15` ise istemci iki dize okur: önce bilinmeyen bir dize, sonra ad.
- **REGIONCHANGE (1534):** ALPHA biçimi. Üç paket gönderilir: `[u8 0]`, `[u8 1, u16 n, u16 × n]`, `[u8 2]`. Sayı offset 1'e yazılır.

## 4. Envanter sabitleri ve kablodaki yuva indeksleri

| Sabit | OURS (`shared/globals.h:195-253`) | ALPHA (`globals.h:220-286`) | KOD | 1534 istemci [C] |
|---|---|---|---|---|
| SLOT_MAX / HAVE_MAX | 14 / 28 | 14 / 28 | = OURS | 14 / 28 (MyInfo döngüleri) |
| COSP_MAX | 5 | **8** | 5 | 4 cospre + 2 çanta okunur; 42 (kanat) okunmaz → OURS modeli |
| CWING..CTOP | 42..46 | 42..46 | = | 43..46 kullanılıyor |
| CFAIRY | — | 47 | — | **yok** (borrowed) |
| BAG1 / BAG2 | 47 / 48 | 48 / 49 (`COSP_BAG1/2` = 5/6 ile tutarsız) | 47 / 48 | iki çanta kaydı, cospre'den hemen sonra |
| MBAG_COUNT × MBAG_MAX | 2 × 12 | 2 × 12 | = | 24 |
| INVENTORY_MBAG / TOTAL | 49 / 73 | 50 / 74 | 49 / 73 | 72 kayıt = 73 − kanat |
| WAREHOUSE_MAX | 192 | 192 (+VIP 48) | 192 | WIZ_WAREHOUSE ayrıştırması 1453 ile aynı |

`WIZ_ITEM_MOVE` C→S düzeni iki istemcide aynı (u8, u32, u8, u8).
- Cospre için OURS `pos = slot − 42`, ALPHA `pos − 1` kullanıyor.
- MyInfo dizin anlamı aynı kaldığı için OURS'un doğru olması beklenir [I].
- Doğrulama: insan istemciyle bir cospre ve sihirli çanta taşıma denemesi gerekli.
- Bot kodu yalnız 14..41 aralığını kullanıyor; etkilenmez.

## 5. Zorunlu liste (giriş sırasına göre)

Amaç: giriş + kasabada yürüme + oyuncu/NPC görme + dövüş.

1. Launcher → LS_VERSION_REQ değeri. Yapıldı (U1-01). [C: exe işlemiyor; Launcher ayrı]
2. LS_CRYPTION anahtarı. Yapıldı (U1-01). [C]
3. LS_LOGIN_REQ. Değişiklik yok. [C yanıt]
4. LS_SERVERLIST echo. Yapıldı (U1-01). [C]
5. GS WIZ_VERSION_CHECK, 1534 değeri ve anahtar. Yapıldı (U1-01). [C]
6. WIZ_LOGIN, ALLCHAR_INFO, SEL_NATION, NEW_CHAR, SEL_CHAR. Değişiklik yok. [C, A]
7. **WIZ_GAMESTART(1) → WIZ_MYINFO: U1-02, ZORUNLU.** [C, C-YENİ] Olmazsa konum, istatistik ve envanter yanlış okunur. Oyuncu yanlış yerde ya da 0 HP ile görünebilir.
8. GameStart(1) içinde sırayla:
   - UserInOutForMe: **REGIONCHANGE (U1-05)** ve **REQ_USERIN UserInfo (U1-04)** → ZORUNLU. [C]
   - NpcInOutForMe: **NpcInfo (U1-05)** → ZORUNLU. [C] Olmazsa her NPC kaydında 1+n bayt kayma olur; NPC'ler yanlış konumlarda, yanlış kimliklerle görünür.
   - Merchant, NOTICE, TIME, WEATHER, ZONEABILITY, SKILLDATA, PREMIUM, QUEST/1: değişiklik yok. [C]
9. Kasabada yürüme: WIZ_MOVE aynı. Her bölge geçişinde REGIONCHANGE (U1-05) ve USER_INOUT (U1-04). NPC_REGION aynı, REQ_NPCIN kaydı U1-05. [C]
10. Dövüş:
    - ATTACK, MAGIC, TARGET_HP, HP/MSP, STATE, DEAD, REGENE, EXP, LOYALTY: aynı. [C]
    - **LEVEL_CHANGE, ITEM_MOVE yanıtı, WEIGHT_CHANGE, POINT_CHANGE, ALL_POINT: u32 ağırlık (U1-03).** [C] Olmazsa seviye atlama, ekipman değişimi ve ganimetten sonra istemci "maks. ağırlık 0" okur; aşırı yük davranışı görülebilir [I].
11. Ronark (PK bölgesi) için isteğe bağlı: WIZ_RANK (U1-06), BATTLE_EVENT, DEATH_LIST. Kanıt: [C] var, sunucu tarafı eksik.

## 6. Uygulama planı önerileri

Ortak kural: her değişiklik `if (ProtocolProfile::ClientVersion(__VERSION) >= 1534)` dalında yapılır (`shared/ProtocolProfile.h`, U1-01, `main`'de mevcut). 1453 yolu bayt bayt aynı kalmalı.

Botlar sunucunun ürettiği paketleri süreç içinde aldığı için `BotCore` ayrıştırıcıları aynı profile bağlanmalı. Öneri:
- `BotCore`'a `struct WireLayout { bool v1534; }` parametresi ekle; varsayılan eski düzen olsun ki testler bozulmasın.
- `BotSession` bu değeri `ProtocolProfile::ClientVersion(__VERSION) >= 1534` ile doldurur.
- `BotCore` doğrudan sunucu global'ine bağlanmamalı.

### U1-02 — WIZ_MYINFO 1534 düzeni (zorunlu, 1–3 dosya)

- **Değişecek:** `GameServer/User.cpp` `CUser::SendMyInfo` (`:947-1084`). İsteğe bağlı: `User.h`'ye `WriteMyInfoItem19` yardımcısı.
  - `m_bCity` kaldırılır (988).
  - `GetClanID() << GetFame()`.
  - Klan bloğu + RGB0 (`CKnights::m_bCapeR/G/B`, `Knights.h:63`). Klansız: `u64 0, u16 -1, u32 0`.
  - `u8 2, 3, 4, 5`.
  - `uint32(MaxWeight(m_sMaxWeight))`, `uint32(m_sItemWeight)`.
  - Eşyalar §3.1 sırasıyla, hepsi 19 B, `u32 0` mühür alanıyla.
- **Kaynak referansı:**
  - ALPHA `User.cpp:1074-1146` (başlık; bunu al).
  - ALPHA `:1150-1169` (eşya döngüsü; bunu alma, 74 kayıt).
  - [C] `0x6f7520`, izi `scratchpad/k1534/d/final_cases.txt` (`0xe` satırı).
- **Bot:** yok (`BotManager.cpp:4285` yalnız sayaç).
- **Doğrulama:** derleme. `FDP_PACKET_TRACE` (`GameServer/PacketTrace.cpp`) ile giden MyInfo uzunluğunu kontrol et (klansız, boş çanta: 1453 uzunluğu + 551). İnsan istemciyle giriş denemesi.

### U1-03 — Ağırlık alanları u32 (zorunlu, 2 dosya)

- **Değişecek:**
  - `User.cpp`: `CUser::LevelChange` (1866-1872), `CUser::PointChange` (1912), `CUser::SendItemMove` (3378), `CUser::AllPointChange` (4216-4219).
  - `ItemHandler.cpp`: `CUser::SendItemWeight` (520-525).
- **ALPHA:** `SendItemMove` `:3849-3873`, `AllPointChange` `:4530-4780`, `SendItemWeight` `:512-517` doğru. `LevelChange` `:2046-2106` ve `PointChange` `:2113-2131` **yanlış** (u16 kırpması); bu ikisini [C]'ye göre yaz.
- **[C]:** `0x704dd0`, `0x705a80`, `0x701140`, `0x7015c0`, `0x714510`.
- **Bot:** yok.

### U1-04 — UserInfo 1534 + bot ayrıştırıcısı (zorunlu, 4 dosya)

- **Değişecek:**
  - `GameServer/CharacterMovementHandler.cpp` `CUser::GetUserInfo` (`:99-161`): klansız 14 B (111); klanlıda pelerinden sonra R, G, B, 0, u8 2 (119); eşya listesi `BREAST..LEFTHAND, CTOP, CHELMET, CLEFT, CRIGHT` (145-158).
  - `BotCore/Perception.h` `ParseUserInfo` (kalabalik `:251-299`): 1534'te `:272 r.Skip(2) // cape id` sonrasına `r.Skip(5)`; `:289 r.Skip(70)` → `Skip(84)`.
  - `GameServer/Bot/BotSession.cpp` WIZ_USER_INOUT ve WIZ_REQ_USERIN çağrıları (kalabalik `:405-446`) düzen parametresini geçirir.
  - `Tests/BotCoreTests/PerceptionTests.cpp` `AddUserInfo` (kalabalik `:42-~90`): 1534 varyantı ve testleri (klansız, klanlı, 2 kayıtlı liste).
- **ALPHA:** `CharacterMovementHandler.cpp:103-283`. Savaş ejderha zırhı değişimi isteğe bağlı; düzeni etkilemez.
- **[C]:** `0x6fbb50`, döngüler `0x6fc221` (8) ve `0x6fc328` (dizin 0, 2, 3, 4).

### U1-05 — NpcInfo (adsız) + REGIONCHANGE 3 parça + bot (zorunlu, 5 dosya)

- **Değişecek:**
  - `GameServer/Npc.cpp` `CNpc::GetNpcInfo` (`:138-155`): 1534'te `GetName()` (147) yazılmaz. `GetType() == 15` olursa uyarı logu yaz; tür 15 şu an DB'de yok.
  - `GameServer/GameServerDlg.cpp` `RegionUserInOutForMe` (`:1362-1380`): 1534'te `[0]`, `[1, n@1, ids]`, `[2]`, her biri `SendCompressed` ile.
  - `BotCore/Perception.h` `ParseNpcInfo` (kalabalik `:838-870`): 1534'te `:848-849` `r.Str(name)` atlanır, `out.name` boş kalır. Ad yalnız `BotManager` tanı dökümünde kullanılıyor.
  - `BotSession.cpp` WIZ_REGIONCHANGE dalı (kalabalik `:447-474`): 1534'te `sub = data[0]`. 1 → `ParseRegionList(data+1, len-1)`, sonra Retain ve pending. 0 ve 2 → işlem yok. Bu yapılmazsa 0 ve 2 paketleri `Retain(ids, 0)` ile **tabloyu siler** (önceki rapor). NPC dalları (`:506-545`) düzen parametresini geçirir. WIZ_NPC_REGION (`:537-545`) değişmez.
  - `Tests/BotCoreTests/PerceptionTests.cpp`: NPC ve bölge fikstürleri.
- **ALPHA:** `Npc.cpp:144-163`, `GameServerDlg.cpp:1520-1544`.
- **[C]:** `0x6fe240` (ad dalı `0x6fe3b3`), `0x6fdce0`.
- **`RoamMonSense.h`:** yalnız WIZ_ATTACK okuyor (kalabalik `:43-46`); değişmez.

### U1-06 — 1534'ün C→S ek alanları + PK sıralaması (önerilir, 2 dosya)

- **Değişecek:**
  - `NPCHandler.cpp` `CUser::ItemRepair` (`:8-70`): `sNpcID` okunur.
  - `NPCHandler.cpp` `CUser::HandleCapeChange` (`:806-943`): r, g, b (+pad) okunur; başarı yanıtına RGB0 eklenir (920-923). Bilinen DB isteği hatası birlikte düzeltilir (ALPHA `NPCHandler.cpp:1200-1203` ↔ `DatabaseThread.cpp:466-473`).
  - `User.cpp` `CUser::SelectWarpList` (`:4303-4346`): `npcid` okunur.
  - `User.cpp` `CUser::HandlePlayerRankings` (`:5355-5505`): 1534'te tür baytı yok (tür → PK); yanıtta tür baytı ve girdi başı premium yok.
- **ALPHA:** `NPCHandler.cpp:7-79`, `:1041-1209`; `User.cpp:4865-4908`, `:6217-6369`.
- **Bot:** yok; botlar bu paketleri kurmuyor.

### U1-07 — Klan RGB ve görev sayaçları (sonra, ≤ 5 dosya)

- **Değişecek:**
  - `KnightsManager.cpp` `RecvUpdateKnights` (`:606-644`): JOIN kaydına RGB0.
  - `Knights.cpp` `CKnights::SendUpdate` (`:321-328`): RGB0; ittifak kuralları için ALPHA `:456-491`.
  - ALLY_* oluşturucuları: `DatabaseThread.cpp:548,576`, `KnightsManager.cpp:1015,1041,1261`. Her biri için önce istemci okumasını alt koda göre doğrula (`scratchpad/k1534/d/ks_cmp.txt`).
  - `QuestHandler.cpp` `QuestV2MonsterCountAdd` (`:151-187`) ve `QuestV2MonsterDataRequest` (`:210-232`): 18xx biçimi (OURS'ta yorumlu duruyor).
- **Bot:** yok.

### U1-08 — İsteğe bağlı ve açık maddeler

- WIZ_WARP'a `u16 y` ekle.
- GameStart(1)'e WIZ_PREMIUM ekle (ALPHA).
- WIZ_BATTLE_EVENT düzenini [C]'ye göre doğrula (OURS zaten gönderiyor).
- 0x19 sohbet odası, 0x90 ölüm listesi.
- ITEM_MOVE yön 12/13.
- CHANGE_HAIR genişliği, CHAT_TARGET, CORPSE, MAP_EVENT, CONCURRENTUSER.
- **Ayrı KI önerisi:** `ReqSkillDataLoad` gönderim hatası (`DatabaseThread.cpp:292-299`); 1534'ten bağımsız.

## 7. ALPHA'da daha yeni istemcilerden alınmış görünenler

| Öğe | ALPHA | KOD | 1534 istemci [C] | Hüküm |
|---|---|---|---|---|
| Genie `0x97`, `0x98`, Achieve `0x99`, EXP seal `0x9A`, SP `0x9B`, HackShield `0xA1` | var (`packets.h:150-157`, `GenieHandler.cpp`) | yok | işlemiyor (anahtar ≤ 0x90, default false) | borrowed, alma |
| Moving tower `0x84`, Capture `0x85`, VIP depo `0x8B`, `VIPWAREHOUSE_MAX 48` | var | yok | işlemiyor | borrowed |
| 8 cospre / CFAIRY / 74 yuva | var | yok (COSP_MAX 5) | 4 cospre + 2 çanta + 24 = OURS modeli | borrowed, alma |
| Kanat (CWING) görünümü | UserInfo listesinde, depolama kayık | yok | MyInfo dizin 0'ı, UserInfo dizin 1'i okumuyor | 1534'te kanat görünmüyor |
| Pelerin RGB + bayrak 2, MyInfo 2,3,4,5 | var | yok | **var** | gerçek 1534 özelliği, al |
| Mühür eşyası (`SendUserSealItem`) | MyInfo'da | yok | mühür bloğu okuyucusu her iki istemcide var | borrowed değil (OURS u32 0 gönderiyor) |
| Görev 9 "18xx" biçimi | kısmen | yok | **var** (u16 sayaçlar) | gerçek 1534 |
| `>=1950` bekçileri (ALLCHAR bayrağı, ganimet) | var | `>=1920` | 1534'te kapalı | ilgisiz |
| Sohbet odası `0x19` | derlenmeyen dosya | yok | **var** | gerçek 1534; ALPHA'da çalışmıyor |

## 8. Kalan riskler ve doğrulanmayanlar

- Değişken uzunluklu C→S oluşturucuları (login, yeni karakter, sohbet, hareket, saldırı, büyü) istemcide çıkarılamadı. Kanıt: ALPHA ayrıştırıcısı = OURS ve bot paketleri aynı.
- UserInfo cospre sırası çıkarımdır [C + I]. Cospre giyen bir insan istemciyle görsel test gerekli.
- Kontrol akışı farkları yalnız kritik satırlarda elle doğrulandı. "AYNI" işaretli küçük paketlerde akış değişikliği kaçmış olabilir. Örnekler: ITEM_DROP, CHAT_TARGET, CONCURRENTUSER, MAP_EVENT, CORPSE.
- İstemci okuyucuları sınır denetimli. Kısa paketler çökme yerine 0 okur; uzun paketlerin fazlası yok sayılır. Bu yüzden bazı ALPHA hataları (MyInfo kuyruğu, u16 ağırlıklar) görünmeden "çalışıyor" izlenimi verebilir.
- Bu rapor `main` @ `d79aeec5` üzerinde hazırlandı. U1-01 analiz sırasında birleşti; login ve anahtar satırları buna göre güncellendi.
