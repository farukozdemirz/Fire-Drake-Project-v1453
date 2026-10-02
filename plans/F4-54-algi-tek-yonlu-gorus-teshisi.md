# F4-54: Gözlem tablosunda tek yönlü görüş (KI-DEG-01): sayaçlı teşhis ve kök neden düzeltmesi

| Alan | Değer |
|---|---|
| Durum | KAPANDI (2026-10-02, gece/2026-10-02, merge cf22667) |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2; kapı G4) |
| Branch | `bot/F4-54 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | F4-12, F4-13, F4-14, F4-15 (görünür oyuncu/NPC tabloları ve bölge değişimi istekleri) — `KAPANDI` |
| İlgili gereksinim / kabul | KI-DEG-01, Q-27; `docs/03` §16 (3×3 bölge bilgisi); `docs/reports/degerlendirme-2026-10-02.md` DEG-04 |
| Tahmini büyüklük | S–M (teşhis sayaçları + koşullu düzeltme; 4 kod dosyası) |
| Hazırlayan / tarih | Claude / 2026-10-02 (değerlendirme) |

---

## 1. Amaç

F4-18 Tur 2 doğrulamasında (plan dosyası, "Bulgular" madde 3) görüş tablosu (`ObsTable`) bazı bot çiftlerinde **tek yönlü** çıktı: `BotMF_K` aynı bölgede 5 m'deki `BotWP_K`'yi görmüyor, `BotWP_K` `BotMF_K`'yi görüyordu (`member … out_of_view` bu yüzdendi). Neden teşhis edilmedi ve kayıt altına alınmadı. Karar katmanı ve takım görünümü (hedef seçimi, `TeamView.inView`, peel) bu tabloya dayanır; sessiz bir görünmezlik hatası F6'da "bot neden saldırmıyor" sorusuna dönüşür. Bu plan (1) hata ayıklama **sayaçları** ekler, (2) tekrar üretir, (3) kök nedeni bulur ve **yalnızca** `BotCore/Perception.h` / `BotSession` içindeyse düzeltir; sunucu kodunda ise düzeltmez, `KNOWN_ISSUES` kaydına yazar (üretim sunucusu davranışı bu planın kapsamı dışındadır).

## 2. Bağlam (okunması zorunlu)

- `plans/F4-18-algi-takim-gorunumu.md` Doğrulama Tur 2, Bulgular madde 3 (gözlem) ve S1–S5 çalışma zamanı kaydı.
- `GameServer/Bot/BotSession.cpp:195-262` (`WIZ_USER_INOUT`/`WIZ_REQ_USERIN`/`WIZ_REGIONCHANGE`/`WIZ_MOVE`/`WIZ_DEAD` blokları): **ayrıştırma başarısız olursa paket sessizce atılır, sayaç yok**; `WIZ_REGIONCHANGE` dalı `m_obs.Retain(ids, n, 0xFFFF)` ile listede olmayan her birimi **siler**.
- `BotCore/Perception.h` `ParseUserInOut`, `ParseUserInfo` (kayıt alanları: ad `u8` uzunluklu, klan bloğu, ekipman, ...), `ObsTable::Retain` (~satır 330).
- `GameServer/Bot/ActionExecutor.cpp` `TickUserIn` (F4-13, CLI-19: ≥ 1,0 sn aralık, istek başına ≤ 32 kimlik, `m_selfSid` hariç).
- Sunucu: `GameServer/GameServerDlg.cpp:1340-1358` `RegionUserInOutForMe`, `GameServer/User.cpp:1230-1253` `RequestUserIn`, bölge yayını `Send_Region` (kimlere gittiği ve `isInGame()` süzgeci).
- Olası nedenler (hipotezler, sırayla ele al):
  - **H1** `WIZ_REGIONCHANGE` `Retain`'i, kısa süre önce `WIZ_USER_INOUT(IN)` ile eklenen birimi listede olmadığı için siliyor (sıra/yarış: liste sunucuda eski anlık görüntüden, IN daha yeni).
  - **H2** Yeni doğan botun `USER_INOUT(IN/RESPAWN)` yayını, o sırada henüz `isInGame()` olmayan botlara gitmiyor (doğuş zamanlaması); sonradan `REGIONCHANGE`/`REQ_USERIN` yolu da tetiklenmiyor.
  - **H3** `ParseUserInOut`/`ParseUserInfo` bazı kayıtları (ör. klan/ekipman/rütbe alan değişkenliği) reddediyor: paket sessizce düşüyor.
  - **H4** `TickUserIn` istekleri CLI-19 aralığı veya `PendingIds::Peek` (tabloda "biliniyor" sandığı kimlikleri atlama) yüzünden hiç yollanmıyor.

## 3. Kapsam

**Yapılacaklar**

1. **Sayaçlar** (`BotSession`, atomik/`m_obsLock` altında; `/bot see` çıktısına eklenir): `inout_in`, `inout_out`, `inout_parse_fail`, `reqUserIn_recv`, `reqUserIn_units`, `reqUserIn_parse_stop` (liste `declared` > ayrıştırılan), `region_recv`, `region_ids_last`, `region_dropped_total` (`Retain`'in sildiği birim sayısı toplamı) ve `region_dropped_last_ids` (son silinen en çok 8 kimlik), `move_unknown` (tabloda olmayan kimlik için gelen `WIZ_MOVE`), `userin_req_sent`, `userin_req_skipped_gap`. Her sayaç yalnızca alıcı yolunda artar; yeni paket yok.
2. **Tekrar üretme betikleri** (yalnızca `bots/config/` altında örnek betik/senaryo; sunucuya kod eklemez): 3 bot (`BotWP_K`, `BotMF_K`, `BotPHD_K`) için **altı doğuş sırası** ve **iki aralık** (art arda ≤ 100 ms / aralarda 3 sn) ile `spawn` + 6 sn bekleme + her bot için `see` (`bots/config/see_symmetry_<n>.txt` veya tek betik `script_see_symmetry.txt`; betik fiilleri `docs/13`/F4-19 izinli listesinde: `spawn` yasak, bu yüzden doğuşu senaryo dosyasıyla veya `BotCommands.txt` ile elle yap; betik yalnızca `see`/`snap` basar). Beklenen görünüm kümesi sunucu `list` ile (test teşhis komutu, `G` sınıfı) çapraz kontrol edilir.
3. **Kök neden raporu**: Uygulayıcı Raporu'nda her koşunun (12 koşu) tek satırlık simetri sonucu ve sayaç dökümü; hipotezlerin hangisi doğrulandı/çürütüldü.
4. **Düzeltme (yalnızca kök neden `BotCore/Perception.h` veya `BotSession`'daysa):** en küçük değişiklik (örn. H1 için `Retain` yalnızca `REGIONCHANGE` anında listede olmayan **ve** o andan önce alınmış birimleri siler: her birime `lastSeenMs` ile karşılaştırma; H3 için ayrıştırıcı düzeltmesi + gerçek bayt dizili birim testi; H4 için istek/aralık mantığı). Düzeltme için regresyon birim testi **zorunlu**. Kök neden **sunucu tarafındaysa** (H2): düzeltme **yapılmaz**; bulgu `KNOWN_ISSUES` önerisi olarak rapora yazılır, bot tarafı geçici çözümü (örn. doğuş sonrası bir kez `WIZ_REQ_USERIN` ile bölge listesini isteme) yalnızca rapor önerisi olarak kalır.

**Kapsam dışı**

- Sunucu (`GameServer/*.cpp` bot dışı, `shared/`) değişikliği; yeni komut/ini anahtarı; karar katmanı; NPC tablosu (`npcs` aynı yöntemle **yalnızca gözlenir**, bulgu varsa rapora yazılır).
- `docs/`, `KNOWN_ISSUES.md` değişikliği (Claude yapar; KI-DEG-01 zaten açık).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `GameServer/Bot/BotSession.h` | değiştir | sayaç üyeleri |
| `GameServer/Bot/BotSession.cpp` | değiştir | `OnPacket` sayaç artırımları (davranış değişmez); düzeltme gerekirse yalnızca kök neden satırı |
| `GameServer/Bot/BotManager.cpp` | değiştir | yalnızca `CommandSee` çıktısı |
| `BotCore/Perception.h` | değiştir | yalnızca kök neden `BotCore`'daysa |
| `Tests/BotCoreTests/PerceptionTests.cpp` | değiştir | düzeltme yapılırsa regresyon testi |
| `bots/config/script_see_symmetry.txt` | yeni | `see`/`snap` betiği |

Listede olmayan dosyaya dokunmak gerekirse **durup** raporda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F4-54 gece/2026-10-02`; `Durum` → `UYGULANIYOR`; açık sunucuları durdur (`./tools/run-servers.sh stop`).
2. Sayaçlar ve `/bot see` satırı; derle (§7); `ENABLED=1` (`GameServer.ini` **elle değiştirilirse geri al**: md5 öncesi/sonrası raporda).
3. 12 koşu (3 bot × 6 doğuş sırası × 2 aralık): her koşuda üç botun `see` çıktısı + `list` konumları; asimetri bulununca sayaç dökümünü kaydet. **Sunucuyu ve istemciyi bu plan uygulanırken DeepSeek açmaz:** çalışma zamanı koşularını Claude yapar (§6 K8); DeepSeek'in işi sayaçlar, betik ve (kök neden koddaysa) düzeltmedir. Uygulayıcı Raporu'na "çalışma zamanı koşusu Claude'da" yaz.
4. Doğrulama sonrası Claude, kök neden bulgusuna göre ikinci tur (düzeltme talimatı) yazar; bu plan **iki turlu** tasarlandı.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, değişen dosyalar `touch` edilince yeni uyarı yok
- [ ] K2: `./tools/build.sh Debug` rc=0, uyarı yok
- [ ] K3: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; düzeltme yapıldıysa en az 1 yeni regresyon testi, yapılmadıysa test sayısı değişmez
- [ ] K4: sayaçlar yalnızca alıcı yolunda artar; mevcut `OnPacket` davranışı (tablo güncellemeleri, eko kayıtları) **aynıdır** (`git diff` yalnızca ek satır; `-` satırı yok ya da kök neden düzeltmesi)
- [ ] K5: yeni satırlarda `g_pMain|GetUserPtr|_PARTY_GROUP|m_pUser->` yok; sayaçlara kilit dışı erişim yok
- [ ] K6: yeni ini anahtarı/komut/thread/paket yok; `ENABLED=0` davranışı değişmez; ASCII + CRLF; `git diff --check` boş
- [ ] K7: `bots/config/script_see_symmetry.txt` `./Scripts/` ayrıştırıcısından geçer (F4-19 izinli fiiller: yalnızca `see`/`snap`/`list`), 100 adım sınırı içinde
- [ ] K8 (çalışma zamanı, Claude yapar): 12 koşunun tamamında üç botun `see`'si **her yönde simetrik** (aynı bölgedeki her çift birbirini gösteriyor) **veya** asimetri bulunduysa kök neden sayaçla belgeli ve (koddaysa) düzeltmeden sonra 12 koşu simetrik; sonuç tablosu plan dosyasına ve KI-DEG-01'ya işlenir

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release && ./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-54
grep -n "region_dropped\|inout_parse_fail" GameServer/Bot/BotSession.cpp GameServer/Bot/BotManager.cpp
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3 (ASCII, CRLF, tab, Allman, İngilizce yorum). Sunucu davranışını değiştirme; yalnızca bot alıcı yolu ve görüntü.
- Teşhis için sunucu nesnelerini bota **okutma**: karşılaştırma `list` (test teşhis komutu) ile Claude'un çalışma zamanı koşusunda yapılır; sayaçlar yalnızca alınan paketleri sayar.
- Sayaçlar `docs/13` §5.2a "G" sınıfı değildir (alınan paket sayısı `O`'dur), ama karar kodunun kullanacağı alanlar değildir: yalnızca `/bot see` çıktısı.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-54` (taban: `gece/2026-10-02`); `ad8598c` `[F4-54] Algi teshis sayaclari ve see dokumu`.
- Değişen dosyalar ve neden:
  - `GameServer/Bot/BotSession.h`: teşhis sayaç üyeleri (atomikler + `m_obsLock` altındaki `m_regionDroppedLastIds[8]`/`m_regionDroppedLastCount`). Yorum blokları eklendi.
  - `GameServer/Bot/BotSession.cpp`: `OnPacket()` içinde sayaç artırımları (`inout in/out/fail`, `reqUserIn recv/units/stop`, `region recv/ids/dropped`, `move_unknown`); `WIZ_REGIONCHANGE` dalında `Retain` öncesi tablo kimlikleri alınıp sonrasında düşenler hesaplanıyor (mevcut `Retain` satırı değişmedi); `WIZ_MOVE` dalında bilinmeyen kimlik `Find` ile sayılıyor (mevcut `UpdateMove` satırı değişmedi); `ResetForRespawn()` sayaçları sıfırlıyor. Tüm yeni satırlar alıcı yolunda; mevcut tablo güncellemeleri/eko kayıtları değişmedi.
  - `GameServer/Bot/BotManager.cpp`: yalnızca `CommandSee` çıktısına iki teşhis satırı (`diag:` sayaç dökümü + son silinen kimlik listesi). Kilit altında kopyalanıp kilit dışında biçimlendiriliyor.
  - `bots/config/script_see_symmetry.txt` (yeni): yalnızca `list`/`see`/`snap` içeren 7 adımlı betik; `./Scripts/` ayrıştırıcısından geçtiği bağımsız olarak doğrulandı (`ParseScript`: `error=0`, `steps=7`).
- Derleme sonucu:
  - `./tools/build.sh Release` rc=0; yalnızca eski `UpgradeHandler.cpp` C4789 uyarıları; değişen dosyalarda yeni uyarı yok.
  - `./tools/build.sh Debug` rc=0; değişen dosyalarda uyarı yok.
  - `./tools/run-tests.sh Release` ve `Debug`: `96 tests, 0 failed` (düzeltme yapılmadı, test sayısı değişmedi).
  - `tools/check-perception-contract.py` `RESULT: PASS` (`--selftest` rc=0).
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ (Release rc=0, touch'lı yeniden derleme, değişen dosyalarda yeni uyarı yok).
  - K2 ✔ (Debug rc=0, uyarı yok).
  - K3 ✔ (`96 tests, 0 failed`; düzeltme yapılmadığı için test sayısı değişmedi).
  - K4 ✔ (`git diff` yalnızca ek satır; kod dosyalarında `-` satırı yok; mevcut `OnPacket` davranışı aynı).
  - K5 ✔ (yeni satırlarda `g_pMain|GetUserPtr|_PARTY_GROUP|m_pUser->` yok; `m_regionDropped*` yalnızca `m_obsLock` altında; atomikler `OnPacket` yazar, `CommandSee` okur).
  - K6 ✔ (yeni ini anahtarı/komut/thread/paket yok; `ENABLED=0` davranışı değişmez; ASCII+CRLF; `git diff --check` boş).
  - K7 ✔ (betik `ParseScript`'ten geçti; 7 adım ≤ 100; fiiller `list`/`see`/`snap`).
  - K8: çalışma zamanı koşusu **Claude'da** (plan §5.3); DeepSeek sunucu/istemci açmadı. 12 koşunun simetri tablosu ve hipotez (H1..H4) hükmü doğrulamada doldurulacak.
- Plandan sapmalar ve gerekçeleri:
  - `userin_req_skipped_gap` sayacı eklenmedi: artırımı `ActionExecutor::TickUserIn` (gap dalı, `ActionExecutor.cpp:2848`) içindedir fakat bu dosya "Dokunulabilecek dosyalar" tablosunda yok. K4/K6 yalnızca alıcı yolunu şart koştuğu için gap sayacı atlandı; `userin_req_sent` yerine mevcut `m_userInRequests` (gönderilen istek) `diag` satırında `sent` olarak gösterildi. H4 teşhisi `pending` ve `sent` ile izlenebilir.
  - Teşhis için `BotCore/Perception.h` değişmedi (kök neden henüz bilinmiyor; plan yalnızca kök neden `BotCore`'daysa izin veriyor). `Retain`'in düşürdüğü sayı/kimlikler `BotSession` tarafında `Count()`/`At()`/`Find()` ile türetildi.
  - Silinen satır olmaması için yeni atomikler kurucu gövdesinde sıfırlanıyor (init listesi değiştirilmedi); kabul K4'ün "yalnızca ek satır" şartına birebir uyar.
- Açık sorular:
  - `userin_req_skipped_gap` gerekliyse `ActionExecutor.cpp`'nin izin listesine eklenmesi (veya sayaç yuvasının `BotSession` dışına taşınması) gerekir. Onay/karar bekleniyor.
  - Plan §7'deki örnek grep `region_dropped\|inout_parse_fail` (küçük harf/alt çizgi) bu koddaki `m_regionDropped*` / `m_inoutParseFail` (camelCase) adlarıyla eşleşmez; üslup korunarak camelCase bırakıldı.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-02

- **Karar: DOĞRULANDI** (gece modu, `AUTO_LOOP=1`; birleştirmeyi döngü betiği yapar). Kök neden bulundu: **H2** (doğuş zamanlaması; sunucu akışı + bot giriş sırası), kod düzeltmesi bu planın kapsamında değil (aşağıda "Kök neden").
- İncelenen commit: `bot/F4-54` @ `132d337` (kod: `ad8598c`); taban `gece/2026-10-02`. Çalışma ağacı temiz.

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `touch BotSession.cpp BotManager.cpp` + `./tools/build.sh Release` rc=0, `warning C` sayısı 0 |
| K2 | ✔ | aynı dosyalar `touch`, `./tools/build.sh Debug` rc=0, `warning C` 0 |
| K3 | ✔ | `./tools/run-tests.sh Release` ve `Debug`: `96 tests, 0 failed` (düzeltme yok, sayı değişmedi); `check-perception-contract.py` PASS |
| K4 | ✔ | `git diff gece/2026-10-02...bot/F4-54 -- GameServer bots` içinde `-` satırı yok (yalnızca ek); `Retain`/`UpdateMove`/`Upsert` satırları aynen; sayaçlar yalnızca `OnPacket` alıcı yolunda ve `ResetForRespawn`'da |
| K5 | ✔ | yeni satırlarda `g_pMain\|GetUserPtr\|_PARTY_GROUP\|m_pUser->` yok (grep rc=1). `m_regionDroppedLastIds/Count` yalnızca `m_obsLock` altında (`BotSession.cpp:293-315`, `BotManager.cpp:2306-2322`); diğer sayaçlar `std::atomic` |
| K6 | ✔ | yeni ini/komut/thread/paket yok; `ENABLED=0` yolu değişmedi (ek satırlar `OnPacket` içinde); 4 dosya `file`: ASCII, çalışma ağacı CRLF (`grep -c $'\r$'` = satır sayısı; depo LF, `core.autocrlf`), `git diff --check` boş, yeni satırlarda ASCII dışı karakter yok |
| K7 | ✔ | `BotCore/ScriptPlan.h` `ParseScript` ile bağımsız derlenen deneme: `error=0 steps=7`; fiiller `list`/`see`/`snap`; ayrıca sunucuda `script run see_symmetry` 12 kez `completed, 7/7 step(s)` |
| K8 | ✔ | 12 koşu aşağıda. Asimetri bulundu (6/12), kök neden sayaçla belgeli; kök neden `Perception.h`/`BotSession`'da değil, plan §3.4 gereği düzeltme yapılmaz (H2). 3 sn aralıklı 6 koşu simetrik |

**Çalışma zamanı (Claude yaptı):** `Release`, dal ucundan derlenmiş ikililer (`build/bin/x86-Release/Server`), `GameServer.ini` değiştirilmedi (md5 öncesi/sonrası `265a8e1c35ea12df46f6d006fe894d9b`, `[BOT] ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions, SPAWN_ON_START` boş); her koşu için sunucular yeniden başlatıldı (oturum listesi sırası doğuş sırasını etkilemesin), `Scripts/see_symmetry.txt` betiği `script run` ile çalıştı, iş bitince geçici betik silindi ve `run-servers.sh stop` ile üç sunucu kapatıldı (`0/3`), `BotCommands.*` kalmadı. Üç bot 12 koşunun hepsinde aynı konumda doğdu (sunucu `list`): `BotWP_K` (1274,891), `BotMF_K` (1274,905), `BotPHD_K` (1282,922); ikili uzaklıklar 14 / 18,8 / 32 m, yani üçü her zaman aynı 3×3 grupta (beklenen görünüm: herkes diğer ikisini görür).

| Koşu | Aralık | Doğuş sırası (oyuna giriş) | Simetri | Eksik yönler (`A!B` = A, B'yi görmüyor) |
|---|---|---|---|---|
| 1 | ≤100 ms | WP, MF, PHD | ✘ | MF!WP, PHD!WP, PHD!MF |
| 2 | ≤100 ms | WP, PHD, MF | ✘ | MF!PHD, PHD!WP |
| 3 | ≤100 ms | MF, WP, PHD | ✘ | WP!MF, PHD!WP |
| 4 | ≤100 ms | MF, PHD, WP | ✘ | WP!PHD, PHD!MF |
| 5 | ≤100 ms | PHD, WP, MF | ✘ | WP!PHD, MF!WP |
| 6 | ≤100 ms | PHD, MF, WP | ✘ | WP!MF, MF!PHD |
| 7-12 | 3 sn | altı sıranın hepsi | ✔ (6/6) | yok |

**Sayaç dökümü (12 koşuda ortak):** `inout_parse_fail 0`, `region recv 0 / ids 0 / dropped 0` (hiç `WIZ_REGIONCHANGE` gelmedi), `move_unknown 0`, `userin recv 1` (her botta tam bir kez: `GameStart(1)` yanıtı), `userin stop 0` (bildirilen sayı = ayrıştırılan), `userin requests 0 / pending 0` (`TickUserIn` hiç istek göndermedi çünkü bekleyen kimlik yok), `unresolved 0`. Asimetrik koşularda `userin units` ile `inout in` toplamı o botun gördüğü birim sayısına eşit; eksik birim hiçbir yoldan gelmedi (örn. koşu 1: `BotPHD_K` `inout in 0`, `userin recv 1 units 0` → görüş tablosu boş).

**Kök neden (hipotezler):**

- **H1 çürütüldü:** `region recv 0`; hiçbir koşuda `Retain` çalışmadı, silinen birim 0.
- **H3 çürütüldü:** `inout_parse_fail 0` ve `userin stop 0`; kayıtlar düşmüyor, paketler hiç gelmiyor.
- **H4 çürütüldü (kısmen anlamsız):** `TickUserIn` bekleyen kimlik olmadığı için istek göndermiyor (`pending 0`, `sent 0`); aralık/`PeekUserInBatch` bir engel değil. Eksik kimlikleri bilmenin tek yolu `WIZ_REGIONCHANGE` listesidir ve o liste yalnızca bölge sınırı geçilince gelir (`GameServer/CharacterMovementHandler.cpp:34-39`, `RegisterRegion()` true olunca); sabit duran botta hiç gelmez.
- **H2 doğrulandı (sunucu akışı + bot giriş sırası):** `GameStart(opcode 1)` (`GameServer/CharacterSelectionHandler.cpp:266-269`) bölge kullanıcı listesini (`UserInOutForMe`, bota `WIZ_REQ_USERIN` yanıtı olarak gelen tek liste) **o anki** bölge üyelerinden üretir; kullanıcı bölgeye `GameStart(opcode 2)` ile `UserInOut(INOUT_RESPAWN)` (`:284`) sırasında kaydolur ve yayın yalnızca o sırada `isInGame()` olanlara gider (`GameServer/User.h:312`, `m_state` opcode 2'de INGAME olur). Bot `BotManager.cpp:2961-2977`'de iki opcode arasında yalnızca `LOADED_DELAY_MS = 200 ms` bekler ve üç bot aynı tick'te başlayınca üçünün de opcode 1 anlık görüntüsü, diğerlerinin kaydından önce alınır. Bir bot (A) başka bir botun (B) opcode-1 ile opcode-2 arasındaki penceresinde kaydolursa B, A'yı ne listede ne yayında alır; B sabit durduğu için `WIZ_REGIONCHANGE` de gelmez ve görüş tek yönlü kalır (A, B'nin IN yayınını alır). 3 sn arayla doğunca pencereler çakışmaz: 6/6 simetrik.
- Gerçek oyuncu istemcisi aynı pencereye sahiptir (yükleme ekranı süresince kaydolanları kaçırır); bu sunucu davranışı olduğu için kapsam dışıdır.

**Bulgular (önem sırasıyla):**

1. **Not (üslup, engel değil):** `BotSession.cpp:305-313` `WIZ_REGIONCHANGE` dalında her pakette tablo kimlik kopyası (≤ 64) + `Find` döngüsü (O(n²), ≤ 64×64) çalışır; yalnızca bölge geçişinde ve `m_obsLock` altında olduğu için maliyeti önemsiz. Teşhis amaçlı; karar kodu bu alanları kullanmaz.
2. **Not:** `userin_req_skipped_gap` sayacının atlanması doğru karardı (`ActionExecutor.cpp` izin listesinde yoktu); `pending 0 / sent 0` değerleri H4'ü o sayaç olmadan da çürütüyor. Uygulayıcı sorusu: ek izin gerekmiyor.
3. **Not:** Plan §7 örnek `grep` kalıbı camelCase üyelerle (`m_regionDropped*`, `m_inoutParseFail`) eşleşmiyor; uygulayıcı üslubu korudu, kabul.
4. **Sonraki iş (bu plan dışında, öneri):** bot tarafı zamanlama düzeltmesi (`BotManager.cpp` giriş durum makinesi, `PHASE_WAIT_SELECT`/`PHASE_WAIT_LOADED`): bir bot opcode 1 ile opcode 2 arasındayken başka bir botun opcode 1'ini başlatmamak (giriş el sıkışmasını botlar arasında serileştirmek) ve opcode 2 gecikmesini kısaltmak penceresiz doğuşu sağlar; sunucu koduna dokunmaz, bot tablosu veri kaynağı değişmez. Yeni plan gerekir (`docs/KNOWN_ISSUES.md` KI-DEG-01 notuna yazıldı). Bu çözüm uygulanana kadar geçici çözüm: botları ≥ 3 sn arayla doğurmak (`spawn` komutlarını ayrı dosyalarla).

Düzeltme talimatı: yok (karar DOĞRULANDI).
