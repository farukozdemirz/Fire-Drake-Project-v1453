# F4-60: `Perception` dilim 12 — gözlenen durum tablosunun sunucu bağlaması (`SkillMeta` kurucusu, `BotSession` beslemesi, `WIZ_DEAD` temizliği, `/bot snap <bot> status`)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2, m.10 algı eksikleri; F6/F7 priest ve stall için ön koşul) |
| Branch | `bot/F4-60 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | F4-53 (`SkillMeta`, `ObservedStatusTable`, `HealObsRing`) `KAPANDI`; F4-52 (`m_skillEvents` beslemesi, `snap <bot> events`) `KAPANDI`; F4-28/F4-32 (Type4/cure atılabilir; çalışma zamanı doğrulaması için) `KAPANDI` |
| İlgili gereksinim / kabul | `docs/03` §16, MEC-BUF-01..07, MEC-BUF-10, MEC-MAG-19, MEC-DTH-01; `docs/13` §5.2a (`E` sınıfı: tahmin); ADR-0017 Eki F4-53, ADR-0018 Ek 26/Ek 27; DEG-08 (`docs/reports/degerlendirme-2026-10-02.md`) |
| Tahmini büyüklük | S–M (5 kod dosyası, yeni dosya yok) |
| Hazırlayan / tarih | Claude / 2026-10-03 (gece modu) |

---

## 1. Amaç

F4-53, görülen Type4 yayınlarından ve heal olaylarından beslenen saf mantığı (`ObservedStatusTable`, `HealObsRing`) ekledi; ancak **hiçbir yerden çağrılmaz**. Bu plan onu canlı bota bağlar: bota gelen her `WIZ_MAGIC_PROCESS` `EFFECTING` yayınının skill'i statik skill tablolarından `SkillMeta` değer kopyasına çevrilir, iki tabloya beslenir; `WIZ_DEAD` ölenin kayıtlarını siler (MEC-DTH-01); `/bot snap <bot> status` tabloyu döker, böylece çalışma zamanında botun **gözlediği** durum (E sınıfı) sunucunun gerçek buff listesiyle karşılaştırılabilir. Karar/guard/telemetri/yeni paket yok; görünüm alanları (`UnitView`...) ve `check-perception-contract.py` R5 güncellemesi **F4-61**'dir (§3 kapsam dışı). Bot sistemi kapalıyken (`ENABLED=0`) davranış değişmez.

## 2. Bağlam (okunması zorunlu)

- **Besleme noktası (var):** `GameServer/Bot/BotSession.cpp:89-102` F4-52 bloğu: `opcode == WIZ_MAGIC_PROCESS && pkt.size() >= 23` → `ParseSkillEvent` (kilit dışında) → `m_obsLock` altında `m_skillEvents.Add(ev)`. Aynı blok genişletilir (aynı kilit, ayrı değişken). `OnPacket()` DB iş parçacığında, IOCP'de ya da 30 sn zamanlayıcıda çalışabilir (`BotSession.h:15`); tablolar yalnız `m_obsLock` altında okunur/yazılır.
- **Skill tabloları (statik oyun verisi):** `GameServer/GameServerDlg.h:356-361` `m_MagictableArray`, `m_Magictype3Array`, `m_Magictype4Array`, `m_Magictype5Array` (`CSTLMap`, `GetData(id)`, içeride `recursive_mutex`; `GameServer/LoadServerData.h:17-21`). Bot tarafında aynı tablolar zaten okunur: `ActionExecutor.cpp:724` (`m_MagictableArray.GetData`), `:746` (`m_Magictype5Array.GetData`), `:1421` (`m_Magictype3Array.GetData`), `BotManager.cpp:2531`. Okuma **kilit dışında** yapılır (tablo kilidi ile `m_obsLock` iç içe girmesin).
- **Alan adları (`shared/database/structs.h`):** `_MAGIC_TABLE` `bType[2]` (`:21`, `MagicNum`'ı `iNum` ile gösterir: `GetData` anahtarı skill kimliğidir), `_MAGIC_TYPE3` (`:51-62`: `bDirectType`, `sFirstDamage` `int16`, `sTimeDamage` `int16`, `bDuration` `uint8`), `_MAGIC_TYPE4` (`:64-100`: `bBuffType`, `bIsBuff`; `bIsBuff` tabloda değil, yüklemede `CMagicProcess::IsBuff` ile hesaplanır, `shared/database/MagicType4Set.h:47`; bu yüzden çalışma belleğindeki değer okunur), `_MAGIC_TYPE5` (`:102-107`: `bType`).
- **`SkillMeta` alanları (`BotCore/Perception.h`, F4-53):** `skillId`, `type1`, `type2`, `buffType`, `isBuff`, `directType`, `firstDamage`, `timeDamage`, `type3DurationSec`, `type5Kind`. Doldurma kuralı: Type4 kısmı (`type1 == 4 || type2 == 4`) → `m_Magictype4Array`; Type3 kısmı (`type1 == 3 || type2 == 3`) → `m_Magictype3Array`; Type5 (`type1 == 5`) → `m_Magictype5Array`. İlgili tablo satırı yoksa o alanlar 0 kalır (sınıflandırma yardımcıları sıfırla güvenle çalışır: `SkillSendsType4` yalnız `type1/type2`'ye bakar; `buffType`/`isBuff` 0 ise Type4 kaydı yine yazılır ama `buffType 0`: bu durumda **meta çıkarma başarısız sayılır, `nullptr` verilir**, çünkü Type4 tablo satırı yoksa kayıt anahtarı bilinmez).
- **`WIZ_DEAD`:** `BotSession.cpp:333-339` (oyuncu dalı) ve `:399-405` (NPC dalı) `u16 id = data[0] | data[1] << 8` ile okur; `m_hp.Invalidate(id)` çağırır. Kullanıcı ve NPC kimlik uzayları ayrıdır (NPC ≥ 10000); `ObservedStatusTable` kimliği tek anahtar kullanır, bu yüzden **tek** yeni blok yeter. `ResetForRespawn` (`BotSession.cpp:418-490`, `:484-485` `m_hp.Clear()`, `m_skillEvents.Clear()`) yeni tabloları da temizlemeli.
- **`CommandSnap`:** `GameServer/Bot/BotManager.cpp:2552-2806` (kuyruk `:2806`); kullanım dizgesi `snap <bot> [events]` iki yerde (`:2559`, `:2566`); komut dağıtımı `:662-663`; `events` dökümü `:2779-2798` (`wantEvents`), argümansız `snap` sonunda `events total=<n>` satırı `:2799-2805` (bu davranış **değişmez**). Tablolar kilit altında **kopyalanır**, biçimlendirme kilit dışındadır (`:2596-2603`); aynı düzen kullanılacak. Betik fiili `snap` zaten izinli (`BotCore/ScriptPlan.h:66`); `status` ikinci argüman olduğu için `ScriptPlan.h` değişmez.
- **Sözleşme denetimi:** `tools/check-perception-contract.py` yalnız `GameServer/Bot` ve `BotCore` altında R1 (`GetNpcPtr`, `m_RegionUserArray`, ...), R2 (`GetUserPtr`, `GetMap`, `m_buffMap`, `isInParty`... ; izinli listeye girmeyen kullanım ihlal), R3 (`->m_pUser`), R4 (`BotCore` include), R5 (görünüm alanları) arar. `m_Magictable*Array` R2 sembolü **değildir** (`ActionExecutor.cpp` zaten kullanır); yeni satırlar R1-R3 sembollerini içermemeli, `UnitView`/`NpcView`/`TeamMemberView` **değişmez** ⇒ R5'e dokunulmaz.
- **Doğrulama için gerçek skill verisi (yerel DB, yalnız skill tabloları, 2026-10-03):** Fresh mind `112645` `{4, 0}` `Moral 2` `BuffType 8` `Duration 600` `Msp 60`; Insensibility peel `112660` `Moral 2` `BuffType 2` `Duration 600` `Msp 150` (Skill `1126`, `SkillLevel 60`); Undying `112654` `Moral 4` (party gerekir: bu planda kullanılmaz); Malice `112703` `{4, 0}` `Moral 7` `BuffType 2` (debuff) `Duration 150` `Msp 40` `Range 56`, El Morad `212703` `Range 90`; Great healing `112527` `{3, 0}` `Moral 2` `Msp 80` (`FirstDamage 960`, F4-53 §2); Cure curse `112525` / `212525` `{5, 0}` `Moral 2` `Msp 60` (`MAGIC_TYPE5.Type 2`). `Moral 2` = tek hedefli dost skill, bot `cast` ile hedef adıyla atılabilir (F4-42 §9.1: party gerekmez; hedef tam canlı iken heal reddedilebilir, bulgu 4).
- **Süre taban zamanı:** `ParseSkillEvent` olay zamanını çağıranın `nowMs`'i ile damgalar (`std::chrono::steady_clock` ms, `BotSession.cpp:93-94`); `snap` da aynı tabanda `nowMs` hesaplar (`BotManager.cpp:2624`).

## 3. Kapsam

**Yapılacaklar**

1. **`BotCore/Perception.h`** (yalnızca ekleme; F4-53 bloğunun içinde, `ObservedStatusTable`'a bir sorgu): `int Targets(int16_t * out, int cap) const` — en az bir kaydı olan birimlerin hedef kimliklerini yuva sırasıyla `out`'a yazar (≤ `cap`), yazılan sayıyı döndürür; `out == nullptr` ya da `cap <= 0` ⇒ 0. `snap status` dökümü birim listesini bu sorguyla alır (görünür birim listesinden türetmek, görünürde olmayan hedefleri kaçırırdı).
2. **`Tests/BotCoreTests/PerceptionTests.cpp`** (dosya sonuna): 1 yeni `TEST_CASE` — `Perception_Status_Targets` (259 → 260).
3. **`GameServer/Bot/BotSession.h`**: `BotCore::ObservedStatusTable m_status;` ve `BotCore::HealObsRing m_healObs;` — `m_skillEvents` satırının (`:162`) altında, aynı biçimde açıklamalı ("guarded by m_obsLock (the same mutex as m_obs)", ADR-0017 Ek F4-53/F4-60).
4. **`GameServer/Bot/BotSession.cpp`:**
   - Dosya-statik yardımcı `static bool FillSkillMeta(uint32 skillId, BotCore::SkillMeta & out)` (dosyanın başında, ilk fonksiyondan önce; `stdafx.h` zaten `GameServerDlg.h`'yi getirir): `_MAGIC_TABLE * m = g_pMain->m_MagictableArray.GetData(skillId)`; `nullptr` ⇒ `false`. `out` sıfırlanır (`memset` ya da değer ilkleme), `skillId`, `type1 = m->bType[0]`, `type2 = m->bType[1]`. Type4 kısmı varsa `_MAGIC_TYPE4 * t4 = ...GetData(skillId)`; `nullptr` ⇒ `false`; aksi halde `buffType = t4->bBuffType`, `isBuff = t4->bIsBuff`. Type3 kısmı varsa `t3` varsa `directType/firstDamage/timeDamage/type3DurationSec` kopyalanır (`t3 == nullptr` ⇒ Type3 alanları 0 kalır; hata sayılmaz). `type1 == 5` ise `t5` varsa `type5Kind = t5->bType`. Hiçbir işaretçi saklanmaz, hiçbir tablo satırı değiştirilmez; `m_buffMap`/`GetUserPtr` kullanılmaz.
   - F4-52 bloğunun içinde (`:92-102`), `ParseSkillEvent` başarılıysa ve `ev.op == BotCore::kMagicEffecting && ev.target >= 0` iken kilitten **önce** `BotCore::SkillMeta meta; bool haveMeta = FillSkillMeta(ev.skillId, meta);`; kilit altında mevcut `m_skillEvents.Add(ev)` aynen kalır, hemen ardından `m_status.Observe(ev, haveMeta ? &meta : nullptr);` ve `m_healObs.Observe(ev, haveMeta ? &meta : nullptr);`. `Add(ev)` her zaman çalışır (CASTING/FLYING dahil); tablo/heal beslemesi yalnız `EFFECTING` + hedefli olaylar içindir (CASTING/FLYING için tablo araması **yapılmaz**).
   - Yeni küçük blok (F4-52 bloğunun hemen altında): `opcode == WIZ_DEAD && pkt.size() >= 2` → `uint16 id = (uint16)pkt.contents()[0] | ((uint16)pkt.contents()[1] << 8);` kilit altında `m_status.ClearTarget((int16_t)id);`. Mevcut iki `WIZ_DEAD` dalına **dokunulmaz**. Birim görüş dışına çıkınca (`WIZ_USER_INOUT` out, `WIZ_NPC_INOUT` out) **temizlenmez**: buff sunucuda sürer, görüşe dönünce tahmin geçerli kalabilir; kapasite taşması ve `Prune` zaten eskiyi atar (kararın gerekçesi koda yorum olarak yazılır).
   - `ResetForRespawn` içinde (`:484-485`'in yanında, aynı kilit bloğu): `m_status.Clear(); m_healObs.Clear();`.
   - Bu planda `Prune` çağrısı **yoktur**: `Find`/`Collect` yalnız canlı kayıtları döndürür; `Prune` çağrısı karar katmanıyla birlikte düşünülür.
5. **`GameServer/Bot/BotManager.cpp` — `CommandSnap`:** kullanım dizgeleri `snap <bot> [events|status]` olur; `wantStatus = (words.size() == 2 && words[1] == "status")`; `events` ve `status` birlikte verilemez (ikinci argüman bunlardan biri değilse mevcut kullanım iletisi). `wantStatus` iken tablolar kilit altında kopyalanır (`statusCopy = s->m_status; healCopy = s->m_healObs;`, yalnız bu atamalar), biçimlendirme kilit dışında. Çıktı (mevcut `snap` çıktısının **sonuna**, `events` dalının yerine; `events total=<n>` satırı `status` iken **yazılmaz**, `events` dalı değişmez):
   - `status units=<n> records=<n> heals_total=<n> heals_in_ring=<n>` (`Units()`, `Records()`, `healCopy.Total()`, `healCopy.Count()`);
   - her birim için (`Targets()` ile, en çok 10 birim) her canlı kayıt (`Collect(target, nowMs, out, 8)`, en çok 8): `status target=<id> skill=<id> type=<buffType> <buff|debuff> caster=<id> remain=<ms>ms src=E` (`remain` = `StatusRemainingMs`);
   - heal halkasının en yeni ≤ 5 olayı: `status heal age=<ms> skill=<id> caster=<id> target=<id> nominal=<n> hot=<0|1>`.
   Argümansız `snap` ve `snap <bot> events` çıktıları **değişmez**. Mesaj tamponu 320 baytı geçmemeli (`snprintf`).
6. **Çalışma zamanı doğrulaması** (Claude yapar, §6 K9): bot sunucusuyla gerçek skill'ler; DeepSeek bunu **yapmaz** (sunucu açma yetkisi planın uygulayıcı adımında yok; DeepSeek yalnız derleme + birim test).

**Kapsam dışı (yapılmayacak)**

- `UnitView`, `NpcView`, `TeamMemberView`, `PerceptionSnapshot`'a durum alanı eklemek (`statusCount`, `HealRate` vb.), `BuildSnapshot`/`AttachHp` değişikliği ve `tools/check-perception-contract.py` R5 güncellemesi: **F4-61** (Claude/sonraki plan). Bu planda `BotCore/Perception.h` yalnız `Targets()` eklenir.
- Type3 DoT/HoT hedef kaydı ve `REMOVE_TYPE3` (Cure disease) etkisi; `MAGIC_DURATION_EXPIRED`/iptal paketleri; `Prune` periyodik çağrısı; bitiş tahmininin hata payı **istatistiği** (yalnız bir kez elle ölçülür, §6 K9).
- Karar/telemetri (`decisions` olayları, jsonl), takım içi paylaşım (`P`), `UseItem`/yeni cast dilimi, `ScriptPlan.h` fiil listesi, yeni ini anahtarı, yeni komut (yalnız `snap` ikinci argümanı), yeni thread, yeni paket isteği.
- `GameServer/` içinde `Bot/` dışındaki dosyalar, `shared/`, `AIServer/`, `docs/`, `tools/`: değişmez.
- Mevcut hiçbir satırın silinmesi/yeniden biçimlendirilmesi (kullanım dizgesi iki satırı hariç: eski `[events]` → `[events|status]`).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/Perception.h` | değiştir | yalnızca `ObservedStatusTable::Targets` (F4-53 bloğu içinde, `Records()` yanında) |
| `Tests/BotCoreTests/PerceptionTests.cpp` | değiştir | yalnızca dosya sonuna 1 `TEST_CASE` |
| `GameServer/Bot/BotSession.h` | değiştir | `m_status`, `m_healObs` |
| `GameServer/Bot/BotSession.cpp` | değiştir | `FillSkillMeta`, besleme, `WIZ_DEAD` bloğu, `ResetForRespawn` |
| `GameServer/Bot/BotManager.cpp` | değiştir | yalnızca `CommandSnap` |

Yeni dosya yok ⇒ `.vcxproj`/`.filters` değişmez. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F4-60 gece/2026-10-02`; `Durum` → `UYGULANIYOR`; açık sunucuları durdur (`tools/run-servers.sh stop`). Plan başındaki test sayısını not et: `./tools/run-tests.sh Release 2>&1 | tail -3` (beklenen `259 tests, 0 failed`).
2. `Perception.h`: `ObservedStatusTable`'a `int Targets(int16_t * out, int cap) const` ekle (`Units()`'ten sonra; yuvaları sırayla gez, `count > 0` olanların `target`'ını yaz). İmza bağlayıcıdır. Test `Perception_Status_Targets`: boş tablo ⇒ 0; `nullptr`/`cap 0` ⇒ 0; hedef `2985` (iki kayıt, `buffType 1` ve `2`) ve `2986` (bir kayıt) ⇒ `Targets` 2 ve ikisi de listede (sıra yuva sırasıdır: ilk eklenen önce); `cap 1` ⇒ 1; `ClearTarget(2985)` sonrası yalnız `2986`; cure ile son debuff'ı silinen birim listede **yok**; 33. hedef eklenince (`kObsStatusUnits` = 32) sayı 32'de kalır.
3. `BotSession.h/.cpp`: §3.3-§3.4. Kod iskeleti (imzalar bağlayıcı, gövdeler sende):

```cpp
// BotSession.cpp, file scope, before the constructor
static bool FillSkillMeta(uint32 skillId, BotCore::SkillMeta & out);

// inside OnPacket(), the F4-52 block (after ParseSkillEvent succeeded):
//   BotCore::SkillMeta meta;
//   bool haveMeta = false;
//   if (ev.op == BotCore::kMagicEffecting && ev.target >= 0)
//       haveMeta = FillSkillMeta(ev.skillId, meta);        // static tables, outside m_obsLock
//   { lock m_obsLock;  m_skillEvents.Add(ev);
//     m_status.Observe(ev, haveMeta ? &meta : nullptr);
//     m_healObs.Observe(ev, haveMeta ? &meta : nullptr); }
```

4. `BotManager.cpp` `CommandSnap`: §3.5. Çıktı satırlarının önekini mevcut satırlarla aynı yaz: `"BotManager: cmd snap:   status ..."` (üç boşluk).
5. Derleme ve test (§7). Çalışma zamanı doğrulaması **Claude'un** işidir (§6 K9): `Durum` → `UYGULANDI`, Uygulayıcı Raporu.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0; beş dosya `touch` edilip yeniden derlendiğinde değişen dosyalarda yeni uyarı yok (yalnızca eski `UpgradeHandler.cpp` C4789 olabilir)
- [ ] K2: `./tools/build.sh Debug` rc=0, yeni uyarı yok
- [ ] K3: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`, test sayısı plan başındakinden **1 fazla** (`259` → `260`); `Perception_Status_Targets` her iki yapılandırmada `[ OK ]`
- [ ] K4 (`BotCore` saflığı): `git diff gece/2026-10-02...bot/F4-60 -- BotCore | grep '^+' | grep -E '#include|windows\.h|stdafx|GameServer|shared/'` boş; `Perception.h` farkında silinen satır 0 (`grep -c '^-[^-]'` = 0)
- [ ] K5 (sözleşme): `python3 tools/check-perception-contract.py` `RESULT: PASS`, R1 0, R2 0 ihlal (28 izinli), R3 0 ihlal (18 izinli), R4 0, R5 0; `--selftest` rc=0; eklenen satırlarda `GetUserPtr|m_buffMap|m_pUser->|GetRegion|m_RegionUserArray|isInParty|GetMap(` yok (`git diff -U0 gece/2026-10-02...bot/F4-60 | grep '^+' | grep -E ...` boş); `BotSession.cpp`'de tablo erişimi yalnız `FillSkillMeta` içinde (`grep -n 'm_Magictable' GameServer/Bot/BotSession.cpp` yalnız o fonksiyonun satırları)
- [ ] K6 (kapsam): `git diff --stat gece/2026-10-02...bot/F4-60` yalnızca §4'teki 5 dosya ve bu plan dosyasını gösterir (`shared/`, `AIServer/`, `tools/`, `docs/`, `ScriptPlan.h`, `UnitView` yok)
- [ ] K7 (kilit): `grep -n 'm_status\|m_healObs' GameServer/Bot/*.cpp GameServer/Bot/*.h`: her kullanım `m_obsLock` altında (BotSession.cpp besleme/`WIZ_DEAD`/`ResetForRespawn`; BotManager.cpp yalnız kilit altındaki kopya ataması); `FillSkillMeta` çağrısı kilit **dışında**; CASTING/FLYING olaylarında `FillSkillMeta` çağrılmaz (`ev.op == BotCore::kMagicEffecting` koşulu)
- [ ] K8 (kurallar): eklenen satırlar ASCII; beş dosya baştan sona CRLF (`file`; satır sayısı = CRLF sayısı); sekme girinti, Allman; `git diff --check` boş; yeni `printf`(`snprintf` dışı)/`Sleep`/`CreateThread`/`rand(` yok; yeni ini anahtarı/komut/thread/paket isteği yok; mevcut `WIZ_DEAD` dalları, `m_castEcho` bloğu ve F4-52 `m_skillEvents.Add` satırı değişmedi (`-` satırları yalnızca `snap` kullanım dizgesi)
- [ ] K9 (çalışma zamanı, Claude yapar; `[BOT] ENABLED=1`, `TELEMETRY=decisions`, zone 71, hepsi yakın; iş bitince `run-servers.sh stop`): (S1) `cast BotPHD_K 112645 BotWP_K` sonrası **gözlemci** `BotPHB_K` ve `BotWG_K` için `snap <bot> status`: `target=<BotWP_K sid> skill=112645 type=8 buff caster=<BotPHD_K sid> src=E`, `remain` ≈ 600000 − yaş (±2000 ms), ve `BotWP_K`'nın kendi gerçek buff satırı (`buff skill=112645 type=8 remain=…s`) ile **fark ≤ 3 sn**; (S2) `cast BotPHD_K 112703 BotWP_E` (düşman debuff): gözlemcide `debuff type=2 remain` ≈ 150000, `BotWP_E`'nin gerçek buff satırı `type=2 debuff` ile fark ≤ 3 sn; reddedilen/hatalı atışta (`CastOutcome` `srv_fail`) kayıt oluşmaz; (S3) El Morad priest (`BotPHB_E`; `bad_skill` ise başka El Morad priest) `212703`'ü `BotWP_K`'ya atar (menzil 90), ardından `BotPHD_K` `112525` (Cure curse) `BotWP_K`'ya: gözlemcide debuff kaydı **silinir**, buff'lar (`type=8`) kalır; `BotWP_K` gerçek listesinde `type=2` yok; (S4) `BotWP_E` `BotWP_K`'ya birkaç vurduktan sonra `cast BotPHD_K 112527 BotWP_K`: `status heal ... skill=112527 target=<BotWP_K sid> nominal=960 hot=0`, `heals_total` 1 artar (hedef tam canlıysa sunucu reddedebilir: `[Ö]`, `docs/05` §9.1 bulgu 4); (S5) kopyalama: iki ardışık `snap <bot> status` aynı kayıtları gösterir (`remain` azalır, kayıt sayısı aynı); (S6) tahmin hata payı bir kez kaydedilir: `E` bitişi ile gerçek `remain` farkı (S1/S2'den), `docs/05`'e **yazılmaz**, doğrulama raporuna yazılır; (S7) ölüm: bir bot öldüğünde (yoksa kodla) `ClearTarget` ile o hedefin kaydı silinir; (S8) gerileme: argümansız `snap` (`events total=<n>` satırı aynı), `snap <bot> events`, `see`, `cast`, `list` çalışır, hatalı `snap <bot> foo` kullanım iletisini yazar (`[events|status]`), `Bot_*.log`'da bu oturumda `WARN`/`ERROR` 0

## 7. Doğrulama komutları

```bash
./tools/run-tests.sh Release 2>&1 | tail -3     # plan başında sayıyı not et (259)
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release && ./tools/run-tests.sh Debug
python3 tools/check-perception-contract.py | sed -n '1,14p'
python3 tools/check-perception-contract.py --selftest; echo rc=$?
git diff --stat gece/2026-10-02...bot/F4-60
git diff gece/2026-10-02...bot/F4-60 -- BotCore/Perception.h | grep -c '^-[^-]'
grep -n 'm_status\|m_healObs' GameServer/Bot/BotSession.cpp GameServer/Bot/BotManager.cpp GameServer/Bot/BotSession.h
grep -n 'm_Magictable' GameServer/Bot/BotSession.cpp
file BotCore/Perception.h Tests/BotCoreTests/PerceptionTests.cpp GameServer/Bot/BotSession.h GameServer/Bot/BotSession.cpp GameServer/Bot/BotManager.cpp
git diff --check gece/2026-10-02...bot/F4-60
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3 (ASCII, CRLF, sekme, Allman, İngilizce yorum). Dosyalar CRLF'dir: ekleme araçları satır sonunu bozmamalı (`file` ile denetle).
- Tablo okuma (`m_Magictable*Array.GetData`) `m_obsLock` **dışında** yapılır; `m_obsLock` altında yalnızca kopyalama/ekleme (tick süresini uzatma). `FillSkillMeta` yalnızca değer kopyası döndürür, tablo satırı işaretçisi saklamaz.
- Tüm kayıtlar `E` sınıfıdır ve **tahmindir** (başkasının buff'ı sunucuda tahminden önce ya da sonra bitebilir: MEC-BUF-07; iptal görünmez). `snap status` çıktısında `src=E` yazılır; bunu gerçek buff listesi saymayın (yoruma yaz).
- Bot yalnızca kendisine gelen paketlerden öğrenir: `FillSkillMeta` oyuncu kimliği ya da konumu okumaz; başka oturumun `CUser`'ına erişmez.
- Heal olaylarında `ev.data[]`'ya güvenilmez (F4-53 §8); heal beslemesi yalnız `skillId/caster/target`'a dayanır.
- Kapsam büyütme: `UnitView` alanı, karar/telemetri, `Prune` zamanlaması, DoT/HoT kaydı yazma. Gerekirse Uygulayıcı Raporu'nda "açık soru" olarak yaz.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum:
- Branch / commit'ler:
- Değişen dosyalar ve neden:
- Derleme sonucu:
- Kabul kriterleri öz-değerlendirme (K9: Claude yapar):
- Plandan sapmalar ve gerekçeleri:
- Açık sorular:

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar:
- İncelenen:
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | | |

- Bulgular:
- Düzeltme talimatı:
