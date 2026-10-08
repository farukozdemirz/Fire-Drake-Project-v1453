# A — AlphaGame v1534 kaynak analizi (sunucu tabanı olarak benimsenebilir mi?)

Tarih: 2026-10-08. Kapsam: yalnız kaynak okuma (salt-okunur). Hiçbir indirilen ikili çalıştırılmadı/yüklenmedi.
Etiketler: **[V]** kendim dosyada okudum/komutla ölçtüm; **[I]** çıkarım; **[A]** ajan (alt görev) bulgusu, örnekleri ben de kontrol ettim.

Kısaltmalar:
- **ALPHA** = `/mnt/c/dev/fdp1534/alpha/ALPHA KO 1534 PROJE/1-Game Source` (`shared/version.h`: `__VERSION 1534`, `__GUARD_VERSION 2025`)
- **KOD** = KODevelopers-1534 (2017, GitHub)
- **UP** = 1453 üst kaynak (`0f52027`)
- **OURS** = `/mnt/c/dev/fdp-merge-final` `main` (`9e6590d3`)

Satır numaraları aksi belirtilmedikçe ALPHA dosyalarına aittir. CRLF kaldırıldıktan sonra sayılmıştır (`tr -d '\r' | nl`).

## 0. Özet ve karar

**Karar: Seçenek A.** Mevcut kaynağımızı (OURS) koruyalım. ALPHA'yı yalnız başvuru ve veri kaynağı olarak kullanalım; tek tek özellik alalım. B (ALPHA'yı yeni taban yapmak) önerilmez. Gerekçe §8'de.

1. **Soy [V].** ALPHA, "KOD + değişiklikler" değildir; UP ile aynı 1453 ailesinden ayrı bir çataldır.
   - KOD'a özgü satırların yalnız %7,4'ü ALPHA'da var.
   - UP'ye özgü satırların %52,3'ü ALPHA'da var.
   - UP→ALPHA (boşluk hariç): 171 dosya aynı, 121 farklı, +11.294 / −3.526 satır; 31 dosya eklenmiş (bazıları derlenmeyen ölü kod).
   - 144 yeni fonksiyon (4.236 satır). Eklenen alanlar: Genie, BDW, Juraid, Chaos, Monster Stone, balıkçılık ve madencilik, mühür, VIP depo, klan eklentileri, kral sistemi.
   - Pet sistemi ve bowl etkinliği yok.
   - "Guard" (`__GUARD_VERSION 2025`) ve HWID lisansı kodda tanımlı ama **ölü kod**: hiç çağrılmıyor.
2. **Klan [V].**
   - Otomatik klan türü yükseltmesi **yok**; yalnız NP iadesinde kademe düşürme eklenmiş.
   - Pelerin RGB pakette **gönderiliyor**; pelerin boyama açık; derece eşikleri ini'den okunuyor.
   - UP'deki `WIZ_CAPE` DB isteği hatası (r,g,b) **düzeltilmiş** (`NPCHandler.cpp:1200-1203`).
3. **Protokol.**
   - Opcode numaraları aynı.
   - Kripto anahtarı farklı: ALPHA `0x1257…0465` kullanıyor, bizim `0x7412…5200`. Mevcut istemcimiz ALPHA'ya bağlanamaz [V].
   - Botların kurduğu paketlerin **hepsi aynı** [A].
   - Botların ayrıştırdığı **3 paket farklı** [A+V]:
     - **UserInfo**: kayıt başına +19 bayt (klan bloğuna pelerin RGB, 12 ekipman yuvası)
     - **NpcInfo**: NPC adı gönderilmiyor
     - **WIZ_REGIONCHANGE**: 3 parçalı paket. Düzeltilmezse oyuncu tablosunu her bölge değişiminde siler.
   - Envanter düzeni değişmiş: 73 → 74 yuva [V].
   - KOD bu düzenleri 1453 ile aynı bırakıyor. Hangisinin gerçek 1534 istemcisine uyduğu kanıtlanmamış [I].
4. **Mekanik [A, örnekler V].** 91 davranış farkı: **11 BREAKS**, 36 MAYBE, 44 NEUTRAL. Örnekler:
   - AP iki kez ekleniyor.
   - Ayakta MP yenilenmesi yok.
   - Hız sınırı herkes için 90.
   - Debuff direnci "ikinci şans" oldu.
   - Mage Armor yansıtması değişmiş.

   Hasar formülleri, menzil, düşmanlık, Ronark kuralları ve party kuralları aynı. GameServer↔AIServer protokolü değiştiği için AIServer da ALPHA'nınki olmalı.
5. **Kancalar [V].** 16 dosya, 42 hunk:
   - `patch --dry-run` ile 34/42 hunk uygulanıyor.
   - 3-yollu birleştirmede 5 dosyada 10 çakışma bölgesi var; hepsi "iki taraf da satır ekledi" türünde.
   - Bot katmanının kullandığı API'nin 240/241 ismi ALPHA'da var.
   - Kanca taşımak ucuz; asıl maliyet madde 3, 4 ve 6.
6. **Güvenlik [A, 1–3 ve 7 V].**
   - Klasik arka kapı yok.
   - ALPHA'ya özgü **HIGH** açıklar: isimle karakter ele geçirme (mühür sistemi), GM'siz ışınlanma (`WIZ_MOVING_TOWER`).
   - MEDIUM: uzaktan çökertme, bölge ışınlanması, BDW ele geçirme, sabit kodlu SQL kimlik bilgileri, hazır `.lib`/`.exe` dosyaları.
   - Şifre loglama hatası (HIGH) kalıtsal: **OURS `LogInServer/LoginSession.cpp:139`'da da var** [V].
7. **Derleme [V].**
   - Release|Win32 tanımlı; toolset karışık (v142/v143).
   - `/p:PlatformToolset=v143` ile engel görülmedi [I].
   - Bot için GameServer'a C++17 eklenmeli.
   - Yazar fiilen Debug|x64 kullanmış.

---

## 1. Soy (lineage) ve dosya düzeyi karşılaştırma

### 1.1 Sayılar [V]

Kapsam: `.cpp .h .c .hpp .inl .vcxproj .filters .sln .rc`. `Debug/ Release/ obj/ .vs/ x64/` yok sayıldı. Satır sonu normalize edildi.

| Karşılaştırma | Aynı | Değişen | Eklenen | Silinen | +satır | −satır |
|---|---|---|---|---|---|---|
| KOD → ALPHA | 183 | 125 | 28 | 2 | 23.690 | 12.029 |
| UP → ALPHA | 178 | 127 | 31 | 3 | 24.183 | 7.170 |
| UP → KOD | 212 | 93 | 5 | 3 | 8.784 | 3.432 |

Yalnız kaynak dosyalar, boşluk farkı yok sayılarak:

| Karşılaştırma | Aynı | Değişen | +satır | −satır |
|---|---|---|---|---|
| UP → ALPHA | 171 | 121 | 11.294 | 3.526 |
| KOD → ALPHA | 178 | 117 | 11.597 | 7.512 |
| UP → KOD | 210 | 82 | 5.080 | 1.397 |

### 1.2 Soy testi [V]

Ortak dosyalarda iki satır kümesi tanımlandım:
- "yalnız KOD" satırları: KOD'da olup UP'de olmayan satırlar.
- "yalnız UP" satırları: UP'de olup KOD'da olmayan satırlar.

Her iki kümede ALPHA'da bulunan satır oranını ölçtüm (betik `k1534/tools/lineage.py`):
- yalnız-KOD satırlarının ALPHA'da bulunan oranı: **210/2.842 = %7,4**
- yalnız-UP satırlarının ALPHA'da bulunan oranı: **472/903 = %52,3**

**Sonuç: ALPHA, "KOD + değişiklikler" DEĞİLDİR.**
- ALPHA, UP ile aynı 1453 ailesinden ayrı bir çataldır. 1453 sızıntısının bir türevi üzerine 1534'e uyarlanmış ve özel sunucu özellikleri eklenmiştir.
- KOD'un kendine özgü işleri ALPHA'da yok:
  - "bowl" etkinliği: KOD'da 84 satır, ALPHA'da 0.
  - KOD'un ChatHandler GM komutları: ALPHA'da yok.
- KOD ile ALPHA'nın ortak olup UP'de bulunmayan ~210 satır şunlardır:
  - Kore yorum satırlarının farklı kodlaması
  - kar savaşı ve kuşatma savaşı komutları
  - savaş duyuruları

  Bu satırlar ortak bir ara ataya işaret eder [I].

### 1.3 Tarih ve derleme izi [V]

- `.sln` 2021-04-11; son düzenleme 2023-04-30 (`GameServer/User.cpp`, `GameServerDlg.cpp`).
- `GameServer/proj-GameServer.log` VS 2022 Community (Türkçe arayüz) ile derlendiğini ve çıktının `Server-Files\GameServer.exe` olduğunu gösterir.
- Bu çıktı yolu yalnız **Debug|x64** yapılandırmasında tanımlı (`proj-GameServer.vcxproj:141-143`).
- Sonuç: paketteki `GameServer.exe` büyük olasılıkla x64 Debug derlemesidir [I].

### 1.4 UP → ALPHA en çok değişen 40 dosya

Yalnız kaynak, boşluk yok sayıldı.

| Dosya | + | − | toplam |
|---|---|---|---|
| GameServer/User.cpp | +1909 | -262 | 2171 |
| GameServer/GameServerDlg.cpp | +1300 | -355 | 1655 |
| GameServer/MagicInstance.cpp | +580 | -233 | 813 |
| GameServer/DBAgent.cpp | +596 | -197 | 793 |
| AIServer/Npc.cpp | +360 | -226 | 586 |
| GameServer/UpgradeHandler.cpp | +350 | -226 | 576 |
| GameServer/KnightsManager.cpp | +353 | -190 | 543 |
| GameServer/KingSystem.cpp | +396 | -145 | 541 |
| GameServer/EventHandler.cpp | +469 | -35 | 504 |
| GameServer/NPCHandler.cpp | +359 | -145 | 504 |
| GameServer/ItemHandler.cpp | +332 | -145 | 477 |
| GameServer/DatabaseThread.cpp | +262 | -162 | 424 |
| GameServer/Npc.cpp | +313 | -57 | 370 |
| GameServer/MerchantHandler.cpp | +279 | -79 | 358 |
| GameServer/ChatHandler.cpp | +250 | -96 | 346 |
| GameServer/TradeHandler.cpp | +237 | -63 | 300 |
| GameServer/GameDefine.h | +217 | -65 | 282 |
| GameServer/Knights.cpp | +222 | -55 | 277 |
| GameServer/User.h | +208 | -64 | 272 |
| GameServer/CharacterMovementHandler.cpp | +211 | -60 | 271 |
| GameServer/QuestHandler.cpp | +112 | -115 | 227 |
| shared/packets.h | +179 | -33 | 212 |
| GameServer/Unit.cpp | +162 | -41 | 203 |
| GameServer/SealHandler.cpp | +167 | -5 | 172 |
| GameServer/Define.h | +115 | -19 | 134 |
| GameServer/GameServerDlg.h | +103 | -27 | 130 |
| GameServer/MagicProcess.cpp | +89 | -16 | 105 |
| shared/database/structs.h | +85 | -0 | 85 |
| AIServer/GameSocket.cpp | +56 | -28 | 84 |
| shared/globals.h | +50 | -13 | 63 |
| GameServer/AttackHandler.cpp | +35 | -24 | 59 |
| shared/database/OdbcCommand.cpp | +48 | -10 | 58 |
| LogInServer/LoginServer.cpp | +36 | -16 | 52 |
| AIServer/ServerDlg.cpp | +28 | -23 | 51 |
| shared/globals.cpp | +49 | -0 | 49 |
| AIServer/Npc.h | +37 | -12 | 49 |
| AIServer/NpcThread.cpp | +39 | -9 | 48 |
| GameServer/Unit.h | +44 | -2 | 46 |
| GameServer/AISocket.cpp | +36 | -10 | 46 |
| GameServer/LoadServerData.cpp | +39 | -4 | 43 |

### 1.5 Eklenen ve silinen dosyalar (UP → ALPHA) [V]

**Eklenen ve derlenenler** (proje dosyasında var):
- `GameServer/GenieHandler.cpp` (209 satır)
- `GameServer/KnightLogger.cpp/.h` (229 satır)
- `shared/database/` altında 11 yeni tablo başlığı:
  - `EventTimes.h`
  - `FishingTableSet.h`, `MiningTableSet.h`
  - `ItemExchangeExpSet.h`, `ItemExchangeCrashSet.h`
  - `JuraidMountionListInformationSet.h`
  - `KingNominationListSet.h`
  - `MonsterStoneListInformationSet.h`
  - `UnderMonsterChallenge*.h` (2 dosya)
  - `UserSealSet.h`
  - `ItemExpirationSet.h`
- Lua `lprefix.h` ve `lutf8lib.c` (Lua 5.3 dosyaları)
- `ALPHAGAME v1534.sln`

**Eklenen ama derlenmeyen ölü kodlar** (vcxproj'da yok):
- `GameServer/ChatRoomHandle.cpp` (227 satır; `WIZ_NATION_CHAT`=0x19 kullanır, bu değer `WIZ_ITEM_LOG` ile çakışır, `packets.h:27` ve `:150`)
- `AIServer/NpcOrjinal.cpp` (4.009 satır; `Npc.cpp`'nin eski kopyası)
- `shared/Socket*Linux*`, `SocketMgrWin32.*` (8 dosya)

**Silinen:**
- `KnightOnlineServer.sln`
- `scripting/Lua/src/Lua.vcxproj(.filters)`. Yerine `scripting/Lua.vcxproj` kullanılır.

**Fonksiyon düzeyinde** (`tools/funcs.py`):
- ALPHA'da **144 yeni fonksiyon (4.236 satır)** var.
- **17 fonksiyon silinmiş (383 satır)**. Silinen önemli fonksiyonlar:
  - `ForgettenTempleEventTimer`
  - `CastleSiegeWarAttack`
  - DB ittifak fonksiyonları
  - `WordGuardSystem`
  - `MaxWeight`

### 1.6 ALPHA'nın eklediği özellik alanları

| Alan | Kanıt (ALPHA) | Durum |
|---|---|---|
| **Genie** (otomatik avlanma, yeni istemci özelliği) | `WIZ_GENIE 0x97` `packets.h:151`; `GenieHandler.cpp:3,21,70,130` | [V] |
| Hareketli kule / ele geçirme | `WIZ_MOVING_TOWER 0x84`, `WIZ_CAPTURE 0x85`; `GenieHandler.cpp:153`, `EventHandler.cpp:753` | [V] |
| Border Defense War | ini `[BORDER_DEFENSE_WAR]`; `Npc.cpp:703 BDWMonumentProcess`; `TEMPLE_EVENT_BORDER_*` | [V] (UP'de iskelet vardı; 21→53 isabet) |
| Juraid Mountain | ini `[JURAID_MOUNTAIN]`; `JuraidMountionListInformationSet.h`; 2→70 isabet | [V] |
| Chaos | `TEMPLE_EVENT_CHAOS_TIME`; ini `[CHAOS_EXPANSION]` | [V] (UP'de vardı, genişletilmiş) |
| Tapınak etkinlik zamanlayıcısının yeniden yazımı, futbol | `GameServerDlg.cpp:2674 AktiveTempleEventTimer`, `:2829 TempleSoccerEventTimer`; `EventHandler.cpp:457-532` | [V] |
| Forgotten Temple | UP'deki `ForgettenTempleEventTimer` silinmiş; tek zamanlayıcıya taşınmış | [V] |
| Castle Siege War | `CastleSiegeWarAttack` silinmiş; `EventHandler.cpp:159 CastleSiegeWarFlag`, `GameServerDlg.cpp:2598 BanishSiegewarfareLosers`; ini `CASTLE_BATTLE.DAYS` | [V] (UP'de vardı) |
| Monster Stone, Under the Castle | `MonsterStoneListInformationSet.h`, `UnderMonsterChallenge*.h`; `TEMPLE_EVENT_MONSTER_STONE` | [V] |
| Balıkçılık ve madencilik | `User.cpp:6883,6926` (305 satır); `FishingTableSet.h`; `MINING_DELAY 5→2` | [V] |
| EXP / karakter mührü | `WIZ_EXP_SEAL 0x9A`; `DBAgent.cpp:2370,2412,2481`; `SealHandler.cpp` +167 | [V] |
| VIP depo | `WIZ_VIPWAREHOUSE 0x8B`; `VIPWAREHOUSE_MAX 48`; `VIP_*` enum | [V] |
| Klan: duyuru, not, ittifak duyurusu, ad değiştirme, çevrim içi/dışı, pelerin RGB, NP iadesinde kademe düşürme | `KnightsManager.cpp:1507,1563`; `NPCHandler.cpp:953`; `DatabaseThread.cpp:428`; `Knights.cpp:291-393` | [V] (§2) |
| Kral sistemi genişlemesi | `KingSystem.cpp:226,458,1098,1572`; `KingNominationListSet.h` | [V] |
| Eşya takası EXP/kırma, "yaşlı adam" özel takası, yükseltme önizleme, rebirth parşömenleri | `UpgradeHandler.cpp:986`; `packets.h` `UpgradeType*`/`*Scroll` enum | [V] |
| Jackpot ve flash eşyalar | `User.cpp:7636-7824`; ini `[JACK_POTS]` | [V] |
| Askeri kamp bölgeleri, Krowaz kapısı, ilan panosu, satıcı listesi | ini `[MILITARY_CAMP]`; `User.cpp:5022,7858`; `MerchantHandler.cpp:786-873` | [V] |
| KnightLogger (log iş parçacığı) | `KnightLogger.cpp:28`; `DEADLOGUSER..PUSLOG` enum | [V] |
| Ulus değiştirme sohbet komutu `nt` | `ChatHandler.cpp` (CUser tablosu); `CharacterHandler.cpp:6` | [V] (yetki: §6) |
| **"Guard" / `__GUARD_VERSION 2025`** | Yalnız `version.h:4`'te tanımlı. `WIZ_HACKSHIELD_GUARD 0xA1` (`packets.h:157`) ve `HACKSHIELD_GUARD_TIME_SECOND` (`globals.h:308`) tanımlı ama **hiç kullanılmıyor**. Kaynakta guard el sıkışması yok. | [V] |
| HWID lisans kilidi | `shared/HardwareInformation.*` üç sunucuda da üye olarak var. `IsValidHardwareID` **hiç çağrılmıyor**. UP ve KOD bunu `GameServerDlg.cpp:89-93`'te çağırır; ALPHA bu çağrıyı kaldırmış. `GetHardwareID()` çağrılsaydı "Lisans" numarası basardı (`HardwareInformation.cpp:31`). | [V] |
| Pet sistemi | **Yok** (pet isabeti 9→7) | [V] |
| Bowl etkinliği | **Yok** (yalnız KOD'da) | [V] |
| PUS | UP'de zaten var. ALPHA yalnız `PUSLOG` ekler. | [V] |

**Not [I]:** Şu öğeler 1.8xx–2.xxx döneminin istemci özellikleridir:
- Genie
- `WIZ_ACHIEVE 0x99`
- `WIZ_USER_INFORMATIN 0x98`
- GetUserInfo'daki kanat/cospre yuvaları (§3)
- VIP depo

ALPHA'nın bunları daha yeni özel sunucu kaynaklarından taşıdığı anlaşılıyor. Bu yüzden ALPHA'nın paket düzeninin **gerçek bir 1534 istemcisiyle** uyumlu olduğu kanıtlanmış değildir. KOD aynı paketleri 1453 düzeninde bırakır. Hangi düzenin doğru olduğu ancak hedef istemciden alınan paket kaydıyla anlaşılır.

---

## 2. Klan sistemi ve pelerinler [V]

| Soru | UP | ALPHA |
|---|---|---|
| **Klan türünün otomatik yükseltilmesi** (Training→Promoted→Accredited→Royal) | Yok. Yalnız Lua `PromoteClan` var (`QuestHandler.cpp:424`, varsayılan `ClanTypePromoted`). | **Yok.** Aynı Lua yolu (`QuestHandler.cpp:432`, `KnightsManager.cpp:765`). Bağış (`KnightsManager.cpp:840`, `DBAgent.cpp:1357`) yalnız fonu artırır, bayrağı değiştirmez. Yeni olan tek şey ters yön: **NP iadesinde kademe düşürme** (`Knights.cpp:295-343`; fon yetmezse Royal1→…→Accredited5'e iner ve fon ekler). Kademe kuralı yine DB/GM/Lua işidir. |
| **Klan derecesi** (`m_byGrade` 1–5) | `GetKnightsGrade` sabit eşikli (`GameServerDlg.cpp:2917-2931`: 720000/340000/50000/25000) | Eşikler ini'den okunur: `[CLAN_GRADE] GRADE1..4` (`GameServerDlg.cpp:330-333`, `:3957-3971`; varsayılanlar aynı; `Server-Files/GameServer.ini:35-39` aynı değerler) |
| **Pelerin RGB'nin pakette gönderilmesi** | Yok. Yalnız `uint16 capeID` gönderilir (`CharacterMovementHandler.cpp:119`, `User.cpp:951`, `Knights.cpp:326`). | **Var.** GetUserInfo `CharacterMovementHandler.cpp:126-138`: `u16 cape, u8 R, u8 G, u8 B, u8 0`, ardından `u8(2)` (`:143`). SendMyInfo `User.cpp:1109-1128`, ardından `u8 2,3,4,5` (`:1132`). `KNIGHTS_UPDATE` `Knights.cpp:456-491`. Pelerin değişikliği yanıtı `NPCHandler.cpp:1171-1185`. İttifaktaki klan ana klanın pelerinini kullanır; alt-ittifak kendi rengini korur; paralı asker yalnız pelerini alır (`Knights.cpp:467-481`). |
| **Pelerin boyama** | Kapalı (yorum satırı, `NPCHandler.cpp:813-887`) | **Açık** (`NPCHandler.cpp:1050-1053`, `:1126-1137`, `:1162-1167`): Accredited5+ gerekir, +1000 klan puanı |
| **Uzun/kraliyet pelerini kademeye göre** | `KNIGHTS_CAPE.byGrade/byRanking` kontrolü (`NPCHandler.cpp:852-858`) | Aynı kontrol (`NPCHandler.cpp:1102-1108`). Ek olarak kademe düşerse pelerin 0'a sıfırlanır (`Knights.cpp:377-392`). "Uzun pelerin" görseli istemci + `KNIGHTS_CAPE` verisi işidir; kodda ayrı bir kural yok. |
| **İttifakta pelerin** | Yalnız ittifak lideri değiştirebilir (`NPCHandler.cpp:834-840`) | Ana veya alt ittifak klanı değiştirebilir. Alt ittifak yalnız renk değiştirebilir, pelerin kimliğini değiştiremez (`NPCHandler.cpp:1078-1090`). `SendAllianceUpdate` yeni (`Knights.cpp:495-507`). |
| **Bilinen UP hatası**: `WIZ_CAPE` DB isteği `(clanID,capeID)` yazar, `ReqChangeCape` `r,g,b` okur | Hata var: yazan taraf `NPCHandler.cpp:935-937`, okuyan taraf `DatabaseThread.cpp:428-435` | **Düzeltilmiş.** Yazan taraf `NPCHandler.cpp:1200-1203` (`clanID, capeID, R, G, B`), okuyan taraf `DatabaseThread.cpp:466-473`. Ayrıca `KNIGHTS_UPDATE_CAPE 0xF3` DB yolu var (`Knights.cpp:387-390` → `DatabaseThread.cpp:580` → `DBAgent.cpp:1411-1418`); tutarlı. |

ALPHA'da yeni küçük hatalar [V]:
- `Knights.cpp:497-498` `SendAllianceUpdate`, `pAlliance` için null kontrolü yapmıyor.
- `KnightsManager.cpp:779-791` `UpdateKnightsGrade`: klan ittifaktaysa bayrağı **hiç güncellemiyor**; yalnız ittifaktan çıkarma isteği gönderip dönüyor.

Bot katmanına etkisi: GetUserInfo'nun klan bloğu değiştiği için `BotCore/Perception.h ParseUserInfo` kırılır (§3.2).

---

## 3. Protokol

### 3.1 Genel [V]

**`shared/packets.h` farkı** (`tools/defs.py`, yorumlar hariç):
- `#define`: 21 eklendi, 3 kaldırıldı, 1 değişti.
- enum: 75 eklendi, 1 kaldırıldı, 1 değişti.
- **Mevcut WIZ opcode'larının numaraları değişmemiş.** Değişiklikler:
  - `WIZ_PACKET10/11/13` (0x84/0x85/0x8B) yer tutucuları yeniden adlandırılmış: `WIZ_MOVING_TOWER`, `WIZ_CAPTURE`, `WIZ_VIPWAREHOUSE`.
  - Yeni opcode'lar: `WIZ_GENIE 0x97`, `WIZ_USER_INFORMATIN 0x98`, `WIZ_ACHIEVE 0x99`, `WIZ_EXP_SEAL 0x9A`, `WIZ_SP_CHANGE 0x9B`, `WIZ_HACKSHIELD_GUARD 0xA1`.
  - `WIZ_NATION_CHAT 0x19`, `WIZ_ITEM_LOG` ile çakışır (`packets.h:27` ve `:150`); kullanan dosya derlenmiyor.
  - Değişen değerler: `MINING_DELAY 5→2`, `MiningSoccer 16→10`.
  - `MERCHANT_BUY_UNK1 0x30` → `MERCHANT_MENISIA_LIST 0x30`.
  - Yeni alt kodlar: `KNIGHTS_USER_ONLINE 0x27`, `KNIGHTS_USER_OFFLINE 0x28`, `KNIGHTS_NOTICE 0x50`, `KNIGHTS_MEMO 0x58`, `KNIGHTS_UPDATE_USER 0xF2`, `KNIGHTS_UPDATE_CAPE 0xF3`, `KNIGHTS_SAVE 0xF4`, `ClanNameChange*`, `Fishing*`.
  - Yeni `TEMPLE_EVENT_*` süreleri, `CHATROM_CHAT 33`, `NOAH_KNIGHTS_CHAT 34`, `ALLIANCE_NOTICE 35`.

**`shared/JvCryption.cpp`:**
- ALPHA özel anahtarı (`:11`): **`0x1257091582190465`**. Standart 1453/1534 anahtarıdır; KOD ile aynı.
- UP ve OURS: `0x7412580096385200` (`:11`). Yerel, yamalı 1453 exe'mizle eşleşen anahtar budur.
- Sonuç: mevcut istemcimiz ALPHA sunucusuna anahtar değişmeden bağlanamaz.
- Botlar süreç içinde çalıştığı ve şifreleme katmanını kullanmadığı için bundan etkilenmez [I].

**`GameServer/LoginHandler.cpp`:**
- `VersionCheck` aynı: `u16 __VERSION` + anahtar. 1534 değeri gönderilir.
- ALPHA, `LoginProcess`'teki `WordGuardSystem` hesap adı beyaz listesini ve fonksiyonun kendisini **kaldırmış** (UP `LoginHandler.cpp:38`, `:57-73`). Güvenlik gerilemesi (§6).

**`LogInServer/*`:**

Sunucu listesi düzeni:
- ALPHA her zaman `u16 echo` ekler (`LoginSession.cpp:170-172`). UP'de bu alan `#if __VERSION >= 1500` altındadır (`LoginSession.cpp:166`); 1453'te kapalıdır.
- Bilinmeyen bayt ALPHA'da her zaman `u8 0` (`LoginServer.cpp:124`). UP 1453–1599 aralığında `u8 1` gönderir (`LoginServer.cpp:106-107`).
- Haber kutusu biçimi değişmiş.
- `__VERSION > 1886` için 10 dinleme portu dalı eklenmiş.

Kimlik bilgileri:
- `LoginSession.cpp:82-85` hesap adı ve şifreyi kırpar (`ltrim/rtrim`).
- `LoginServer.cpp:139-141`'de ODBC DSN/UID/PWD için **sabit kodlu varsayılan kimlik bilgileri** var. Değerler bu rapora kopyalanmadı.

HWID/guard el sıkışması **yok**. `LoginServer.h`'ye eklenen `HardwareInformation`/`m_HardwareIDArray` üyeleri hiç kullanılmıyor.

**`__VERSION` koşulları:** GameServer/shared'de yalnız ≥1700/≥1950 dalları var. 1453 ile 1534 arasında derleme yolu farkı yok.

### 3.2 Botların kurduğu ve ayrıştırdığı paketler

Ayrıntılı tablo Ek B'de. Kaynak: alt ajan [A]. Benim ayrıca doğruladıklarım [V]:
- GetUserInfo: UP `CharacterMovementHandler.cpp:99-161` / ALPHA `:103-283`
- GetNpcInfo: UP `Npc.cpp:138-155` / ALPHA `:144-163`
- REGIONCHANGE: ALPHA `GameServerDlg.cpp:1520-1544`

**Opcode'lar:** botların kullandığı tüm `WIZ_*`, `PARTY_*`, `MAGIC_*` değerleri, `InOutType`, `USER_*` durumları, `NPC_BAND`, `MAX_USER`, `MAX_ID_SIZE`, `MAX_PARTY_USERS` aynı. Bot yolunda `#if __VERSION` bloğu yok.

**Botların kurduğu paketlerin hepsi AYNI:**
- `WIZ_MOVE`, `WIZ_ATTACK`, `WIZ_MAGIC_PROCESS` (cast, iptal, pot, scroll), `WIZ_STATE_CHANGE`, `WIZ_PARTY` (CREATE/INSERT/PERMIT/REMOVE/PROMOTE), `WIZ_CHAT` (PARTY_CHAT)
- `WIZ_REGENE`, `WIZ_TARGET_HP`, `WIZ_REQ_USERIN`, `WIZ_REQ_NPCIN`, `WIZ_SPEEDHACK_CHECK`
- `WIZ_SEL_CHAR` (DB kuyruğu) ve `WIZ_GAMESTART` 1/2

`WIZ_ITEM_MOVE` hiçbir bot dalında kurulmuyor; sunucu tarafındaki okuma yine de aynı.

**Botların ayrıştırdığı paketlerden FARKLI olan 3 tanesi:**

| Paket | UP | ALPHA | OURS'ta gereken değişiklik |
|---|---|---|---|
| **UserInfo kaydı** (`WIZ_USER_INOUT` ve `WIZ_REQ_USERIN` yanıtının her elemanı) | Klan bloğu 9(+n) bayt. 10 ekipman × 7 bayt = 70. Helmet-gizle baytı. | Klan bloğu 14(+n) bayt (`cape u16, R, G, B, 0, u8 2`). **12 ekipman** (CRIGHT, CWING, CHELMET, CLEFT) × 7 = 84. O bayt artık `m_teamColour`. Kayıt başına **+19 bayt**. | `BotCore/Perception.h` ParseUserInfo: `:271`'deki `Skip(2)`'den sonra `Skip(5)` ekle; `:288`'deki `Skip(70)` → `Skip(84)`. Düzeltilmezse konumlar çöp olur ve `REQ_USERIN` listesi ilk elemandan sonra kayar. |
| **NpcInfo kaydı** (`WIZ_NPC_INOUT`, `WIZ_REQ_NPCIN` yanıtı) | weapon2'den sonra `str8 name` | **NPC adı gönderilmiyor**. Kapı dword'ü 0x02 olabilir (Juraid köprüsü). | ParseNpcInfo: `Perception.h:847`'deki `r.Str(name)` okumasını kaldır. Ad yalnız tanı dökümünde kullanılıyor (`BotManager.cpp:2803`). |
| **WIZ_REGIONCHANGE** | Tek paket: `u16 count, u16 sid[]` | Üç paket: `[u8 0]`, `[u8 1, u16 count, sid[]]`, `[u8 2]` | `BotSession.cpp:361-386`: ilk baytı kontrol et; 1 ise kalanı ayrıştır, 0 ve 2'yi yok say. Düzeltilmezse 0 ve 2 paketleri boş liste sayılır, `Retain(ids,0)` **her bölge değişiminde oyuncu tablosunu siler**. |

Ayrıca `Tests/BotCoreTests` içinde bu üç düzeni kodlayan test verileri güncellenmeli.

**Düzen aynı, davranış farklı (ayrıntı Ek A/B):**

*Saldırı ve skill:*
- R saldırısında istemci delay/menzil kontrolü kaldırılmış.
- Bilinmeyen skill id gönderen oyuncu bağlantıdan atılıyor.
- Oyuncular 300000–399999 aralığındaki skill id'lerini kullanamıyor.

*Hareket ve hız denetimi:*
- Hız sınırı her sınıf için 90.
- SpeedHackTime: 3 ihlalde geri ışınlıyor, 10 ihlalde bağlantıyı kesiyor.

*Isınlanma ve canlanma:*
- Warp sonrası `WIZ_REQ_NPCIN` yerine `WIZ_NPC_REGION` gönderiliyor. Botun bekleyen-id yolu bunu karşılar.
- Ölü oyuncu için `TARGET_HP` hp=0 ile yanıtlanıyor.
- Skill ile diriltmeden sonra da NP=0 olan oyuncu PK bölgesinden atılıyor.

*Karakter seçimi:*
- Yasaklı hesap ve race 0 için bağlantı kesiliyor.
- `SetLogInInfoToDB` her karakter için çağrılıyor. Bizim bot atlatmamız taşınmalı.

**Kapsam notları [A]:**
- `BotCore/RoamMonSense.h` OURS `main`'de yok; alt ajan dosyayı proje dizinindeki `gece/2026-10-02` dalından okudu.
- Alt ajan `/mnt/c/dev/fdp-kalabalik` ağacını yalnız okudu. Aynı düzenleri kullanıyor.

**Ek [V] — envanter düzeni değişmiş:**

| | `COSP_MAX` | `INVENTORY_MBAG` | `INVENTORY_TOTAL` | Kaynak |
|---|---|---|---|---|
| UP | 5 | `SLOT_MAX+HAVE_MAX+COSP_MAX+MBAG_COUNT` | 73 | `shared/globals.h:226-251` |
| ALPHA | **8** | `SLOT_MAX+HAVE_MAX+COSP_MAX` | **74** | `shared/globals.h:252-282` |

Sonuçlar:
- Sihirli çanta yuvaları +1 kayar.
- `strItemTime` 8→4 bayt/yuva olur (`DBAgent.cpp:146-147`, `:347-348`).
- `USERDATA` sorgusunda yeni sütunlar var.

Bot kodu yalnız `INVENTORY_INVENT..+HAVE_MAX` (14..41) aralığını kullanır (`BotManager.cpp:2815`, `ActionExecutor.cpp:1680`), bu aralık değişmemiş. Ancak `db/002`, `db/004` betikleri 73 yuvalık blob yazar:
- ALPHA bu blobları okuyabilir (tampon sıfırla başlatılıyor).
- `SendMyInfo` ve envanter paketleri 74 yuvalık düzene göre gönderilir.
- DB şeması ALPHA'nın `KN_online` şemasına göre olmalıdır (B raporu).

---

## 4. Botların doğrulanmış kurallarını bozabilecek mekanik sapmalar

Kaynak: alt ajan raporu, Ek A (dosya bazında satır kanıtlı tam liste) [A].

Benim ayrıca okuyup doğruladıklarım [V]:
- #8 `User.cpp:2487`
- #9 `User.cpp:3905-3908`
- #10 `User.cpp:3406-3413`
- #11 `User.cpp:4146-4175`

**Sayım:** 91 davranış maddesi var. **11 BREAKS / 36 MAYBE / 44 NEUTRAL.**

**BREAKS: `docs/03`'teki doğrulanmış kuralları bozanlar** (her satırda ALPHA:satır ve UP:satır Ek A'da):

| # | Dosya | Değişiklik | Etkilenen kural |
|---|---|---|---|
| 1 | AttackHandler.cpp | R saldırısında istemci `delaytime`/`distance` kontrolü kaldırılmış (UP `:23-33`) | MEC-R-04 |
| 2 | MagicInstance.cpp | `ExecuteType1` yeniden yazılmış (`:1144-1244`). Leg cut/Scream/Shock Stun yankısı gönderilmiyor. Menzil dışı hedef sessizce atlanıyor. Blink hedefi reddediliyor. AoE yolu eklenmiş. | MEC-MAG-24, MEC-BUF-10 |
| 3 | MagicInstance.cpp | Şimşek/buz stun zarı değişmiş (UP `:1354-1378` → ALPHA `:1501-1527`, `:1735-1810`). Staff/blade skill'leri id+80000 Type4 debuff uyguluyor. Bu 12 id bizim `MAGIC_TYPE4`'te yok, bu yüzden yankı gitmiyor. | MEC-MAG-09/24 |
| 4 | MagicInstance.cpp | Slow/stun debuff'larında direnç artık "ikinci şans" (`:2084-2165`). ≈%78,7 direnç ölçümü geçersiz. Direnilen cast `data[1]=0` ile yayınlanıyor. | MEC-BUF-05, MEC-MAG-15 |
| 5 | MagicInstance.cpp | AoE debuff, gizliliği yalnız gerçek kurbanlardan kaldırıyor (UP: 3×3 bölgedeki her düşman) | MEC-BUF-09 |
| 6 | MagicInstance.cpp | Mage Armor yansıtması tam hasar değil; saldırana Fire/Ice/Lightning Armor proc skill'i atıyor (`:3358-3404`) | MB-03 |
| 7 | MagicProcess.cpp | Berserker (BuffType 18) +%20 saldırı veriyor (`:468-473`, `:839-843`) | hasar modeli |
| 8 | User.cpp | AP hesabında `additionalAP` ve AP bonusu **iki kez** uygulanıyor (`:2487`). STR 255 warrior'da ≈+108 AP. | hasar modeli (MEC-DMG-01 girdisi) |
| 9 | User.cpp | Ayakta MP yenilenmesi kapatılmış (`:3907` yorum satırı); yalnız otururken yenileniyor | mana ekonomisi (F4 ölçümleri) |
| 10 | User.cpp | WIZ_MOVE hız sınırı her sınıf için 90 (`:3413`; önceden 45/67/90) | MEC-MOV-02 |
| 11 | User.cpp | SpeedHackTime toleransı +10 → +15; 3 ardışık ihlalde geri ışınlama, 10'da bağlantı kesme (`:4146-4175`) | MEC-MOV-09 |

**Önce bakılması gereken MAYBE maddeleri:**

*Ödül, envanter, seviye:*
- PK bölgesindeki her PvP öldürmede +10000 EXP ve "Meat Dumpling" eşyası veriliyor. Bot envanteri dolar, botlar seviye atlayabilir.
- Rakip (rival) bonusu artık kurbandan düşülmüyor (kurban −200 yerine −50 NP kaybediyor).
- `ExpChange`, bölge seviye aralığı dışına çıkanı bölgeden atıyor.
- Taşıma kapasitesi yarıya inmiş.

*Skill ve buff davranışı:*
- Bilinmeyen skill id gönderen oyuncu bağlantıdan atılıyor.
- Caster'ı giden DoT/HoT etkisi hiç bitmiyor.
- Grup heal: party'deki caster artık otomatik dahil edilmiyor; hedef noktanın Radius'u içinde olmalı.
- Grup HP buff'ları menzil dışındaki party üyelerini atlıyor.
- Type8 (çağırma/diriltme) değişmiş.

*Diğer:*
- Canavar dönüşümündeki oyuncuya saldırılamıyor.

**Değişmeyenler (kontrol edildi):**
- Fiziksel ve büyü hasar formülleri (`Unit::GetDamage`, `GetMagicDamage`), `MAX_DAMAGE 32000`.
- Saldırı menzili (`isInAttackRange`), R saniyede 1 (`PLAYER_R_HIT_REQUEST_INTERVAL 1.0`), recast ve aynı-tür kapıları.
- Ronark düşmanlık kuralı; zone 71'de güvenli alan ve blink yok.
- Öldürmede altın ve NP tabanı; Ronark PvP'de EXP kaybı yok.
- Party 8 kişi ve davet kuralları.
- Hareket yalnız `IsValidPosition` ile denetleniyor; WIZ_WARP yalnız GM.
- MB-04, MB-05, MB-14 hataları hâlâ duruyor.
- Zone 71 en yüksek seviye 80 → 83.

**Karşılaştırılmayanlar:**
- AIServer ve QuestHandler.
- ALPHA'nın GameServer↔AIServer protokolü değişmiş: `AG_USER_MOVE` +1 bayt, `AG_ZONE_CHANGE` +event room, yeni `AG_HEAL_MAGIC`. Bu yüzden **ALPHA GameServer'ı UP/OURS AIServer'ıyla çalışmaz.** B seçeneğinde AIServer da ALPHA'nınki olmak zorunda; canavar davranışı yeniden doğrulanmalı.

---

## 5. Kanca noktası uyumluluğu (dokunduğumuz 16 üst kaynak dosyası) [V]

### Yöntem

Scratch dizini `k1534/apply/`. Rapor dışında hiçbir şey saklanmadı; dizin silindi.

Dosya kümeleri:
- `base` = `git show 0f52027:<f>`
- `ours` = `git show main:<f>`
- `alpha` = ALPHA dosyası

Hepsinde CRLF kaldırıldı.

Yapılan iki test:
- (a) `patch --dry-run -p1 -l --fuzz=2` ile hunk bazında uygulama denemesi.
- (b) `git merge-file alpha base ours` ile 3-yollu birleştirme.

`git apply --3way` için depo gerekir; aynı işi `merge-file` yapar.

### Sonuç tablosu

| Dosya | Hunk | patch --dry-run | 3-yollu çakışma | Not |
|---|---|---|---|---|
| `GameServer/AttackHandler.cpp` | 2 | 2/2 (biri −7 kaydırmalı) | 0 | DamageTrace kancası |
| `GameServer/CharacterSelectionHandler.cpp` | 1 | 1/1 (+6) | 0 | `SetLogInInfoToDB` atlatma |
| `GameServer/ChatHandler.cpp` | 4 | 3/4 (#3 başarısız; #2 ve #4 fuzz 2) | 2 | Komut tabloları: ALPHA'da `noticeall`, `siegewarfare`, `give_item`, `exp_add`, `np_add`, `money_add`, `tp_all` ve `nt` (CUser tablosu) eklenmiş. Çözüm basit: satır ekleme. |
| `GameServer/DatabaseThread.cpp` | 1 | 1/1 (+48) | 0 | `AccountLogout` atlatma |
| `GameServer/GameServerDlg.cpp` | 6 | 3/6 (#3, #4, #5 başarısız) | 4 | ALPHA `Startup()` sırasını değiştirmiş: `Listen`, `InitSessions`, `RunServer` yeni `StartUserSocketSystem()`'a taşınmış (`GameServerDlg.cpp:118-145`). Log dosyaları `CreateDirectories()`'e taşınmış. Bu yüzden `BotManager::Startup()`, `NavService::Startup()`, `StartTicking()` elle yerleştirilmeli. `Timer_UpdateSessions` (`:832-859`) `#ifndef DEBUG` bloğunu kaldırmış; bot atlatması elle eklenmeli. |
| `GameServer/GameServerDlg.h` | 1 | 1/1 (+78) | 0 | |
| `GameServer/MagicInstance.cpp` | 2 | 2/2 (fuzz 1, +89) | 0 | `ExecuteSkill` kancası |
| `GameServer/User.cpp` | 3 | 2/3 (#1 başarısız) | 1 | `CUser` kurucusu: ALPHA gönderme tamponunu `KOSocket(...,16384, 8192)` yapmış, biz `3172`. `Send/SendCompressed` geçersiz kılması ALPHA'da çakışmıyor; `CUser::Send` tanımlı değil, `KOSocket.h` değişmemiş. |
| `GameServer/User.h` | 3 | 2/3 (#3 başarısız) | 1 | Komut bildirimi: ALPHA'da `HandleNtsCommand` |
| `GameServer/proj-GameServer.vcxproj` | 5 | 3/5 (#2, #3 başarısız) | 2 | `PreprocessorDefinitions` satırları; ALPHA'da 8 yapılandırma var, bizde 2 |
| `GameServer/proj-GameServer.vcxproj.filters` | 2 | 2/2 | 0 | |
| `shared/KOSocketMgr.h` | 5 | 5/5 (+12/+13) | 0 | ALPHA yalnız `SuspendServer/ResumeServer` eklemiş |
| `shared/SMDFile.h` | 1 | 1/1 | 0 | `GetHeights()` |
| `shared/SocketDefines.h` | 1 | 1/1 | 0 | `SOCKET_IO_EVENT_BOT_TICK` |
| `shared/SocketMgr.cpp` | 3 | 3/3 (−1) | 0 | ALPHA'da yalnız `(PULONG_PTR)` x64 düzeltmesi var |
| `shared/SocketMgr.h` | 2 | 2/2 (+2) | 0 | ALPHA `ThreadMutex` eklemiş |
| **Toplam** | **42** | **34/42** | **10 bölge / 5 dosya** | Tüm çakışmalar "iki taraf da satır ekledi" türünde. Anlamsal yeniden tasarım gerekmiyor. |

### API yüzeyi [V]

Kontrol yöntemi (`tools/api.py`, `tools/sig.py`):
- `GameServer/Bot/*.cpp|h` dosyalarının çağırdığı üye ve metotlar çıkarıldı. UP'de tanımlı olan 241 isimden **240'ı** ALPHA'da var. Eksik tek isim `team`; yerel değişken adı, gerçek bir eksik değil.
- Botların kullandığı 54 sabitin tamamı ALPHA'da var.
- 89 bildirimin 9'u farklı. Hiçbiri derlemeyi kırmaz. Örnek: `CanUseItem(uint32, uint16 sCount)`; ALPHA'da `uint32`→`uint16`.

Soket katmanı:
- `KOSocket.h`, `Socket.cpp/.h` aynı.
- `KOSocket.cpp` yalnız harf büyüklüğü ve cast farkı içeriyor.
- `Packet.h`'ye `m_sOwnerID` eklenmiş.
- `ByteBuffer.h`'de `ASSERT`'ler sessiz `return` olmuş (`ByteBuffer.h:148-169`). Taşma artık sessiz geçer.

**Sonuç:** Metin düzeyinde kanca taşıma küçüktür: yaklaşık 10 elle çözüm, 1–2 saat.

Asıl maliyet başka yerlerde:
- paket düzeni (§3.2)
- mekanik (§4)
- DB şeması (B raporu)
- derleme ayarı (§7)

### x64 ve Win32 [V]

- ALPHA'nın asıl kullanılan yapılandırması **Debug|x64**. Çıktısı `..\..\Server-Files`. `Debug|x64` ve `shared/x64/Debug` artıkları bunu gösterir (§1.3).
- Release|Win32 yapılandırması tanımlı (`TargetMachine MachineX86`).
- Kodda `_WIN64` dalı yalnız derlenmeyen `SocketMgrWin32.cpp:24` dosyasında var.
- Kaynak Win32 kökenli. x64 için yapılan değişiklik `(PULONG_PTR)` cast'i gibi her iki platformda da derlenir. Win32 derlemesinin çalışması beklenir [I]. Derlemeyi denemedim (kural gereği).

---

## 6. Güvenlik incelemesi

Kaynak: alt ajan raporu, Ek C (26 satır, tam sohbet komutu listesi) [A].

Benim ayrıca okuyup doğruladıklarım [V]:
- #1 `SealHandler.cpp:222-238`
- #2 `GenieHandler.cpp:153-182`
- #3 `LoginSession.cpp:143`
- #7 `LoginServer.cpp:139-141`
- HWID ve guard'ın ölü kod olması (§1.6)
- `WordGuardSystem`'in kaldırılması (§3.1)

**Klasik arka kapı bulunmadı.** Şunların hiçbiri yok:
- usta şifre, özel ad karşılaştırması, gizli GM yetkisi verme
- sohbetten veya paketten kabuk/SQL çalıştırma
- dışarıya bağlantı (URL, yabancı IP, WinInet, URLDownloadToFile, ShellExecute)
- lisans kill-switch, zaman bombası, gizlenmiş yük

SQL çağrılarının hepsi `?` parametresi veya `%d` kullanıyor. `+` sohbet komutları yalnız `isGM()` iken çalışıyor (`ChatHandler.cpp:120`).

| # | Önem | Yer (ALPHA) | Bulgu | Soy |
|---|---|---|---|---|
| 1 | **HIGH** | `SealHandler.cpp:222-265,321`; `DBAgent.cpp:2370-2426,2498-2508` | **İsimle karakter ele geçirme.** Karakter mühürleme işlemi istemciden gelen karakter adının istekte bulunan hesaba ait olup olmadığını kontrol etmiyor. Mühür açılınca kurbanın karakteri saldırganın hesap yuvasına yazılıyor. | yalnız ALPHA |
| 2 | **HIGH** | `GenieHandler.cpp:153-209` (`WIZ_MOVING_TOWER 0x84`) | **GM yetkisi olmadan ışınlanma.** Herhangi bir oyuncu istemcinin seçtiği koordinata ya da hedeflediği oyuncunun yanına `Warp` yapabiliyor. Bu, GM'e özel `WIZ_WARP` kapısını aşar. Ronark'ta bot ve insan için doğrudan istismar. | yalnız ALPHA |
| 3 | **HIGH** | `LogInServer/LoginSession.cpp:143` | **Şifre düz metin olarak loglanıyor.** Biçimde 2 `%s` var, 3 argüman veriliyor; şifre `Authentication=` alanına düşüyor. Paketteki `Server-Files/Logs/Login_30_4_2023.log` bu tür 8 kayıt içeriyor (değerler okunmadı). **Kalıtsal: OURS `LogInServer/LoginSession.cpp:139`'da aynı hata var [V].** | UP / KOD / OURS |
| 4 | MEDIUM | `SealHandler.cpp:295-299`; `User.h:611-617` | Uzaktan çökertme: mühür açma yolunda yuva numarası kontrol edilmiyor ve null değer kullanılıyor. `GetItem` 74. konumda bir fazla okuyor (off-by-one). | yalnız ALPHA |
| 5 | MEDIUM | `CharacterMovementHandler.cpp:867-871` (`ZoneMilitaryCamp`) | NPC veya kapı olmadan, istemcinin seçtiği bölgeye geçiş (mevcut x/z korunarak). Ronark'a doğrudan giriş sağlar. | yalnız ALPHA |
| 6 | MEDIUM | `EventHandler.cpp:753-811` (`WIZ_CAPTURE 0x85`) | Sınır savaşı anıtı her yerden ele geçirilebiliyor. Bekleme alanı (`m_tBorderCapure`) hiç başlatılmıyor. | yalnız ALPHA |
| 7 | MEDIUM | `GameServerDlg.cpp:243-250`; `AIServer/ServerDlg.cpp:848-849`; `LoginServer.cpp:140-141` | Sabit kodlu varsayılan SQL kullanıcı adı ve şifresi. `.ini` dosyaları aynı değerleri taşıyor (değerler bu rapora yazılmadı). | yalnız ALPHA |
| 8 | MEDIUM | `proj-*.vcxproj` Debug\|x64 (`:226`, `:418`, `:409`) | Tedarik zinciri: hazır `Server-Files/shared.lib` ve `Lua.lib` dosyalarına bağlanıyor. Paketteki `.exe`'ler x64 Debug derlemesi. `LogInServer.exe` başka bir kaynak sürümünden derlenmiş (varsayılan sunucu adı tutmuyor). | ALPHA paketi |
| 9–14 | LOW | ayrıntı Ek C | Binary ile kaynak farkı; gizlenmiş .NET `WarpGateEditor.exe`; DB yedeğinde otomatik hesap açan giriş prosedürleri; Genie'nin ücretsiz kullanılması; Menissia ile satıcıya ışınlanma ve null; `+prison` | karışık |
| 15–26 | INFO | ayrıntı Ek C | HWID/"Lisans" ölü kod; `__GUARD_VERSION` kullanılmıyor; dış ağ bağlantısı yok; SQL enjeksiyonu yok; gizli GM yok; yıkıcı DB prosedürleri (`DB_SIFIRLA`, `DB_ROLLBACKFORMAT`; kod çağırmıyor); derlenmeyen dosyalar; paketle gelen loglar (kişisel veri); Lua `os/io` açık ama kullanılmıyor; AIServer kimlik doğrulamasız dinliyor | — |

**Benim bulgum [V]:** `LoginHandler.cpp` hesap adı beyaz listesi `WordGuardSystem`'i kaldırmış (UP `:38`, `:57-73`). Önemi LOW: SQL parametreli olduğu için doğrudan enjeksiyon yok, ama girdi sertleştirmesi gerilemiş.

**Öneriler:**
- Paketteki `exe/lib/pdb/idb/ilk/obj` dosyalarını kullanma; her şeyi kaynaktan derle.
- 1, 2, 4, 5 ve 6 numaralı bulgular kapatılmadan ALPHA kodu hiçbir yerde kullanılmamalı.
- OURS için ayrı iş: `LoginSession.cpp:139` şifre loglama hatası (KNOWN_ISSUES adayı).
- SQL kimlik bilgilerini değiştir; paketteki logları silme ve dağıtma (kişisel veri).

---

## 7. Derleme [V]

Kaynak yalnız okundu; derleme denenmedi. Okuma aracı `tools/vcx.py` (XML ayrıştırma).

### Çözüm yapısı

- Projeler: `AIServer`, `GameServer`, `LogInServer`, `Lua` (`Scripting\Lua.vcxproj`; dizin adı küçük harf, Windows'ta sorun değil), `shared`.
- Bağımlılıklar `.sln` `ProjectDependencies` ile tanımlı:
  - GameServer → shared, Lua
  - AIServer → shared, LogInServer, GameServer
  - LogInServer → shared
- `ProjectReference` yok.
- Yapılandırmalar: `Debug`, `Debug-XP`, `Release`, `Release-XP`, `Template`; hepsi `Win32` ve `x64` için.

### Release|Win32 ayarları

| Proje | Toolset | Çıktı | Kütüphaneler | Notlar |
|---|---|---|---|---|
| GameServer | **v142** | `$(SolutionDir)..\bin\Release\` | `ws2_32.lib`, `…/bin/Release/Lua.lib`, `…/bin/Release/shared.lib` | `MachineX86`, `/MT`. **`LanguageStandard` yok** (v143 varsayılanı C++14). Include `$(SolutionDir)../src/scripting/Lua/src` var olmayan bir yol; zararsız, çünkü include'lar göreli (`../scripting/...`). |
| AIServer | v143 | aynı | `ws2_32`, `shared.lib` | |
| LogInServer | **v142** | aynı | `ws2_32`, `shared.lib` | |
| shared | v143 (XP yapılandırmaları `v141_xp`) | aynı | — | `#pragma comment(lib, "odbc32.lib")` ve `"iphlpapi.lib"` |
| Lua | **v142** | aynı | — | Lua 5.3 kaynağı |

### Engeller ve gerekenler (VS 2022 Community, yalnız v143, Win32 Release)

1. `v142` toolset kurulu değilse GameServer, LogInServer ve Lua için MSB8020 hatası alınır. Bizim `tools/build.sh` zaten `/p:PlatformToolset=v143` geçiriyor (OURS `tools/build.sh:24`). Bizim GameServer projesi de v142'dir. Engel değil.
2. Bot kodu C++17 ister. OURS `LanguageStandard=stdcpp17` kullanır. ALPHA GameServer'a eklenmeli. ALPHA kodu C++14 ile derlendi. C++17'de kırılabilecek bilinen yapılardan (`auto_ptr`, `random_shuffle`, `register`, `bind1st`) GameServer'da yok. `std::ptr_fun` yalnız `shared/tstring.cpp:32-66`'da var; o proje ayrı derlenir. Bu yüzden risk düşük [I].
3. Şu yapılandırmalar Release|Win32 yolunu etkilemez:
   - `Debug|x64` ve `Template` yapılandırmaları: hazır `..\..\Server-Files\Lua.lib` ve `shared.lib` dosyalarına bağlanır (Debug|x64 grubu `proj-GameServer.vcxproj:198-226`).
   - `C:\KO\src\...` mutlak include yolları.

   **Uyarı:** paketle gelen `Server-Files/*.lib` dosyalarına asla bağlanılmamalı.
4. Dış SDK yok. Gerekenler yalnız Windows SDK 10.0, ODBC ve iphlpapi.
5. Derlemede eksik dosya beklenmez: vcxproj'daki 44 GameServer kaynağı diskte var. Ölü dosyalar (`ChatRoomHandle.cpp`, `NpcOrjinal.cpp`, `Socket*Linux*`) projeye dahil değil.

**Sonuç [I]:** VS 2022 v143 ile Win32 Release derlemesini engelleyen bir şey görülmedi. Gerekenler: toolset override ve bot için C++17. Bu, gerçek bir derlemeyle doğrulanmalı (kural gereği denemedim).

---

## 8. Karar: A mı, B mi?

### Seçenek A — OURS kalır, ALPHA'dan özellik veya veri alınır (ÖNERİLEN)

| Paket | Kapsam (tahmin) | Not |
|---|---|---|
| A1. Klan düzeltmeleri | ~4 dosya, ~150 satır | `WIZ_CAPE` DB isteğine r,g,b (`NPCHandler.cpp`); `[CLAN_GRADE]` ini (GameServerDlg .h/.cpp); NP iadesinde kademe düşürme ve pelerin sıfırlama (`Knights.cpp:291-393`); pelerin boyama (`HandleCapeChange`). Paket düzeni değişmez. |
| A2. Pelerin RGB'nin pakete eklenmesi ve ittifak pelerini | ~5 dosya, ~200 satır, ayrıca ParseUserInfo `Skip(5)` | **Yalnız** hedef istemci bu düzeni bekliyorsa yapılır; paket kaydıyla karar verilir (T-UPG-01). |
| A3. ALPHA paket düzeni (12 ekipman, NPC adsız, 3 parçalı REGIONCHANGE) | 3 builder (~150 satır) ve 3 bot ayrıştırıcısı | A2 ile aynı koşul. KOD aynı paketleri 1453 düzeninde tutuyor; doğruyu istemci belirler. |
| A4. Dönem etkinlikleri (BDW, Juraid, Chaos, Monster Stone, Under the Castle) | ~3–5 bin satır, 8+ DB tablosu | K-12'ye bağlı, isteğe bağlı. Kod kalitesi düşük; her parça güvenlik incelemesiyle alınmalı. |

**A'da kaybedilenler** (taşınmazsa):
- Genie
- VIP depo
- balıkçılık ve madencilik
- EXP/karakter mührü (zaten HIGH açıklı)
- jackpot ve flash eşyalar
- kral sistemi eklentileri
- eşya takası EXP/kırma ve "yaşlı adam" takası
- askeri kamp ve Menissia satıcı listesi
- klan duyurusu, notu ve ad değiştirme
- KnightLogger
- tapınak ve futbol etkinliği yeniden yazımı
- ALPHA'nın ek GM komutları
- hareketli kule (zaten HIGH açıklı)

**A'nın riskleri:**
- ALPHA'nın DB, harita ve Lua verisi ALPHA koduna özgü tablolara veya Lua fonksiyonlarına dayanıyor olabilir. Veri alınırken eşleme gerekir (B raporu) [I].
- Bazı dönem düzeltmeleri elle taşınmalı.

### Seçenek B — ALPHA yeni taban olur, bot katmanı yeniden uygulanır

| İş kalemi | Kapsam (tahmin) |
|---|---|
| 16 dosyalık kanca taşıma | 42 hunk, 10 elle çözülecek çakışma (§5) |
| Değişmeden kopyalanacaklar | `GameServer/Bot` (15 dosya, 11.523 satır), `BotCore` (23 dosya, 9.677 satır), `PacketTrace`/`DamageTrace` (4 dosya), `Tests` |
| Bot ayrıştırıcıları | 3 düzeltme ve test verileri (§3.2) |
| Proje dosyası | Bot kaynakları, `stdcpp17`, trace tanımları; Release\|Win32 dışındaki 6 yapılandırma temizlenir |
| ALPHA güvenlik düzeltmeleri | en az 6 işleyici (mühür, hareketli kule, askeri kamp, ele geçirme, `GetItem` sınırı, şifre loglama) ve kimlik bilgileri |
| DB | Bot betikleri `db/002–009` ALPHA şemasına göre yeniden yazılır (74 yuva, yeni sütunlar) |
| Mekanik | 11 BREAKS ve 36 MAYBE yeniden doğrulanır. `docs/03` kuralları (MEC-R-04, MEC-MAG-09/15/24, MEC-BUF-05/09/10, MB-03, MEC-MOV-02/09, AP ve MP modeli) yeniden ölçülür. Bot savaş ayarı (F4 ölçümleri) yenilenir. |
| AIServer | ALPHA'nınki zorunlu; canavar davranışı doğrulanmamış |
| Dokümanlar | `docs/02`/`docs/03` satır referanslarının tamamı (`0f52027`) geçersizleşir |
| **Toplam** | ≈25–30 üst kaynak dosyasına dokunuş, ~1.000–1.500 satır değişiklik (21,6 bin satır değişmeden kopya) ve büyük doküman/doğrulama yükü |

**B'nin riskleri:**
- ALPHA'ya özgü kodda HIGH açıklar ve düşük kalite: ölü kopya dosyalar, null kontrolü eksikleri, `AP` hatası.
- Paket düzeninin hangi istemciye ait olduğu belirsiz (Genie, kanat yuvaları gibi 1.8xx–2.x izleri).
- Doğrulanmış mekaniklerin 11'i bozuluyor; regresyon riski yüksek.

### Gerekçe

Kancaları taşımak ucuz, B'yi engellemiyor. Asıl maliyet şurada:
- B, doğrulanmış mekaniklerin bir kısmını ve botların ayarını geçersiz kılıyor.
- Güvenlik borcu getiriyor.
- Kazancı, çoğu bot projesinin hedefi dışındaki içerik sistemleri.

Önceki analiz (`docs/reports/surum-yukseltme-analizi-…` §6) de A'yı önermişti. ALPHA bu kararı değiştirmiyor; tersine güçlendiriyor.

### Sonraki adım (karar sorusu adayı)

Hedef 1534 istemcisinden `FDP_PACKET_TRACE` ile UserInfo, NpcInfo ve REGIONCHANGE kaydı alınmalı. Bu kayıt A2/A3'ün gerekip gerekmediğini belirler:
- ALPHA düzeni mi gerekiyor, yoksa KOD/UP düzeni yeterli mi?

Sonra A1 tek planla yapılabilir.

---

## Ek A — Mekanik farkların tam listesi (alt ajan raporu, `b/mech.md`)

### ALPHA (v1534) vs UP (v1453): game-mechanics differences in GameServer

- **ALPHA** = `/mnt/c/dev/fdp1534/alpha/ALPHA KO 1534 PROJE/1-Game Source/GameServer`. This is an untrusted download. Nothing from it was executed.
- **UP** = `scratchpad/upstream1453/GameServer`. Our verified rules (`docs/03`) are based on this tree.
- **Method:** `diff -a -w` after removing CR characters (helper `k1534/tools/d.sh`). Whitespace, brace and rename-only hunks are ignored. Any line where a symbol changed was read on both sides.
- **Line numbers:** given as `ALPHA file:line` and `UP file:line`. They refer to the trees above, not to commit `0f52027`.
- **Classification tags:**
  - **[BREAKS]** changes or invalidates a rule the bots rely on. Most of these cite a `docs/03` ID (MEC-* / MB-*).
  - **[MAYBE]** may affect the bots.
  - **[NEUTRAL]** a refactor, a cosmetic change, or a new feature unrelated to the bots.
- **Evidence tags:**
  - **V** = verified (both sides read).
  - **I** = inferred (the code was read, but the runtime effect is a deduction).
- **Context:** the bots fight in Ronark Land (zone 71). Event room is 0 and the zone has no blink. A behaviour change outside zone 71 is NEUTRAL unless noted.

#### Summary

| File | BREAKS | MAYBE | NEUTRAL |
|---|---:|---:|---:|
| AttackHandler.cpp | 1 | 3 | 3 |
| Unit.cpp | 0 | 3 | 7 |
| Unit.h | 0 | 1 | 1 |
| MagicInstance.h | 0 | 0 | 1 |
| MagicInstance.cpp | 5 | 13 | 6 |
| MagicProcess.cpp | 1 | 4 | 5 |
| User.cpp (+ User.h) | 4 | 9 | 7 |
| CharacterMovementHandler.cpp | 0 | 2 | 4 |
| PartyHandler.cpp | 0 | 0 | 2 |
| Define.h / GameDefine.h | 0 | 1 | 8 |
| **Total** | **11** | **36** | **44** |

##### BREAKS list (11)

| # | File | Change | Affected docs/03 rule |
|---|---|---|---|
| 1 | AttackHandler.cpp | The client-supplied `delaytime`/`distance` check for R is gone. | MEC-R-04 |
| 2 | MagicInstance.cpp | ExecuteType1 was rewritten: Type1 echo suppressed for leg cutting/Scream/Shock Stun, a per-target `sRange` skip, blinking targets rejected, AoE path added. | MEC-MAG-24, MEC-BUF-10 |
| 3 | MagicInstance.cpp | Lightning/ice stun roll is inverted and extended to ice. Staff/blade skills now apply a real Type4 debuff (id+80000). | MEC-MAG-09, MEC-MAG-24 |
| 4 | MagicInstance.cpp | Speed/slow/stun Type4 debuffs on players: resistance is now a second chance to land (≈78.7 % resist no longer holds), and a resisted cast is broadcast with `data[1]=0`. | MEC-BUF-05, MEC-MAG-15 (MB-09 broadcast) |
| 5 | MagicInstance.cpp | Type4 AoE strips stealth only from actual victims (after region and range checks), not from every enemy in the 3×3 regions. | MEC-BUF-09 |
| 6 | MagicInstance.cpp | Mage Armor reflect: full damage to attacker is replaced by casting Fire/Ice/Lightning Armor proc skill onto the attacker. | MB-03 |
| 7 | MagicProcess.cpp | Berserker (BuffType 18) now also gives +20 % attack (`m_bAttackAmount`). | damage model (docs/04, Ek A) |
| 8 | User.cpp | The AP formula applies `additionalAP` and the AP-bonus % a second time for all classes. | damage model (docs/04, MEC-DMG-01 input) |
| 9 | User.cpp | Standing MP regeneration was removed; only sitting regenerates MP. | regen/mana economy (F4 measurements) |
| 10 | User.cpp | WIZ_MOVE speed field limit forced to 90 for every class (was 45/67/90). | MEC-MOV-02 |
| 11 | User.cpp | SpeedHackTime: tolerance changed +10 → +15; warp-back only after 3 consecutive hits; disconnect at 10. | MEC-MOV-09 |

##### Preserved rules (checked; no change)

- **R attack:** MEC-R-05 (`isInAttackRange` body identical). MEC-R-06 (zone, alive, blink, hostility; the event-room check is a no-op in zone 71). MEC-R-07 (`CanCastRHit`, `PLAYER_R_HIT_REQUEST_INTERVAL 1.0`). MEC-R-09.
- **Skills:** MEC-MAG-02 / MB-02 (recast check unchanged; the type went int32 → int64). MEC-MAG-03 (gate unchanged, now skipped only for `MAGIC_TYPE4_EXTEND`). MEC-MAG-07 (UseStanding). MEC-MAG-11 (the range check in CheckSkillPrerequisites is unchanged).
- **Damage:** MEC-DMG-01/02/05 (`Unit::GetDamage` changed only in braces). MEC-DMG-03 (`MagicInstance::GetMagicDamage` changed only in formatting). MEC-DMG-06 (`MAX_DAMAGE 32000`). MAX_PLAYER_HP 14000.
- **Zones:** MEC-ZON-01 (player hostility in zone 71: different nation and `canAttackOtherNation`). MEC-ZON-02 (zone 71 has no safety-area entry). Zone 71 attributes are unchanged except max level 80 → 83.
- **Death:** MEC-DTH-02 (no EXP loss in Ronark PvP). MEC-DTH-04 (GoldChange identical for zone 71). MEC-DTH-08 (`BlinkStart` lists zone 71 explicitly). MEC-DTH-10 (`/town` conditions).
- **Party:** MEC-PTY-01/02 (`MAX_PARTY_USERS 8`; same-nation / same-zone / level rule in zone 71).
- **Movement:** MEC-MOV-03 (only `IsValidPosition`). MEC-MOV-06 (WIZ_WARP is GM-only). MEC-MOV-07 (zone change leaves the party). MEC-MOV-08 (Ronark entry rule, max level now 83).
- **Known bugs:** MB-04 and MB-05 are still present. MB-14 (no random offset for Karus on zone change) is still present.
- **Constants:** `RIVALRY_DURATION 300`, `RIVALRY_NP_BONUS 150`, `PLAYER_SKILL_REQUEST_INTERVAL 0.7`.

##### Not compared (out of scope, but relevant)

- **AIServer:** NPC targeting and guard towers were not compared. ALPHA's GameServer→AI protocol also differs: `AG_USER_MOVE` has an extra type byte, `AG_ZONE_CHANGE` has an extra event room, and `AG_HEAL_MAGIC` is new. ALPHA's GameServer is not compatible with UP's AIServer.
- **QuestHandler (`CheckExistEvent`, MEC-MAG-14):** User.h shows the quest map was refactored (`s_QuestMap`, `GetActiveQuestID`), but QuestHandler itself was not diffed.

---

#### AttackHandler.cpp (UP 239 lines, ALPHA 254)

1. **[BREAKS] V.** The client-trusted check is removed. In UP, R was rejected when `delaytime < weapon.Delay+10`, when `distance > weapon.Range`, or (empty hand or mage) when `delaytime < 100`. ALPHA no longer checks any of these.
   - UP AttackHandler.cpp:23-33. ALPHA: absent (between :18 RemoveStealth and :20).
   - This invalidates MEC-R-04. It is looser: bot R packets are still accepted.
2. **[MAYBE] V.** The FREEZE immunity check for R now applies only when the target is a player. An NPC under freeze no longer blocks R.
   - UP :49. ALPHA :41.
3. **[NEUTRAL] V.** The temple-event gating is now per event room (`isAttackable[room]`), and a quest-event (Stone zones) gate was added. The `||` / `&&` precedence is odd.
   - UP :44-47. ALPHA :29-39.
4. **[NEUTRAL] V.** Damage overrides by NPC type: fossil gives 1 with a pickaxe, otherwise 0; refugee 10; tree 20; nation-NONE partner NPC 0.
   - ALPHA :65-88.
5. **[MAYBE] V/I.** Regene with a bind point now respawns at the bind coordinates.
   - UP overwrote the position. `x,z` stayed 0, then `SetPosition(x,0,z)` and `m_LastX/Z = 0` ran (UP :131-134, :168-171), so a bind respawn landed at (0,0) (I).
   - ALPHA sets `x,z = pEvent->fPos` (ALPHA :149-154, :188-191).
   - Only matters if a bot has `m_sBind` in zone 71 (MEC-DTH-06).
6. **[MAYBE] V.** The NP==0 kick-out on respawn (MEC-DTH-09) now also runs after a skill resurrection (`magicid != 0`).
   - UP :230-239 (only when `magicid == 0`). ALPHA :250-254 (always).
7. **[NEUTRAL] V.** The home-zone respawn branch was extended to ZONE_ELMORAD4, and the Moradon arena respawn to Moradon 1-5.
   - UP :137, :148. ALPHA :157, :168.

#### Unit.cpp (UP 1356, ALPHA 1504)

1. **[MAYBE] V.** `Unit::CanAttack` now also requires the same event room, and a target in **monster transformation** can no longer be attacked.
   - UP :855-875. ALPHA :857-881 (:862-863, :875).
   - Zone 71 uses event room 0. Humans using monster-transform scrolls become untargetable.
2. **[MAYBE] V/I.** `CNpc::isHostileTo`:
   - UP made **every** NPC hostile to players while `m_bAttackBifrostMonument` was true (UP :1171, a bug). ALPHA limits this to the Bifrost monument, plus the border monument in temple zones (ALPHA :1207-1217).
   - UP's siege block dereferenced a possibly uninitialised `pKnights` (UP :1180-1190). It is gone.
   - New: an event-room check (:1197); during a NATION_BATTLE in a war zone, NPC protos 1101-2203 and 2302-2306 are hostile to all (:1221-1225); Delos artifact/gate rules (:1227-1255).
   - Possible effect on own-nation NPC targeting during Bifrost phases (I).
3. **[NEUTRAL] V.** `CUser::isHostileTo`:
   - UP's `if (isInTempleEventZone()) return true` (UP :1243-1244) was replaced by nation- and event-room-gated rules for Juraid, BDW, Chaos and Stone (ALPHA :1355-1372).
   - CSW hostility was rewritten (ALPHA :1311-1352), and an event-room check was added (:1289).
   - The Ronark path is identical (different nation and `isInPVPZone`), so MEC-ZON-01 holds.
4. **[NEUTRAL] V.** `isInSafetyArea`: zone 71 is still absent, so MEC-ZON-02 holds.
   - New boxes: Ronark Land **Base** (73) (ALPHA :1443-1445), ZONE_BATTLE3/4 (:1477-1487), and home zones 2-4.
   - The Bifrost and Arena boxes are now gated by nation.
   - UP :1311-1345. ALPHA :1433-1494.
5. **[MAYBE] V/I.** `isAttackable` for NPC_PVP_MONUMENT: UP returned false for the own-nation monument; ALPHA returns true.
   - UP :903-907. ALPHA :909-913.
   - Attackability then depends only on NPC hostility (I).
6. **[NEUTRAL] V.** `Unit::GetMagicDamage` (item elemental bonus / reflect) returns 0 when the target is blinking. There is no blink in zone 71.
   - UP :593. ALPHA :595.
7. **[NEUTRAL] V.** `SetZoneAttributes`:
   - Zone 71 is unchanged (UP :1100-1104; ALPHA :1127-1131), except that `MAX_LEVEL` is now 83.
   - ZF_WAR_ZONE was dropped from Bifrost, Krowaz, Desperation, Hell, Dragon and Juraid.
   - Delos became Siege2 with attack flags. The Stone zones were added. Military zone duplicates were added.
8. **[NEUTRAL] V.** `GetDamage` (physical) differs only in brace formatting (UP :278-308; ALPHA :287-309).
9. **[NEUTRAL] V.** `isInArena` now covers Moradon 2-5 (ALPHA :1395-1398).
10. **[NEUTRAL] V.** `GetDistance(nullptr)` returns 0 (ALPHA :98-99), so a null target counts as "in range". Edge case.

#### Unit.h

1. **[MAYBE] V.** `MAX_LOOT_RANGE` went from 121 (11 m) to 50 (≈7.07 m). UP :21; ALPHA :21.
2. **[NEUTRAL] V.** The transformation state moved from CUser to Unit (ALPHA :190-193, :356-375). Added: `Type7BuffMap` and helpers for temple / quest-event zones (:68-98).

#### MagicInstance.h

1. **[NEUTRAL] V.** Added:
   - `MORAL_ENEMY_PARTY = 37` (:50) and `bIsRunProc` (:86).
   - Hard-coded skill-ID lists (:89-122): `BuffType3()` = 112570/112575/2xxxxx; `LightStunSkills` = charged blade / Light Staff; `ColdSkills` = frozen blade / Ice Staff; `*Not` = id + 80000.
   - `Type7Cancel`, `SendSkillNotEffect`.

#### MagicInstance.cpp (UP 2996, ALPHA 3424)

1. **[NEUTRAL] V.** MP is not charged while the caster is blinking: FLYING (ALPHA :88), IsAvailable (:1112), Type4 (:2062-2065). `ExecuteSkill` refuses non-Type4 skills with id < 300000 while the caster is blinking (:745; UP :656). No blink in zone 71.
2. **[MAYBE] V.** New `SkillUseFail` cases:
   - Players cannot cast ids 300000-399999 (:350-354). 490024 is GM-only (:356-360). 120011/120021/220011/220021 work only in Delos.
   - Casting fails while trading, merchanting or with the store open.
   - Some NPC target types fail: tree, fossil, refugee, partner, border monument (ALPHA :195-230).
   - Prison mana-skill whitelist (:374-388). Fishing buff premium checks (:409-438).
3. **[NEUTRAL] V.** Recast: `int32` → `int64` cast (UP :366; ALPHA :459). The same-second hole (MB-02) is still there. The type gate (MEC-MAG-03) is skipped only for `MAGIC_TYPE4_EXTEND` (:488, :507).
4. **[NEUTRAL] V.** The FREEZE check in prerequisites applies only to player targets (UP :432; ALPHA :527).
5. **[BREAKS] V.** `ExecuteType1` was rewritten (UP :1056-1110 → ALPHA :1144-1244):
   - New AoE path when the target is -1 (:1155-1180).
   - A single target is rejected if it is a blinking player.
   - Per-target `sRange` skip: no damage, no echo, returns false. This now also applies to `UseStanding != 0` skills, which skipped the range check in prerequisites.
   - Weapon durability wears on each skill hit (:1234-1235).
   - **The Type1 echo is suppressed** for 106520/206520/105520/205520 (leg cutting), 106802/206802 (Scream) and 106820/206820 (Shock Stun) (:1240). The bot's `{1,3}` / `{1,4}` result parsing (MEC-MAG-24, `data[3] ∈ {0,-104}`) no longer sees the Type1 packet.
6. **[MAYBE] V.** `ExecuteType2`: weapon wear per hit (:1380-1381). The echo is suppressed for ice shot and lightning shot (108562/108566/2xxxxx) (:1390).
7. **[MAYBE] V.** `ExecuteType3` AoE:
   - The caster is **no longer auto-added for heals** (UP :1275-1276). It is now added only through `UserRegionCheck`: a caster outside a party is accepted, but a party caster is accepted only within `Radius` of the target point (MEC-MAG-18 edge).
   - Arena self-exclusion. Siege weapon fallback (ALPHA :1410-1443).
8. **[MAYBE] V.** `ExecuteType3`: 112570/112575 (critical restore, Past Recovery) skip any player with `m_bType3Flag` set, i.e. any active DoT or HoT (:1480).
9. **[BREAKS] V.** Lightning stun roll (UP :1354-1378 → ALPHA :1501-1527, :1735-1810):
   - UP: lightning only, DirectType 1. If the success roll fails or the resistance roll succeeds, no stun visual.
   - ALPHA: DirectType 0/1, **lightning and ice**. The resistance roll is a *second chance to land* (`<=`).
   - For charged blade, frozen blade, Light Staff and Ice Staff on players, ALPHA sets `nSkillID += 80000` (a member, so later loop iterations are affected), sends a status update, and inserts a real Type4 debuff from `MAGIC_TYPE4[id+80000]`.
   - **Local DB:** none of the 12 `id+80000` rows exist in `MAGIC_TYPE4` (query returned 0), so ExecuteType3 `return false`s after the damage is applied, with no echo.
   - Alters MEC-MAG-09 and the MEC-MAG-24 staff skills.
10. **[MAYBE] V.** Changes by DirectType:
    - DirectType 3 (MP Shell 4901xx) now restores `targetMP / FirstDamage` instead of `FirstDamage` (UP :1408-1409; ALPHA :1557-1559).
    - DirectType 16 (Vampiric Fire 110574) heals the caster by the full amount instead of half (UP :1489; ALPHA :1639).
11. **[MAYBE] V.** Every Type3 spell cast by a mage wears the weapon (`ItemWoreOut(ATTACK, -damage)`, :1819-1820). On heals (`AG_HEAL_MAGIC`, :1823-1827), AI-server packets are now sent, so NPCs may switch aggro to healers.
12. **[BREAKS] V.** Type4 speed/slow/stun debuffs (BuffType 6/40/47) on players (UP :1813-1879 → ALPHA :2081-2165):
    - `bSuccessRate >= 100` always lands.
    - Otherwise a success roll is made, then the resistance roll as a second chance. Resist chance is far below UP's ≈78.7 % / 100 % (MEC-BUF-05).
    - A resisted cast is broadcast with `data[1]=0` (UP always sent `bResult=1`; MEC-MAG-15 / MB-09). Status INFLICT is sent only when the debuff lands.
    - The buff entry still stays in the map (the MB-09 map part is preserved).
13. **[BREAKS] V.** Type4 AoE stealth removal:
    - UP removed stealth from every other-nation player in the 3×3 regions before the region check (UP :1645-1652, MEC-BUF-09).
    - ALPHA removes it only for victims who passed `UserRegionCheck` and `sRange`: in a PK zone with a different nation, in BDW/Juraid, in CSW, or in the arena (ALPHA :1920-1950).
14. **[MAYBE] V.** The per-victim `sRange` skip no longer exempts BUFF_TYPE_HP_MP (UP :1690; ALPHA :1917-1919). Group HP buffs (e.g. Greatness) skip members ≥ `sRange` from the caster.
15. **[MAYBE] V.** Type4 target handling:
    - A single target is rejected only if it is a blinking **player** (ALPHA :1898-1904).
    - AoE with no victims (UP :1660-1678; ALPHA :1876-1895): `MORAL_ENEMY_PARTY` and siege fall back to the caster; `bType[1]==4` returns false **without** an echo (UP sent `SendSkill`).
16. **[MAYBE] V.** One extra `SendSkill()` after the loop for DECREASE_RESIST / DISABLE_TARGETING **and now AC** (BuffType 2: Malice, Defense …) (ALPHA :2172-2177). UP sent it per target, and only for the first two types (UP :1881-1883). Extra EFFECTING broadcast (MEC-BUF-10 parsing).
17. **[MAYBE] V.** Type5 RESURRECTION_SELF no longer requires `m_iLostExp > 0` (UP :2023; ALPHA :2306-2309).
18. **[NEUTRAL] V.** Type6: failing transformation checks **no longer abort** (`//return false;` :2375). Monster transformation robs `nBeforeAction`. Siege transformation changes stats (:2416-2440).
19. **[MAYBE] V.** Type7 now returns true (UP :2225 returned false, MB-10); it adds a Type7 buff map and an NPC-sleep state change (:2463-2550).
20. **[MAYBE] V.** Type8 (UP :2229-2453 → ALPHA :2554-2817):
    - AoE candidates now come from the caster's 3×3 regions, not the whole server (UP :2243-2250).
    - A single target is rejected if dead or blinking, so **WarpType 11 single-target resurrect no longer works**.
    - Summon (12) now also requires a non-blinking target in the same party (:2691-2697). For Moral 4 this is effectively the same as MEC-MAG-21 / MEC-T8-01.
    - Descent (25) now requires the same party (:2763-2771).
    - New WarpType 29 (soccer).
21. **[BREAKS] V.** `ReflectDamage` (Mage Armor, MB-03) (UP :2942-2976 → ALPHA :3358-3404):
    - UP dealt the full `damage` to the attacker.
    - ALPHA runs the armor's proc skill (190573 / 190673 / 190773 for Karus, 2905xx/2906xx/2907xx otherwise) from the wearer onto the attacker, via a `bIsRunProc` MagicInstance. This uses the magic-damage formula and resistances.
    - It runs only if the attacker is a player. Mage Armor is then removed with a notify.
    - The proc skills exist in local MAGIC.
22. **[MAYBE] V.** Type9:
    - Re-casting an active state now returns silently instead of `SendSkillFailed` (UP srv_fail → ALPHA no_result) (ALPHA :2840-2841).
    - Stealth is not allowed in Forgotten Temple.
    - `else if (bStateChange >= 7 || bStateChange <= 8)` is **always true** (:2921, also in Type9Cancel). Any unmatched state, e.g. stealth when `canStealth()` is false, falls into the pet/NPC-spawn branch.
23. **[NEUTRAL] V.** New `SendSkillNotEffect`. Formatting-only changes elsewhere: IsAvailable moral switch, Type5 dead/alive checks, `GetMagicDamage`.
24. **[NEUTRAL] V.** CheckType3Prerequisites: a heal on an NPC outside Delos now returns true early (UP had the Delos test commented out) (ALPHA :584).

#### MagicProcess.cpp (UP 1172, ALPHA 1257)

1. **[MAYBE] V.** An unknown skill ID from a player now **disconnects** the player, with a server-wide notice.
   - UP's condition `nSkillID < 0` is never true for uint32, so UP never kicked. UP :30; ALPHA :31.
   - A bot sending an id missing from MAGIC is kicked.
2. **[NEUTRAL] V.** `UserRegionCheck`: NPC-type exclusions (:120-139), `MORAL_ENEMY_PARTY` treated as AREA_ENEMY (:169), and a quest-zone AREA_ALL case (:187-188).
   - The PARTY_ALL, AREA_ENEMY and summon-180 s logic is unchanged (MEC-MAG-16/18, MEC-T8-02).
3. **[BREAKS] V.** BUFF_TYPE_ATTACK_SPEED_ARMOR (18) also adds `m_bAttackAmount += Attack−100` on grant and subtracts it on removal (ALPHA :468-473, :839-843; UP :406-410, :759-763).
   - Locally only Berserker 106775/206775 uses it (Attack 120, AC −300), so it is **+20 % damage**.
4. **[NEUTRAL] V.** BUFF_TYPE_DAMAGE is applied only to players (ALPHA :375-377).
5. **[NEUTRAL] V.** The FREEZE grant calls `BlinkStart()` (ALPHA :492-498). That is a no-op in zone 71.
6. **[NEUTRAL] V.** SPEED2 (Cold Wave) sets `m_bSpeedAmount` to 100 (players) or 200 (NPCs) instead of scaling it, and removal no longer resets it (ALPHA :586-597, :959; UP :520-521, :878-879). Server-side speed is not enforced anyway (MEC-BUF-06).
7. **[MAYBE] V.** MAGE_ARMOR expiry/removal now sends MAGIC_DURATION_EXPIRED to the client (UP :987 excluded it; ALPHA :1071).
8. **[MAYBE] V.** STATS removal now subtracts the buff amounts (`SetStatBuffRemove`) instead of zeroing (UP :693-701; ALPHA :769-779). `SetStatBuff` is now `+=` (User.h), so values can drift if a grant or remove is unbalanced (I).
9. **[MAYBE] V/I.** `UpdateAIServer(…, pTarget, pCaster)` argument order is fixed. UP passed the caster as the target (UP :623; ALPHA :699; signature MagicProcess.h:12). The AI server now records debuffs on the real target, so NPC behaviour under slow/stun may change.
10. **[NEUTRAL] V.** New BuffTypes FISHING (48) and BATTLE_CRY (171). The "unhandled buff type" printf is commented out.

#### User.cpp (UP 6014, ALPHA 7925) + User.h

1. **[BREAKS] V.** AP formula. After the class branches, ALPHA applies `m_sTotalHit = (m_sTotalHit + additionalAP) * (100 + m_byAPBonusAmount)/100` **again** for every class (ALPHA :2487; UP :2191-2205 apply it once).
   - `additionalAP` = 3 + max(0, STR−150) for non-rogues.
   - Example: STR 255 means about +108 AP more.
2. **[BREAKS] V.** **Standing MP regeneration was removed**: the `MSpChange` in the `USER_STANDING` branch of `HPTimeChange` is commented out (UP :3357; ALPHA :3907). Only sitting regenerates MP.
3. **[BREAKS] V.** `SpeedHackUser`: `nMaxSpeed = 90;` overrides the class limits 45/67/90 (ALPHA :3413; UP :2862-2875). The MEC-MOV-02 kick at speed > 67 for warrior/mage/priest no longer exists.
4. **[BREAKS] V.** `SpeedHackTime` (UP :3581-3607 → ALPHA :4146-4182):
   - Tolerance is now `+15` (UP :3593 `+10`).
   - Warp-back happens only after **3 consecutive** violations (`m_SpeedHackTrial`), and the player is **disconnected at 10**.
   - The counter resets on a good check.
   - Thresholds move to 77.5 / 90.6 / 102.5 m (MEC-MOV-09).
5. **[MAYBE] V.** `SendLoyaltyChange`:
   - NP underflow in a PK zone calls `Home()` (ALPHA :712-718). This is a no-op for a dead victim because Home returns when dead or HP < 50 %.
   - Each PvP kill reward in a PK zone gives the killer **+10000 EXP** (`PVP_BONUS_EXP`) and a **Meat Dumpling** 508216000 (ALPHA :857-866). This grows bot inventory/weight and can level bots up.
   - The monthly NP formula was rewritten.
6. **[MAYBE] V.** `LoyaltyChange`:
   - The rival bonus is **no longer deducted from the victim** (UP :2845 `loyalty_target -= bonusNP`; absent at ALPHA :3383). The victim loses −50 instead of −200 on a rival kill (MEC-DTH-11).
   - NP now changes in all zones except an exclusion list; UP required `isNationPVPZone` or siege (UP :2800-2805; ALPHA :3337-3344).
   - Ronark ini values, and therefore MEC-DTH-03 +64/−50, are unchanged.
7. **[MAYBE] V.** `ExpChange`:
   - Now kicks the user out of the zone when the level is outside the zone's range (ALPHA :1980-1983). Ronark is 35-83.
   - EXP is multiplied by `m_sExpGainAmount`.
   - 10 % jackpot EXP in RecvUserExp (:1793).
8. **[MAYBE] V.** `HPTimeChangeType3`: if the DoT/HoT source unit no longer exists, the tick is skipped **and never expires** (ALPHA :3947-3950). The slot stays used and `m_bType3Flag` stays set (I).
9. **[MAYBE] V.** `m_sMaxWeight` is halved (ALPHA :2460; UP :2179). Carry capacity for pots goes down.
10. **[MAYBE] V.** `Home()` uses the bind object first, the event-zone gate replaces the Chaos/BDW checks, and soccer cleanup was added (ALPHA :4268-4300; UP :3695-3722). The MEC-DTH-10 HP/Kaul/freeze conditions are unchanged.
11. **[MAYBE] V.** `SetStatBuff` is now `+=`, and there is a new `SetStatBuffRemove`. `GetStatBuff` and `GetStatBonusTotal` are now signed. `GetActiveQuestID` semantics changed (User.h).
12. **[MAYBE] V.** `SendTargetHP` now also answers for dead targets (UP :2342; ALPHA :2627).
13. **[MAYBE] V.** `OnDeath`: in party loops, `if (pParty == nullptr) return;` skips `m_sWhoKilledMe` and the `WIZ_DEAD` broadcast (ALPHA :5799-5800, :5636, :5701). Edge case.
    - Otherwise the Ronark PvP flow is the same as UP: rival, anger gauge, LoyaltyChange/Divide, GoldChange, no EXP loss (ALPHA :5728-5827).
    - Krowaz rolling stone / saw blade deaths cost −100 NP and no EXP.
14. **[NEUTRAL] V.** `BlinkStart` now uses an explicit zone list that includes RONARK_LAND (ALPHA :5267-5289). Compared with UP (`canAttackOtherNation` early return plus the `DISABLE_PLAYER_BLINKING` Debug guard; UP :4525-4541), MEC-DTH-08 holds in zone 71.
    - The Debug-build disable is gone. Other PvP zones (Bifrost, Eslant, home zones) now blink, for 10 s.
15. **[NEUTRAL] V.** `isInAttackRange` has an identical body (UP :5013; ALPHA :5941). `GoldChange`, `LoyaltyDivide` (apart from the zone exclusion list) and `GetLoyaltyDivideSource` are identical for zone 71.
16. **[NEUTRAL] V.** `OnDisconnect` / party: the leader promotes `uid[1]`, then non-leaders are removed. The net effect is the same as UP, but a null party causes an early `return` that skips the rest of OnDisconnect (ALPHA :204-215).
17. **[NEUTRAL] V.** `SetMaxHp(1)` now sets the max HP to 14000 (ALPHA :1267-1271). It is called only when leaving the Snow war or Chaos dungeon.
18. **[NEUTRAL] V.** `ItemWoreOut` repair-all now includes weapons, and calls `SetUserAbility` after a repair.
19. **[NEUTRAL] V.** New feature code: genie, monster stone, mining, fishing, flash bonuses, premium stacking, rankings, logging, WIZ_GENIE / CAPTURE / MOVING_TOWER handlers.
20. **[NEUTRAL] V.** The master passives in `HpChange` use `g_pMain->m_byMaxLevel` instead of `MAX_LEVEL` as the upper skill-point bound. Same effect.

#### CharacterMovementHandler.cpp (UP 699, ALPHA 871)

1. **[NEUTRAL] V.** `MoveProcess` core is unchanged: `SpeedHackUser` + `IsValidPosition` only, so MEC-MOV-03 holds.
   - The broadcast is filtered by event room.
   - `AG_USER_MOVE` has an extra transformation-type byte (AI protocol change).
   - `EventTrapProcess` adds −1500 HP from poison gas, in Krowaz only (ALPHA :42-56).
2. **[NEUTRAL] V.** `CanChangeZone`: the zone 71 rule is identical (war closed, or Ardream-type war, and NP > 0) (ALPHA :400-413).
   - `CanLevelQualify` is now enforced but stubbed to `return true`.
   - The Eslant case falls through into Delos.
3. **[NEUTRAL] V.** `ZoneChange`: party leave and rival removal are the same.
   - `m_LastX/Z` is set before the Delos override.
   - `SetZoneAbilityChange` moved to `RecvZoneChange` (Loaded).
   - `AG_ZONE_CHANGE` has an extra event-room field.
4. **[NEUTRAL] V.** `Warp` and `RecvZoneChange` use `RegionNpcInfoForMe` instead of `NpcInOutForMe`.
5. **[MAYBE] V.** `RecvZoneChange` no longer ignores dead users (UP :668-670 removed). On Loaded it re-sends the death animation.
6. **[MAYBE] V.** The `GetUserInfo` (`WIZ_USER_INOUT`) layout is now v1534: cape bytes, `uint8` hair, team colour instead of the helmet flag, a different cospre slot list, and dragon-armor substitution during war. Anything that parses this packet with v1453 layout breaks (protocol, not mechanics).

#### PartyHandler.cpp (UP 637, ALPHA 641)

1. **[NEUTRAL] V.** Invites:
   - Cross-nation parties are allowed in Moradon 1-5 and Forgotten Temple … Lost Temple.
   - The event room must match.
   - Not allowed: Stone zones for quest-event users; CSW Delos across clans (ALPHA :89, :99-108).
   - The zone 71 same-nation / same-zone / level rule is unchanged (MEC-PTY-02).
2. **[NEUTRAL] V.** The party BBS filter also excludes GMs and other event rooms. `PartyRemove` / `PartyDelete` only rename JURAID.

#### Define.h / GameDefine.h

1. **[NEUTRAL] V.** `MAX_LEVEL` 80 → 83 (Define.h:21 → :22). This is the Ronark max level and the ExpChange cap.
2. **[NEUTRAL] V.** `BLINK_TIME` 15 → 10 s. No blink in zone 71.
3. **[NEUTRAL] V.** Battle-type ids renumbered: `SIEGE_BATTLE = 3`, `CLAN_BATTLE` 3 → 4.
4. **[NEUTRAL] V.** New zone ids: Karus 2-4 (3/5/6), El Morad 2-4 (4/7/8), Moradon 2-5 (22-25), Eslant 2-3 (13-16), Stone 1-3 (81-83), Delos Castellan 35, Lost Temple 56. `ZONE_RONARK_LAND 71` and `_BASE 73` are unchanged.
5. **[NEUTRAL] V.** Level minimums: Nation base 1 → 35, Eslant 40 → 60, Bifrost 35 → 70, Ronark Land Base 35 → 45. `MIN_LEVEL_RONARK_LAND` stays 35.
6. **[MAYBE] V/I.** Map trap events: `ZONE_TRAP_INTERVAL` 1 → 2 s and `ZONE_TRAP_DAMAGE` 400 → 500 (`TrapProcess`). Relevant only if the Ronark map has trap events (I).
7. **[NEUTRAL] V.** `RANGE_20M` is now squared (400). It is used only for the arena death-notice radius.
8. **[NEUTRAL] V.** New constants: `SKILLPACKET 80000`, `PVP_BONUS_GOLD`, `MINIRIVALRY_NP_BONUS`, `EVENT_MONUMENT_NP_BONUS` (unused), item ids, dragon-armor ids, BuffTypes 48 and 171.
9. **[NEUTRAL] V.** `CLAN_LEVEL_REQUIREMENT` 20 → 35. The `_KNIGHTS_SIEGE_WARFARE` and event-status structs were reworked.

---

## Ek B — Bot paket düzenleri (alt ajan raporu, `b/packets.md`)

### Bot packet layouts: UP (v1453) vs ALPHA (v1534 download)

Date: 2026-10-08. Read-only analysis. No ALPHA code was run.

Paths:
- **UP** = `scratchpad/upstream1453` (`__VERSION 1453`)
- **ALPHA** = `/mnt/c/dev/fdp1534/alpha/ALPHA KO 1534 PROJE/1-Game Source` (`shared/version.h:3` `__VERSION 1534`)
- **OURS** = `/mnt/c/dev/fdp-merge-final` (main @ `9e6590d3`)

Method: `diff -a -w` with CR stripped, comparing the server handler that reads each client packet and the server builder of each packet the bots parse. Field types were checked in `User.h`, `Unit.h`, `Npc.h` and `Knights.h` of both trees. OURS server builders are byte-identical to UP: the only OURS deltas in these files are `FDP_DAMAGE_TRACE` scopes in `AttackHandler.cpp`/`MagicInstance.cpp`, plus the `m_botSink` hooks.

Notes on scope:
- `BotCore/RoamMonSense.h` is **not in OURS main** (main is 1586 commits behind `gece/*`). It was read from the project dir (`gece/2026-10-02`, line 46). The latest bot tree (`/mnt/c/dev/fdp-kalabalik`, `gece/2026-10-08-kalabalik`) parses the same opcodes with the same layouts. It only adds consumers: `ParseNpcAttackEvent` and `ParseHumanMeleeHit` on WIZ_ATTACK, `HumanHitFromSkill` on skill events, and a WIZ_ZONE_CHANGE opcode flag.
- **WIZ_ITEM_MOVE** is not built by any bot code (checked on main and on every `gece/*` and `bot/*` branch). For the record, `CUser::ItemMove` reads `u8 dir, u32 item, u8 src, u8 dst` in both trees (UP `ItemHandler.cpp:541/548`, ALPHA `:593/600`). SAME.
- `__VERSION` guards: GetUserInfo has **no** `#if __VERSION` block in either tree. The only guards near the bot path are `DatabaseThread.cpp` ReqAllCharInfo (`>=1920` UP, `>=1950` ALPHA) and ALPHA `User.cpp:2668/2689` (loot, `>=1950`/`<1950`). Bots do not use WIZ_ALLCHAR_INFO_REQ or loot. For 1453 and 1534, none of these guards are active.

#### 1. Opcode and sub-opcode values (shared/packets.h)

`diff` of `shared/packets.h`: **no existing value renumbered**.
- ALPHA renames 0x84/0x85/0x8B: `WIZ_PACKET10/11/13` become `WIZ_MOVING_TOWER/CAPTURE/VIPWAREHOUSE`.
- ALPHA adds `WIZ_NATION_CHAT 0x19` (a duplicate of `WIZ_ITEM_LOG 0x19`), `WIZ_GENIE 0x97`, `0x98..0x9B` and `WIZ_HACKSHIELD_GUARD 0xA1`.

| Symbol | UP | ALPHA |
|---|---|---|
| WIZ_LOGIN / WIZ_SEL_NATION / WIZ_SEL_CHAR | 0x01 / 0x05 / 0x04 | same |
| WIZ_MOVE / WIZ_USER_INOUT / WIZ_ATTACK | 0x06 / 0x07 / 0x08 | same |
| WIZ_NPC_INOUT / WIZ_NPC_MOVE / WIZ_ALLCHAR_INFO_REQ | 0x0A / 0x0B / 0x0C | same |
| WIZ_GAMESTART / WIZ_MYINFO / WIZ_LOGOUT | 0x0D / 0x0E / 0x0F | same |
| WIZ_CHAT / WIZ_DEAD / WIZ_REGENE | 0x10 / 0x11 / 0x12 | same |
| WIZ_REGIONCHANGE / WIZ_REQ_USERIN | 0x15 / 0x16 | same |
| WIZ_NPC_REGION / WIZ_REQ_NPCIN / WIZ_WARP / WIZ_ITEM_MOVE | 0x1C / 0x1D / 0x1E / 0x1F | same |
| WIZ_TARGET_HP / WIZ_ZONE_CHANGE / WIZ_STATE_CHANGE | 0x22 / 0x27 / 0x29 | same |
| WIZ_VERSION_CHECK / WIZ_PARTY / WIZ_MAGIC_PROCESS / WIZ_SPEEDHACK_CHECK | 0x2B / 0x2F / 0x31 / 0x41 | same |
| PARTY_CREATE..PARTY_STATUSCHANGE, PARTY_PROMOTE | 1..9, 0x1C | same |
| MAGIC_CASTING/FLYING/EFFECTING/FAIL .. MAGIC_CANCEL2 | 1/2/3/4 .. 13 | same |
| PARTY_CHAT (ChatType) | 3 | same |
| InOutType (IN 1, OUT 2, RESPAWN 3, …) | Define.h:54- | same (Define.h:55-) |
| USER_STANDING / USER_SITDOWN / USER_DEAD | 1 / 2 / 3 | same (ALPHA adds USER_MONUMENT 6) |
| NPC_BAND / MAX_USER / MAX_ID_SIZE / MAX_PARTY_USERS | 10000 / 3000 / 20 / 8 | same (ALPHA adds INVALID_BAND 30000) |
| ATTACK_TARGET_DEAD, LONG_ATTACK | 2, 1 | same |

Serialization: `ByteBuffer.h` is unchanged in behavior. Strings default to a u16 length (`DByte`) and `SByte()` switches to a u8 length. The only changes are that a few ASSERTs became early `return`s. In `Packet.h`, ALPHA adds an unused `m_sOwnerID` field, and `Packet(op, sub)` still appends the sub byte. `KOSocket.h` is identical, so the virtual `Send`/`SendCompressed` that OURS overrides in `CUser` with the `m_botSink` hook exist in ALPHA the same way. ALPHA has no `CUser::Send` override, same as UP.

#### 2. Packet table

Line numbers: UP and ALPHA refer to `GameServer/` unless prefixed. OURS refers to the fdp-merge-final tree.

| Packet | Direction | OURS file:line | UP file:line | ALPHA file:line | Verdict | Difference (UP vs ALPHA) and required OURS change |
|---|---|---|---|---|---|---|
| WIZ_MOVE (client) `u16 x10, u16 z10, u16 y10, i16 speed, u8 echo` | bot builds | Bot/ActionExecutor.cpp:155-156 | CharacterMovementHandler.cpp:15 (MoveProcess 4-54) | CharacterMovementHandler.cpp:15 (4-57) | SAME | Behavior only: ALPHA `SpeedHackUser` forces `nMaxSpeed = 90` (User.cpp:3413), so it is looser than UP (45/67/90). |
| WIZ_MOVE (broadcast) `u16 sid, u16 x, u16 z, u16 y, i16 speed, u8 echo` | bot parses | BotCore/Perception.h:338, :365; Bot/BotSession.cpp:388 | CharacterMovementHandler.cpp:45-47 | CharacterMovementHandler.cpp:47-49 | SAME | ALPHA adds a `type` byte only to the AI-server copy (`AG_USER_MOVE`), not to the client packet. |
| WIZ_USER_INOUT `u16 type, u16 id, [UserInfo]` | bot parses | Perception.h:301 (ParseUserInOut), BotSession.cpp:322 | CharacterMovementHandler.cpp:63-69 | CharacterMovementHandler.cpp:66-73 | **DIFFERENT** (via UserInfo) | The framing is the same, but the UserInfo record differs (next row). |
| **UserInfo record** (`CUser::GetUserInfo`) | bot parses | Perception.h:250-297 (ParseUserInfo) | CharacterMovementHandler.cpp:99-161 | CharacterMovementHandler.cpp:103-283 | **DIFFERENT** | See §3.1: **+5 bytes** in the clan block and **+14 bytes** of equipment (12 records instead of 10). OURS: after `r.Skip(2); // cape id` (Perception.h:271) add `r.Skip(5)` (cape R, G, B, pad u8, flag u8). Change `r.Skip(70)` (Perception.h:288) to `r.Skip(84)`. The byte at :285 is now `m_teamColour`, not the helmet flag. Same size, new meaning. |
| WIZ_REQ_USERIN (client) `u16 n, u16 sid[n]` | bot builds | ActionExecutor.cpp:3461-3464 | User.cpp:1200-1211 | User.cpp:1429-1446 | SAME | ALPHA skips users in a different event room (bots: room 0, no effect). |
| WIZ_REQ_USERIN (reply) `u16 count, count x (u8 0, u16 sid, UserInfo)` | bot parses | Perception.h:316 (ParseUserList), BotSession.cpp:346 | User.cpp:1202-1222; GameServerDlg.cpp:1307-1325, 1347-1376 | User.cpp:1437-1458; GameServerDlg.cpp:1500-1518, 1546-1575 | **DIFFERENT** (via UserInfo) | The framing is the same. Every record is 19 bytes longer, so the list desyncs after the first entry unless ParseUserInfo is fixed. |
| WIZ_REGIONCHANGE (server) | bot parses | Perception.h:499 (ParseRegionList), BotSession.cpp:361-386 | GameServerDlg.cpp:1327-1345: one packet `u16 count, u16 sid[count]` | GameServerDlg.cpp:1520-1544: **three** packets `[u8 0]`, then `[u8 1, u16 count, u16 sid[count]]` (count `put` at offset 1), then `[u8 2]` | **DIFFERENT** | ALPHA adds a sub-opcode and splits the list into begin/list/end. With the current OURS code, the 1-byte `[0]` and `[2]` packets make ParseRegionList return 0. `m_obs.Retain(ids, 0, …)` then **empties the player table** on every region change. The `[1]` packet is read misaligned: count = `0x01 \| lo<<8`. OURS: in the BotSession WIZ_REGIONCHANGE branch, read `u8 sub`. Ignore 0 and 2 (optionally use 0 as "begin" and 2 as "commit"). For 1, call ParseRegionList(data+1, len-1). WIZ_NPC_REGION does **not** change, so keep ParseRegionList itself unchanged for it. |
| WIZ_NPC_REGION `u16 count, u16 id[count]` | bot parses | BotSession.cpp:448-454 | GameServerDlg.cpp:1528-1545, 1578-1607 | GameServerDlg.cpp:1737-1755, 1788-1817 | SAME | — |
| WIZ_REQ_NPCIN (client) `u16 n, u16 id[n]` | bot builds | ActionExecutor.cpp:3570-3573 | User.cpp:1230- | User.cpp:1466-1484 | SAME | ALPHA rejects ids `< NPC_BAND` or `> 30000`. Bots only request NPC ids of 10000 and above, so there is no effect. |
| WIZ_REQ_NPCIN (reply) `u16 count(requested), count x (u16 id, NpcInfo)` | bot parses | Perception.h:889 (ParseNpcList), BotSession.cpp:436 | User.cpp:1230-; GameServerDlg.cpp:1465-1526 | User.cpp:1466-1521; GameServerDlg.cpp:1665-1735 | **DIFFERENT** (via NpcInfo) | The framing is the same, but each NPC record has no name (next row). |
| WIZ_NPC_INOUT `u8 type, u16 id, [NpcInfo]` | bot parses | Perception.h:873, BotSession.cpp:418 | Npc.cpp:90-99 | Npc.cpp:97-106 | **DIFFERENT** (via NpcInfo) | — |
| **NpcInfo record** (`CNpc::GetNpcInfo`) | bot parses | Perception.h:837-869 (ParseNpcInfo) | Npc.cpp:138-155 | Npc.cpp:144-163 | **DIFFERENT** | ALPHA **drops `GetName()`**, the SByte u8-length name between weapon 2 and nation. See §3.2. OURS: delete `if (!r.Str(name, kNpcNameMax)) return false;` (Perception.h:847) and leave `NpcObs.name` empty. Bots only use the NPC name in the BotManager diagnostic dump (BotManager.cpp:2803). Targets use `npc#<id>` keys. A name can come from the K_NPC table by `protoId` if wanted. The ALPHA gate dword can now be `0x02` (Juraid bridge). This is a value only; `gateOpen = (u32 != 0)` still works. |
| WIZ_NPC_MOVE `u16 id, u16 x, u16 z, u16 y, u16 speed10` | bot parses | Perception.h:912, BotSession.cpp:456 | Npc.cpp:73-82 | Npc.cpp:80-89 | SAME | — |
| WIZ_ATTACK (client) `u8 type, u8 result, i16 tid, i16 delay, i16 distance` | bot builds | ActionExecutor.cpp:888-889 | AttackHandler.cpp:10 | AttackHandler.cpp:10 | SAME | Behavior: ALPHA **removed** the client delay and weapon-range check (UP :23-33). `CanCastRHit` and `isInAttackRange` remain. |
| WIZ_ATTACK (broadcast) `u8 type, u8 result, u16 attacker, u16 tid` | bot parses | BotSession.cpp:109-115; RoamMonSense.h:46 (gece); kalabalik ParseHumanMeleeHit | AttackHandler.cpp:90-92; AISocket.cpp:272-274 (NPC→player) | AttackHandler.cpp:108-110; AISocket.cpp:278-280 | SAME | ALPHA RecvNpcAttack also drops blinking targets and other event rooms. |
| WIZ_MAGIC_PROCESS (client) `u8 op, u32 skill, i16 caster, i16 target, i16 data[7]`: cast (CASTING/FLYING/EFFECTING), cancel (MAGIC_FAIL, data[3] = -100), pot and scroll EFFECTING | bot builds | ActionExecutor.cpp:1061-1064, :1622-1626, :1740-1743 (kalabalik also :2361 scroll) | MagicProcess.cpp:16-49 (MagicPacket) | MagicProcess.cpp:17-50 | SAME | Behavior: an unknown skill id from a player now **always disconnects**. UP did this only when id < 0. ALPHA CheckSkillPrerequisites blocks players from skill ids 300000-399999, 490024 (GM only) and 120011/120021/220011/220021 outside Delos. It also fails casts while trading, merchanting or with a store open. |
| WIZ_MAGIC_PROCESS (broadcast) 23 bytes, same order | bot parses | Perception.h:1816 (ParseSkillEvent); BotSession.cpp:120-157 | MagicInstance.cpp:727-744 (BuildSkillPacket) | MagicInstance.cpp:831-848 | SAME | The Type4 broadcast still carries `data[1] = bResult` and `data[3] = duration` (ExecuteType4: UP MagicInstance.cpp:1862, ALPHA MagicInstance.cpp:2144). `data[5]` changes meaning (target speed amount); bots do not read it. ALPHA `SendSkillNotEffect` would encode a failure as EFFECTING, but it is **never called**. |
| WIZ_STATE_CHANGE (client) `u8 type(1), u16 buff` | bot builds | ActionExecutor.cpp:2105-2106 | User.cpp:2676 (StateChange) | User.cpp:3204 | SAME | ALPHA also accepts buff USER_MONUMENT (6) for type 1. |
| WIZ_STATE_CHANGE (broadcast) `u16 id, u8 type, u32 buff` | bot parses | BotSession.cpp:172-179 | User.cpp:2743/2783-2784 | User.cpp:3271/3320-3321; Npc.cpp:350-351 (NPC, same layout) | SAME | ALPHA adds a type 11 (team colour) case. |
| WIZ_TARGET_HP (client) `u16 uid, u8 echo` | bot builds | ActionExecutor.cpp:2251-2252 | User.cpp:326-332 | User.cpp:352-358 | SAME | — |
| WIZ_TARGET_HP (reply) `u16 tid, u8 echo, i32 maxHp, i32 hp, u16 dmg` | bot parses | Perception.h:397; BotSession.cpp:185-215 | User.cpp:2322-2353 (2350-2351) | User.cpp:2607-2638 (2635-2636) | SAME | Behavior: ALPHA also answers for a **dead** player (UP returned nothing). ParseTargetHp accepts hp 0, so this is harmless and arguably better. |
| WIZ_DEAD `u16 id` | bot parses | BotSession.cpp:163, :399, :465 | Unit.cpp:959-961 | Unit.cpp:965-967 | SAME | — |
| WIZ_REGENE (client) `u8 type` | bot builds | ActionExecutor.cpp:2407-2408 | User.cpp:304-305 | User.cpp:330-331 | SAME | Behavior: the loyalty-0 kick from a PK zone now also runs after a skill resurrect (UP only on a normal respawn). |
| WIZ_REGENE (reply) `u16 x, u16 z, u16 y` | bot parses | BotSession.cpp:220-226 | AttackHandler.cpp:191-193 | AttackHandler.cpp:211-213 | SAME | After the reply, both trees send RegionUserInOutForMe, so ALPHA sends the three-part REGIONCHANGE here too (see above). |
| WIZ_PARTY client: CREATE/INSERT `u8 sub, str16 name`; PERMIT `u8 sub, u8 ok`; REMOVE/PROMOTE `u8 sub, u16 sid` | bot builds | ActionExecutor.cpp:2585-2586, 2729-2730, 2884-2885, 2983-2984, 3141-3142 | PartyHandler.cpp:6-50 | PartyHandler.cpp:6-49 | SAME | ALPHA adds invite refusals for: a different event room, Moradon2-5 counted as neutral zones, monster stone, and CSW in Delos with a different clan. |
| WIZ_PARTY server: PERMIT invite `u8 2, u16 sid, str16 name`; INSERT error `u8 3, i16`; member record `u8 3, u16 sid, u8 flag, str16 name, i16 maxHp, i16 hp, u8 lvl, u16 cls, i16 maxMp, i16 mp, u8 nation`; REMOVE `u8 4, u16 sid`; DELETE `u8 5`; HPCHANGE `u8 6, u16 sid, i16 maxHp, i16 hp, i16 maxMp, i16 mp` | bot parses | Perception.h:1236 (ParsePartyEvent); BotSession.cpp:235-283 | PartyHandler.cpp:157-164, 233-260, 315-333, 393-395, 433-434; User.cpp:2023-2030 | PartyHandler.cpp:160-167, 236-260, 318-336, 396-398, 436-437; User.cpp:2303-2310 | SAME | HP/MP types are unchanged (`short m_iMaxHp`, `int16 m_sHp`). |
| WIZ_CHAT (client) `u8 type(PARTY_CHAT), str16 text` | bot builds | ActionExecutor.cpp:3344-3345 | ChatHandler.cpp:93, 104 | ChatHandler.cpp:104, 115 | SAME | — |
| WIZ_CHAT (server) `u8 type, u8 nation, i16 sid, str8 name, str16 msg` | bot parses | BotSession.cpp:289-310 | ChatHandler.h:8-21 (Construct) | ChatHandler.h:8-21 | SAME | ALPHA GameStart(1) sends extra GENERAL/PUBLIC notices with **the bot's own sid** as sender (User.cpp:1225-1234). These do not match the PARTY_CHAT plus hash echo check, so there is no false positive. |
| WIZ_WARP (server) `u16 x, u16 z` | bot parses | BotSession.cpp:476-481 | CharacterMovementHandler.cpp:642-644 | CharacterMovementHandler.cpp:807-809 | SAME | Behavior: after a Warp, ALPHA sends **WIZ_NPC_REGION** (ids only, :820) instead of **WIZ_REQ_NPCIN** (full records). The bot's existing pending-id path then requests the records. |
| WIZ_SPEEDHACK_CHECK (client) `u8 0, f32 clock` | bot builds | ActionExecutor.cpp:3660-3661 | User.cpp:3581 (payload unread, `#if 0`) | User.cpp:4146 (payload unread) | SAME | Behavior: ALPHA tolerance is +15 (UP +10). It warps back only after 3 consecutive violations and **disconnects after 10**. |
| WIZ_SEL_CHAR (bot → DB queue) `str16 charName, u8 init` | bot builds | BotManager.cpp:3861-3862 | DatabaseThread.cpp:96, 247-264 (ReqSelectCharacter) | DatabaseThread.cpp:96, 254- | SAME | ReqSelectCharacter is byte-identical. It depends on ALPHA's DB loaders (`LoadUserData` etc.), whose schema was not checked here. |
| WIZ_SEL_CHAR (reply) `u8 result, [u8 zone, u16 x, u16 z, u16 y, u8 victory]` | bot parses (byte 0) | BotSession.cpp:105-106 | CharacterSelectionHandler.cpp:134-~225 | CharacterSelectionHandler.cpp:135-229 (146-147, 197-199) | SAME | ALPHA adds `isBanned()` (authority 255) → Disconnect, `GetRace()==0` → kick, and kicks for monster-stone or CSW zones, and uses `m_byMaxLevel`. Port note: OURS skips `SetLogInInfoToDB` for bots (OURS CharacterSelectionHandler.cpp:190); ALPHA calls it unconditionally (:195). |
| WIZ_GAMESTART (client) `u8 1` / `u8 2` | bot builds | BotManager.cpp:3344, 3360 | CharacterSelectionHandler.cpp:257- | CharacterSelectionHandler.cpp:263-330 | SAME | ALPHA phase 1 also sends SendPremiumInfo and TopSendNotice. |
| WIZ_LOGIN / WIZ_VERSION_CHECK / WIZ_ALLCHAR_INFO_REQ | not used (bots set `m_strAccountID` directly) | BotManager.cpp:3853-3858 | LoginHandler.cpp, DatabaseThread.cpp:184-200 | LoginHandler.cpp, DatabaseThread.cpp:199-213 | n/a | Bots bypass the version and login handshakes. HandlePacket gating (crypto → account → `m_bSelectedCharacter`) is the same (UP User.cpp:217-, ALPHA :244-). ALPHA LoginProcess dropped WordGuard. |

#### 3. Byte layouts of the DIFFERENT records

##### 3.1 UserInfo (`CUser::GetUserInfo`) — UP CharacterMovementHandler.cpp:99-161 vs ALPHA :103-283

```
field                         UP (1453)                     ALPHA (1534)
name                          SByte str (u8 len)            same
nation u8, clanId i16, fame u8 same                         same
-- clan block, no clan --     u32 0, u16 0, u8 0, u16 -1    u16 0, u8 0, u32 0, u16 -1, u16 0, u8 0, u16 0
                              (= 9 bytes)                   (= 14 bytes)
-- clan block, in clan --     u16 alliance, str8 name,      u16 alliance, str8 name, u8 grade, u8 ranking,
                              u8 grade, u8 ranking,          u16 markVer, u16 cape, u8 R, u8 G, u8 B, u8 0,
                              u16 markVer, u16 cape          u8 2
                              (= 9 + n)                     (= 14 + n)
level u8, race u8, class u16  same                          same
x10, z10, y10 u16             same                          same
face u8, hair u8              m_nHair is uint8              uint8(m_nHair) (m_nHair is uint32 now, cast)
resHpType u8, abnormal u32    same                          same
needParty u8, authority u8    same                          same
partyLeader u8, invis u8      same                          same
1 byte                        m_bIsHidingHelmet             uint8(m_teamColour)
direction i16                 same                          same
chicken u8, rank u8,          same                          same
knightsRank i8, personalRank i8
equipment (u32, i16, u8) x N  N = 10: BREAST LEG HEAD GLOVE  N = 12: BREAST LEG HEAD GLOVE FOOT SHOULDER
                              FOOT SHOULDER RIGHTHAND        RIGHTHAND LEFTHAND CRIGHT CWING CHELMET CLEFT
                              LEFTHAND CTOP CHELMET (70 B)   (84 B). During war (isWarOpen and not BATTLE3),
                                                             item ids are swapped to class dragon-armor ids
                                                             (value only, size unchanged).
zone u8                       same                          same
```
`GetItem()` never returns nullptr for these slots (it returns `&m_sItemArray[pos]`), so N is always fixed. Net: **+19 bytes per record**.

OURS ParseUserInfo change (Perception.h:250-297): after line 271 `r.Skip(2); // cape id` add `r.Skip(5); // cape R,G,B, pad, cape flag (1534)`. Change line 288 `r.Skip(70)` to `r.Skip(84)` and update the comment to 12 records. Comment line 285: the byte is team colour. The no-clan branch is the same size as the clan branch in both versions, so the parser keeps a single path.

##### 3.2 NpcInfo (`CNpc::GetNpcInfo`) — UP Npc.cpp:138-155 vs ALPHA Npc.cpp:144-163

```
UP:    i16 protoId, i16 pid, u8 type, i32 sellGroup, i16 size, i32 w1, i32 w2, str8 NAME, u8 nation, u8 level,
       u16 x, u16 z, u16 y, u32 gate, u8 objType, u16 0, u16 0, i8 dir
ALPHA: i16 protoId, i16 pid, u8 type, i32 sellGroup, i16 size, i32 w1, i32 w2,           u8 nation, u8 level,
       u16 x, u16 z, u16 y, u32 gate(0x02 = Juraid bridge), u8 objType, u16 0, u16 0, i8 dir
```
OURS ParseNpcInfo change (Perception.h:837-869): remove the `r.Str(name, kNpcNameMax)` read (line 847) and leave `out.name` zeroed. The rest of the order is unchanged.

##### 3.3 WIZ_REGIONCHANGE — UP GameServerDlg.cpp:1327-1345 vs ALPHA :1520-1544

```
UP:    [u16 count][u16 sid x count]                                (one packet)
ALPHA: [u8 0]                                                      (packet 1, "reflesh")
       [u8 1][u16 count][u16 sid x count]                           (packet 2, count put at offset 1)
       [u8 2]                                                      (packet 3, end)
```
OURS change (BotSession.cpp:361-386): gate on `data[0]`. For 1, parse `data+1, len-1` with the existing ParseRegionList and run Retain and pending. For 0 and 2, do nothing (or use 0 and 2 as begin and commit). Do not apply this to WIZ_NPC_REGION.

#### 4. Other findings that affect a port but are not layout changes

- Hook points: OURS overrides `CUser::Send` and `CUser::SendCompressed` (OURS User.cpp:29-70). ALPHA has the same virtual base (`shared/KOSocket.h:33-34` identical), so the hook ports as-is.
- Unit tests and fixtures (Tests/BotCoreTests) that encode 10-record UserInfo, NPC names or REGIONCHANGE without a sub-opcode must be updated with the parser changes.
- ALPHA's behavior is looser or different in R-attack timing (no client delay check), speed checks (max 90, disconnect after 10 SpeedHackTime violations), skill-id rules (unknown id → disconnect) and post-Warp NPC refresh (NPC_REGION instead of REQ_NPCIN). None of these change a byte layout.

---

## Ek C — Güvenlik incelemesi (alt ajan raporu, `b/sec.md`)

### ALPHA KO 1534 — security review (backdoor hunt), read-only

Scope: `ALPHA KO 1534 PROJE/1-Game Source` (paths below are relative to it unless prefixed `Server-Files/` or `DB/`), `Server-Files/*.ini`, shipped binaries (strings + PE header only, nothing executed), and procedure text carved from `DB/KN_online.bak` (only T-SQL module text extracted; no table rows read).
Baselines: UP = open-source 1453 (`upstream1453`), KOD = KODevelopers-1534 (2017).
Method: per-line "normalized line exists in UP/KOD?" tagging of every ALPHA source line (tools `sec_grep.py`, `sec_removed.py`); targeted reads of all new/changed handlers; vcxproj inspection to see what is actually compiled.
Credentials/user names are never printed (shown as `<redacted>` / `<user>`).

**Bottom line:** no classic backdoor was found (no master password, hidden GM grant, remote shell, phone-home, license kill switch or obfuscated payload). The serious risks are **ALPHA-only exploitable packet handlers** (character hijack via character seal, client-side teleport, remote crash), **plaintext password logging** (inherited, and the shipped log already contains passwords), **default DB credentials**, and **untrusted prebuilt binaries/libs**.

#### Findings

| # | Sev | file:line | Finding | Lineage | Status |
|---|-----|-----------|---------|---------|--------|
| 1 | HIGH | GameServer/SealHandler.cpp:222,228,230,238,265,321; GameServer/DBAgent.cpp:2370-2378 (LOAD_SEAL_USER), 2412-2426 (INSERT_USER_SEALED), 2498-2508 (UPDATE_ACCOUNT_CHAR); dispatch UpgradeHandler.cpp:55-56 | **Character hijack by name.** `CharacterSealProcess` / SealSuccess takes `RecvCharID` from the packet (:222) and only checks the *requester's own* seal password (:224,:228). It never checks that the character belongs to `m_strAccountID`; `LOAD_SEAL_USER(@strCharID)` and `INSERT_USER_SEALED` take no account (proc text in DB backup confirms). The attacker gets a Cypher Ring holding any character's data. Unsealing (:321) runs `UPDATE_ACCOUNT_CHAR(attackerAccount, victimChar, AccSlot)`, which writes the victim's name into the attacker's ACCOUNT_CHAR slot. `LOAD_USER_DATA` only checks ACCOUNT_CHAR membership, so the attacker can then log in as the victim's character. | ALPHA-only | verified statically (code + proc text); not exercised |
| 2 | HIGH | GameServer/GenieHandler.cpp:153-209 (case 2 :172-181, case 1 :159-171, case 16 :183-196); dispatch User.cpp:493-495; opcode shared/packets.h:135 (`WIZ_MOVING_TOWER 0x84`) | **Unauthenticated teleport.** `HandleMovingTower` lets any player `Warp(x,z)` to client-chosen coordinates (case 2) or onto the targeted player (case 1), with no state/zone/GM check. This bypasses the GM-only `WIZ_WARP` gate (User.cpp:339-341). `Warp()` (CharacterMovementHandler.cpp:791-802) only checks that the position is valid on the map. Case 16 sets a transform abnormal for free. | ALPHA-only | verified statically |
| 3 | HIGH | LogInServer/LoginSession.cpp:143 | **Plaintext password logging.** The format has 2×`%s` for 3 arguments (`account, password, sAuthMessage`), so the password lands in the `Authentication=` field of `Logs/Login_<d>_<m>_<y>.log`. The shipped `Server-Files/Logs/Login_30_4_2023.log` (8 lines) already contains non-status values in that field (lengths 7/14), i.e. the packager's passwords (shape-checked, values not printed). | inherited (UP LoginSession.cpp:139, KOD :142) | verified |
| 4 | MEDIUM | GameServer/SealHandler.cpp:295-299,327; GameServer/User.h:611-617 | **Remote crash.** In the unseal path `InvSlot` is unchecked. `GetItem(SLOT_MAX+InvSlot)` returns `nullptr` when pos > 74 (`INVENTORY_TOTAL`), and `pDstItem->nUserSeal` (:299) then dereferences null. Any logged-in player can send it. `GetItem` also has an off-by-one: pos == 74 returns one past the end. `AccSlot` (:321) is also unchecked. | ALPHA-only (UP `GetItem` had no bound at all) | verified statically; not run |
| 5 | MEDIUM | GameServer/CharacterMovementHandler.cpp:867-871; enum shared/packets.h:229 | **`ZoneMilitaryCamp`.** `WIZ_ZONE_CHANGE` sub-op 4 reads a client-chosen zone and calls `ZoneChange(zone, GetX(), GetZ())`. Any player can cross-zone teleport without an NPC/gate to any zone that `CanChangeZone` allows (:296+, level/nation only), arriving at the current x/z. Relevant to Ronark Land. | ALPHA-only | verified statically |
| 6 | MEDIUM | GameServer/EventHandler.cpp:753-811; dispatch User.cpp:490-492 (`WIZ_CAPTURE 0x85`) | `HandleCapture` has no zone, BDW-active or monument-proximity check. `m_tBorderCapure` (User.h:125) is never initialized; it is only set at NPCHandler.cpp:507 and Npc.cpp:713. Any player anywhere can trigger a BDW monument capture (scores +2, loyalty, zone broadcast). | ALPHA-only | verified statically |
| 7 | MEDIUM | GameServer/GameServerDlg.cpp:243-244,249-250; AIServer/ServerDlg.cpp:848-849; LogInServer/LoginServer.cpp:140-141 | **Hardcoded default DB login/password** as ini fallbacks (UID/PWD `<redacted>`). The shipped ini values are identical: GameServer.ini:63-64,67-68; AIServer.ini:3-4; LogInServer.ini:12-13. Known SQL credential. | ALPHA-only (UP/KOD used placeholders) | verified |
| 8 | MEDIUM | GameServer/proj-GameServer.vcxproj:226; AIServer/proj-AIServer.vcxproj:418; LogInServer/proj-LogInServer.vcxproj:409 | **Supply chain.** The `Debug|x64` config links the prebuilt `..\..\Server-Files\shared.lib` / `Lua.lib`, so building one project alone uses the shipped untrusted libs. The shipped `.exe` files are Debug x64 builds (link time 2023-04-29, PDB `C:\Users\<user>\Desktop\ALPHA KO 1534 PROJE\Server-Files\*.pdb`). Recommendation: delete all shipped `.exe/.lib/.pdb/.idb/.ilk/.obj` and rebuild everything from source. | ALPHA-only | verified |
| 9 | LOW | Server-Files/LogInServer.exe vs LogInServer/LoginServer.cpp:166 | **Binary ≠ source.** LogInServer.exe contains default server name `TEST|Server 1`; the source has `ALPHA ONLINE WORLD|Pre-Alpha`, so the exe was built from another revision. Strings of GameServer.exe, AIServer.exe and LogInServer.exe show no URL, foreign IP or download/HTTP/shell API. The only IPs are 127.0.0.1/0.0.0.0, plus the `ftp.yoursite.net` placeholder. DLL names: KERNEL32, WS2_32, ODBC32, IPHLPAPI. CreateProcessW/RegOpenKeyExW/MoveFileExW/DeleteFileW come from the static debug CRT (`system("pause")`) and Lua os lib. | ALPHA | verified (strings/PE header only) |
| 10 | LOW | Server-Files/Map/WarpGateEditor.exe | Third-party .NET tool (262144 B, link time 2013-03-25). Its PDB path contains `CryptoObfuscator_Output`, so it is an obfuscated binary. Not analysed; do not run. | ALPHA package | verified (header only) |
| 11 | LOW | DB/KN_online.bak (procs ACCOUNT_LOGIN, several historical variants) | Login procs auto-register unknown accounts: `INSERT INTO TB_USER(strAccountID,strPasswd)` on first login. One variant is gated by `KOHSTR_GAME_OPTIONS.AutoRegister`. Passwords are compared in plaintext. No literal master-password/account compare found in any variant. | DB (not in UP source) | which variant is active is **inferred** (needs restore) |
| 12 | LOW | GameServer/GenieHandler.cpp:82-83,102-110 | `GenieStart` doesn't require `m_GenieTime>0` or a potion. `GenieAttackProgress` keeps processing after `GenieStop()` when time==0, so the auto-hunt flag is free. Genie options save/load is bounded (100 B, safe `read`). | ALPHA-only | verified statically |
| 13 | LOW | GameServer/MerchantHandler.cpp:873-893, 850-855 | From Moradon (gate :788-791), a player can teleport to any merchanting player's position in any zone without consuming the Menissia item. `MerchantListSend` dereferences `pUser` without a null check (:852-854), a race crash. | ALPHA-only | verified statically |
| 14 | LOW | GameServer/ChatHandler.cpp:414-450 | `+prison` has no internal `isGM()`; it relies on the dispatcher gate at :120, which is OK. It uses the GM's nation, not the target's, for the start position (:446-447). | ALPHA-only | verified |
| 15 | INFO | shared/HardwareInformation.cpp:31; GameServerDlg.h:553; AIServer/ServerDlg.h:142; LogInServer/LoginServer.h:57; shared/version.h:4 | **HWID/"Lisans".** The HWID hash (MAC+CPUID+C: volume serial) is inherited; ALPHA adds a `Lisans HardwareInformation Number` printf and `m_HardwareIDArray` members. The arrays are never filled and `IsValidHardwareID` is never called, so this is dead code with no license lock. `__GUARD_VERSION 2025` is defined but unused. No date checks or kill switches found (searched GetYear/tm_year/UNIXTIME literals, exit/abort/TerminateProcess). | inherited class (UP/KOD; UP's check is commented out at GameServerDlg.cpp:89-93) + ALPHA-only printf/members | verified |
| 16 | INFO | whole tree | **No outbound network.** The only outbound connect is GameServer to AIServer at the ini IP (GameServerDlg.cpp:910, 127.0.0.1). No http/ftp/IP literals other than 127.0.0.1/0.0.0.0. No URLDownloadToFile/WinInet/WinHttp/ShellExecute/WinExec/popen. `system("pause")` only in */main.cpp. | inherited | verified |
| 17 | INFO | GameServer/DBAgent.cpp (all `Execute`), shared/database/OdbcRecordset.cpp:15-30 | **No SQL injection.** Every SQL call uses ODBC `?` parameters or `%d` numeric formatting; there is no `%s`/string concatenation of packet data into SQL. No ExecDirect with user strings. | inherited + ALPHA | verified |
| 18 | INFO | GameServer/User.h:354; ChatHandler.cpp:120; User.cpp:4033-4036, 339-341 | **No hidden GM / name bypass.** `isGM()` = `Authority==0` (unchanged). Chat commands run only if `isGM()`; OperatorCommand and WIZ_WARP are GM-gated. No literal compares on account/char names (only gate-NPC names, User.cpp:5037-5052). No `m_bAuthority` assignment outside inherited ban/mute code. Backup procs: no Authority grants, no xp_cmdshell/OPENROWSET/linked servers. `dt_*` procs use sp_OACreate (legacy SQL2000 VSS procs). | inherited | verified |
| 19 | INFO | DB/KN_online.bak procs `DB_SIFIRLA`, `DB_ROLLBACKFORMAT` | Destructive admin procs (TRUNCATE ACCOUNT_CHAR, KNIGHTS, MAIL_*, …), not called from code. Do not run. Also `KRAL_EKLE`/`King_user` set a king by name (admin utilities). | DB | verified (proc text) |
| 20 | INFO | GameServer/ChatRoomHandle.cpp:159-167,191 | Not in any vcxproj (dead). If enabled: null dereference on a missing room (:167) and any user can delete any room (:191). | ALPHA-only | verified |
| 21 | INFO | AIServer/NpcOrjinal.cpp, shared/Socket*Linux*, ListenSocketLinux.h, SocketMgrWin32.* | Not compiled (absent from vcxproj). Contents: an older Npc.cpp copy and Burlex/twostars socket code. No network/exec additions. | ALPHA-only files | verified |
| 22 | INFO | Server-Files/Logs/** ; */*.recipe, *.log, *.pdb | The package ships runtime logs (Login with passwords, Chat, Item, Merchant, DeathNpc) and build artefacts that reveal the builder's Windows user path. Treat as personal data; do not redistribute. | ALPHA package | verified (shape only) |
| 23 | INFO | GameServer/LuaEngine.cpp:172 | `luaL_openlibs` exposes os/io/package to quest Lua. Scripts are server-side. None of the 601 quest files use `os.`/`io.`/`require`/`loadstring`/`dofile`, and none call GiveCash/GiveKnightCash. Lua src diffs vs UP are official 5.2.3→5.2.4 patches. | inherited | verified |
| 24 | INFO | LogInServer/LoginServer.cpp:54-63; AIServer/ServerDlg.cpp:86 | LoginServer listens on 10 consecutive ports (15100-15109; ALPHA-only). AIServer listens on 0.0.0.0:10020 with no auth (inherited), so firewall it. | mixed | verified |
| 25 | INFO | shared/JvCryption.cpp:11 | Client crypto key = the public 1453/1534 key (KOD active; UP has it commented). | inherited | verified |
| 26 | INFO | GameServer/Unit.cpp:343-349 | `GAME_MASTER_R_HIT_DAMAGE` (ini 30000): fixed GM damage vs NPCs only. | inherited (UP) | verified |

##### Server-Files ini (no credential values shown)
- GameServer.ini: `[AI_SERVER] IP=127.0.0.1` (:2), `[ZONE_INFO] SERVER_IP_00=127.0.0.1` (:90). `[ODBC]` ACCOUNT_UID/PWD and GAME_UID/PWD present (:61-68). Credentials present and equal to the source defaults.
- AIServer.ini: `[ODBC] GAME_UID/PWD` present (:2-4); no host keys.
- LogInServer.ini: `[DOWNLOAD] URL=ftp.yoursite.net` (:3, placeholder), `LANIP_00/SERVER_00=127.0.0.1` (:24,:27). `[ODBC] UID/PWD` present (:11-13).
- No external hosts or IPs in any ini.

##### Shipped binaries (not executed)
| file | size | note |
|---|---|---|
| Server-Files/GameServer.exe | 6,126,080 | PE32+ x64 Debug, link time 2023-04-29 21:12 UTC |
| Server-Files/AIServer.exe | 3,650,560 | same build |
| Server-Files/LogInServer.exe | 2,735,104 | same build; default-string mismatch vs source (#9) |
| Server-Files/shared.lib | 8,952,410 | prebuilt static lib, linked by Debug|x64 (#8) |
| Server-Files/Lua.lib | 1,667,998 | prebuilt static lib |
| Server-Files/Map/WarpGateEditor.exe | 262,144 | .NET, CryptoObfuscator (#10) |
| + *.pdb, *.idb, 1-Game Source/**/Debug, *.ilk, *.obj, *.pch | | build artefacts |

#### Chat commands (complete)

Dispatch: client chat runs `ProcessChatCommand` **only when `isGM()`** (ChatHandler.cpp:120, prefix `+`). The console table (prefix `/`) is reachable only from the server console (GameServerDlg.cpp:1819-1830 via ConsoleInputThread), not from packets. No other chat-text parser exists. KOD's user table uses configurable names (`g_pMain->Command1..43`), so for user commands "ALPHA-only" means absent from UP. KOD's extra user handlers (AccessGM/RemoveGM/IpBanned/BannedAccount/ItemOnline…) are **not** in ALPHA.

##### User (`+`, in-game, GM-only via :120)
| cmd | handler (ChatHandler.cpp line) | internal isGM | ALPHA-only? | effect |
|---|---|---|---|---|
| test | HandleTestCommand :396 | Y | no | none |
| prison | HandlePrisonCommand :414 | **N** (gate only) | **yes** | ZoneChange target to prison |
| zonechange | HandleZoneChangeCommand :578 | Y | no | self teleport |
| monsummon | HandleMonsterSummonCommand :604 | Y | no | spawn monsters |
| npcsummon | HandleNPCSummonCommand :634 | Y | no | spawn NPC |
| monkill | HandleMonKillCommand :652 | Y | no | kill NPC |
| open1..open6 | HandleWar1..6OpenCommand :741-778 | Y | no | open war |
| captain | HandleCaptainCommand :1134 | Y | no | war captains |
| snowopen | HandleSnowWarOpenCommand :784 | Y | no | snow war |
| siegewarfare | HandleSiegeWarOpenCommand :791 | Y | **yes** (vs UP) | open CSW |
| close | HandleWarCloseCommand :798 | Y | no | close war |
| np_change | HandleLoyaltyChangeCommand :805 | Y | no | change player NP |
| exp_change | HandleExpChangeCommand :836 | Y | no | change player exp |
| noah_change | HandleGoldChangeCommand :867 | Y | renamed (UP `gold_change`) | change player gold |
| exp_add | HandleExpAddCommand :904 | Y | no | XP event |
| np_add | HandleNPAddCommand :923 | Y | no (UP handler name differs) | NP event |
| money_add | HandleMoneyAddCommand :941 | Y | no | coin event |
| permitconnect | HandlePermitConnectCommand :958 | Y | no | unban (DB Authority=1) |
| tp_all | HandleTeleportAllCommand :987 | Y | no | send all home |
| summonknights | HandleKnightsSummonCommand :1046 | Y | no | summon clan |
| warresult | HandleWarResultCommand :453 | Y | no | set war winner |
| resetranking | HandleResetPlayerRankingCommand :1087 | Y | no | reset rankings |
| give_item | HandleGiveItemCommand :528 | Y | no | give item (help text says `+mind_start`, a leftover name) |
| nt | HandleNtsCommand :404 | Y | **yes** | in-memory nation flip of the GM (CharacterHandler.cpp:6) |

##### Console (`/`, server console only)
notice :698, noticeall :707 (KOD, not UP), kill :716, open1..6 :742-778, snowopen :785, siegewarfare :792 (KOD), close :799, down :1109 (shutdown), discount :1116, alldiscount :1122, offdiscount :1128, captain :1135, santa :1143, offsanta/offangel :1149, angel :1155, permanent :1161, offpermanent :1199, reload_notice :1208, reload_tables :1229, reload_magics :1287, reload_quests :1314, reload_ranks :1323, count :1329, permitconnect :959, **give_item :479 (ALPHA-only in both baselines)**, exp_add :905, np_add :924, money_add :942 (KOD), tp_all :1018 (KOD), warresult :454. KOD-only console commands absent from ALPHA: exp_change, gold_change, np_change, noticeclan/ally/captain/party/pm.

#### Not covered / limits
- The DB backup was not restored. Which ACCOUNT_LOGIN variant is current, and whether pre-seeded GM accounts (USERDATA.Authority=0) exist, were **not** checked (personal-data tables). After a restore, check with count-only queries.
- No disassembly of binaries; the binary↔source equivalence is string-level only.
- Gameplay dupes/balance were reviewed only where they intersect privilege or teleport; the review is not exhaustive.
