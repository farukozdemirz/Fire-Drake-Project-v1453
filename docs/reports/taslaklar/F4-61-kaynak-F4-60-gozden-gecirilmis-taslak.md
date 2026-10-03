> **Amaç (2026-10-03):** Bu dosya bir plan DEĞİLDİR. F4-60 zaten döngüde uygulanıp `KAPANDI` olduğundan, F4-60'ın görünürlük kaybı / yeniden giriş / `uncertain` işareti / ±2 sn ölçüm sözleşmesi eklenmiş gözden geçirilmiş taslağı **F4-61 planının kaynağı** olarak saklanır; F4-61 yazılırken (F4-60'ın gerçek koduna göre) bu taslaktan yalnızca F4-60'ta olmayan kısımlar alınır. ADR-0018 Ek 29 bu planın kararı için ayrılmıştır (Ek 28 skill sınıflandırmasıdır).

# F4-60: `Perception` dilim 12 — gözlenen durum tablosunun sunucu bağlaması (`SkillMeta` kurucusu, `BotSession` beslemesi, süre sonu/ölüm/görünürlük kaybı/yeniden giriş, `/bot snap <bot> status`)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2, m.10 algı eksikleri; F6/F7 priest ve stall için ön koşul) |
| Branch | `bot/F4-60 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | F4-53 (`SkillMeta`, `ObservedStatusTable`, `HealObsRing`) `KAPANDI` (merge `0001d04`); F4-52 (`m_skillEvents` beslemesi, `snap <bot> events`) `KAPANDI`; F4-14/F4-54 (NPC/oyuncu görüş tabloları, giriş/çıkış paketleri) `KAPANDI`; F4-28/F4-32 (Type4/cure atılabilir, çalışma zamanı doğrulaması için) `KAPANDI` |
| İlgili gereksinim / kabul | `docs/03` §16, MEC-BUF-01..07, MEC-BUF-10, MEC-MAG-19, MEC-DTH-01; `docs/13` §5.2a (`O`/`E` sınıfı, `in_region`, bayatlama); ADR-0017 Eki F4-53, ADR-0018 Ek 26/Ek 27; DEG-08 (`docs/reports/degerlendirme-2026-10-02.md`) |
| Tahmini büyüklük | S–M (5 kod dosyası, yeni dosya yok) |
| Hazırlayan / tarih | Claude / 2026-10-03 (gece modu; gözden geçirme: süre sonu `Prune` çağrısı, görünürlük kaybı/yeniden giriş sözleşmesi ve `O`/`E` bilgi tablosu eklendi; ±2 sn ölçüm) |

---

## 1. Amaç

F4-53 görülen Type4 yayınlarından ve heal olaylarından beslenen saf mantığı (`ObservedStatusTable`, `HealObsRing`) ekledi; ancak **hiçbir yerden çağrılmaz**. Bu plan onu canlı bota bağlar: bota gelen her `WIZ_MAGIC_PROCESS` `EFFECTING` yayınının skill'i statik skill tablolarından `SkillMeta` değer kopyasına çevrilir ve tablolara beslenir. Beş durum **açıkça tanımlanır ve sınanır**: **süre sonu** (`Prune`), **cure** (Type5 `REMOVE_TYPE4`), **ölüm** (MEC-DTH-01, `WIZ_DEAD`), **görünürlük kaybı** (birim görüşten çıkınca kayıtlar silinmez, `uncertain` işaretlenir) ve **yeniden giriş** (eski kayıtlar işaretli kalır, yeni olay taze kayıtla değiştirir, `INOUT_RESPAWN` temizler). `/bot snap <bot> status` tabloyu döker; böylece çalışma zamanında botun **gözlediği** durum (E sınıfı) sunucunun gerçek buff listesiyle ±2 sn içinde karşılaştırılabilir. Karar/guard/telemetri/yeni paket yok; görünüm alanları (`UnitView`...) ve `check-perception-contract.py` R5 güncellemesi **F4-61**'dir. Bot sistemi kapalıyken (`ENABLED=0`) davranış değişmez.

## 2. Bağlam (okunması zorunlu)

Aşağıdaki dosya:satır referansları `gece/2026-10-02` @ `f4daa27` (kod `0001d04`'ten beri değişmedi) üzerinde 2026-10-03'te `git show` / `git grep -a` ile **yeniden doğrulandı** `[D]`. Uygulayıcı §5 adım 1'deki ön kontrolü yine yapar.

- **Besleme noktası (var):** `GameServer/Bot/BotSession.cpp:89-102` F4-52 bloğu: `opcode == WIZ_MAGIC_PROCESS && pkt.size() >= 23` → `ParseSkillEvent` (kilit dışında) → `m_obsLock` altında `m_skillEvents.Add(ev)`. Aynı blok genişletilir. `OnPacket()` DB iş parçacığında, IOCP'de ya da 30 sn zamanlayıcıda çalışabilir (`BotSession.h:15`); tablolar yalnız `m_obsLock` altında okunur/yazılır.
- **Hangi olaylar gelir (önemli, `[D]`):** Type4 sonucu `ExecuteType4` içinde `pTmp = (pSkillCaster->isPlayer() ? pSkillCaster : pTarget)` biriminin **bölgesine** yayınlanır (`GameServer/MagicInstance.cpp:1862-1874`, `BuildAndSendSkillPacket(pTmp, true, ...)` → `:773-774` `SendToRegion`). Yani gözlemci olayı **çağıranın** 3×3 bölgesindeyse alır; hedef gözlemcinin görüş tablosunda olmayabilir ve görüşündeki bir hedefe çağıran görüş dışındaysa olay hiç gelmez. Bu yüzden kayıt tablosu görüş tablosundan (`m_obs`/`m_npcs`) **bağımsız** tutulur ve görünürlük ayrıca işaretlenir (§3 sözleşme).
- **Bitiş/iptal gözlenemez (`[D]`):** süre bitişi/iptali bildirimi yalnızca hedefe gider: `GameServer/MagicProcess.cpp:989-991` (`TO_USER(pTarget)->Send`); aynı şekilde `MagicInstance.cpp:1990-1992`, `:2806-2808`, `:2864-2866` ve `User.cpp:3476-` yalnızca ilgili kullanıcıya `Send` eder (bölge yayını yok). Başkasının buff'ının bitişi/cure/dispel sonucu bölgeye bildirilmez; yalnızca `olay + süre` tahmin edilir. Cure olayının kendisi (`Type5`) ise `EFFECTING` olarak bölgeye yayınlanır (MEC-MAG-19).
- **Skill tabloları (statik oyun verisi):** `GameServer/GameServerDlg.h:356-361` `m_MagictableArray`, `m_Magictype3Array`, `m_Magictype4Array`, `m_Magictype5Array` (`CSTLMap`, `GetData(id)`, içeride `recursive_mutex`; `GameServer/LoadServerData.h:17-21`). Bot tarafında aynı tablolar zaten okunur: `ActionExecutor.cpp:724` (`m_MagictableArray`), `:746` (`m_Magictype5Array`), `:1421`/`:1463` (`m_Magictype3Array`), `BotManager.cpp:2531`; `m_Magictype4Array` bot kodunda **ilk kez** okunur (R2 sembolü değildir). Okuma **kilit dışında** yapılır.
- **Alan adları (`shared/database/structs.h`):** `_MAGIC_TABLE::bType[2]` (`:21`), `_MAGIC_TYPE3` (`:51-62`: `bDirectType`, `sFirstDamage`, `sTimeDamage`, `bDuration`), `_MAGIC_TYPE4` (`:64-100`: `bBuffType`, `bIsBuff` `:96`; `bIsBuff` tabloda değil yüklemede hesaplanır, `shared/database/MagicType4Set.h:47`, bu yüzden çalışma belleğindeki değer okunur), `_MAGIC_TYPE5` (`:102-`: `bType`).
- **`SkillMeta`/tablo API'si (`BotCore/Perception.h`, F4-53):** `SkillMeta` `:1936`, `StatusObs` `:1985`, `StatusRemainingMs` `:1995`, `ObservedStatusTable` `:2014` (`Observe` `:2058`, `Find` `:2110`, `Collect` `:2128`, `ClearTarget` `:2167`, `Prune` `:2177`, özel `Store` `:2211`), `HealObsRing` `:2307`, `kObsInOutOut = 2` `:17` (yorum: 1 in, 3 respawn, 4 warp, 5 summon), `kNpcInOutOut` `:815`, `SkillEvent` `:1804`. `ObsTable::Find` `:665`, `NpcTable::Find/At/Count` `:1030/:1036/:938`.
- **Mevcut `OnPacket` blokları:** `WIZ_USER_INOUT` `BotSession.cpp:256-279` (OUT: `m_obs.Remove` + `m_hp.Invalidate`, `:263-268`; IN: `m_obs.Upsert`); `WIZ_REGIONCHANGE` `:295-321` (`before[]` kopyası `:301-306`, `Retain` `:308`, düşenler `:309-315`); oyuncu `WIZ_DEAD` `:333-339`; NPC `WIZ_NPC_INOUT` `:352-369` (OUT `:359-363`), `WIZ_NPC_REGION` `:382-389`, NPC `WIZ_DEAD` `:399-405`. Kullanıcı ve NPC kimlik uzayları ayrıdır; `ObservedStatusTable` kimliği tek anahtar kullanır.
- **`ResetForRespawn`** `BotSession.cpp:418-490` (`:484-485` `m_hp.Clear()`, `m_skillEvents.Clear()`); `m_selfSid` (atomik, `TickSessions` yazar) botun kendi kimliği için.
- **`CommandSnap`:** `GameServer/Bot/BotManager.cpp:2552-2806`; kullanım dizgesi `snap <bot> [events]` `:2559` ve `:2566`; `wantEvents` `:2563`; tablo kopyaları kilit altında `:2597-2604`; kendi buff satırı biçimi `:2659` (`"... buff skill=%u type=%u %s remain=%us"`, saniye çözünürlüğü); `events` dökümü `:2779-2798`, `events total=<n>` `:2802`. Betik fiili `snap` zaten izinli (`BotCore/ScriptPlan.h:66`).
- **Kural çıkarımları (`docs/03`, `[D]`):** MEC-BUF-01..07 (her `BuffType` için tek kayıt; yeni debuff eskiyi, buff olsa bile, siler; yeni buff aynı `BuffType`'ta reddedilir; `Update()` başına en çok bir kayıt kalkar ⇒ gerçek kaldırma tahminden geç olabilir); MEC-MAG-19 (cure `EFFECTING` yayını koşulsuzdur, hedefteki **tüm** Type4 debuff'larını kaldırır, buff'ları kaldırmaz); MEC-DTH-01 (ölümde tüm DoT/HoT ve buff/debuff silinir); MEC-BUF-10 (başkasının Type4 etkisi bölge yayınından gözlenir, bitiş tahmin `E`).
- **`docs/13` §5.2a (`:169-197`):** kaynak sınıfları `O` (doğrudan gözlem), `E` (türetilmiş tahmin, bitiş zamanı örnek olarak sayılır), `G` (gerçek sunucu bilgisi; kullanılamaz); `in_region = false` ⇒ görünmez, hedef olamaz; hareketli birimde bayatlama süreleri (burada konum için tanımlıdır; **durum kayıtları için bayatlama sözleşmesi bu planla tanımlanır**, §3).
- **Sözleşme denetimi:** `tools/check-perception-contract.py` yalnız `GameServer/Bot` ve `BotCore` altında R1 (`GetNpcPtr`, `m_RegionUserArray`, `GetRegion`, ...), R2 (`GetUserPtr`, `GetMap`, `m_buffMap`, `isInParty`...; izinli listeye girmeyen kullanım ihlal), R3 (`->m_pUser`), R4 (`BotCore` include), R5 (`UnitView`/`NpcView`/`TeamMemberView` alanları) arar. `m_Magictype*Array` R2 sembolü **değildir**; `StatusObs` R5 kapsamında değildir (`VIEW_FORBIDDEN` yalnız üç görünüm yapısını tarar) ⇒ R5'e dokunulmaz.
- **Doğrulama için gerçek skill verisi (F4-53 §2 ve F4-60'ın ilk sürümü; yerel DB, yalnız skill tabloları, 2026-10-03; Claude çalışma zamanından önce yeniden doğrular):** Fresh mind `112645` `{4, 0}` `Moral 2` `BuffType 8` `Duration 600`; Insensibility peel `112660` `BuffType 2` `Duration 600`; Malice `112703` `{4, 0}` `Moral 7` `BuffType 2` (debuff) `Duration 150` `Range 56`, El Morad `212703` `Range 90`; Great healing `112527` `{3, 0}` `Moral 2` (`FirstDamage 960`); Cure curse `112525` / `212525` `{5, 0}` `Moral 2` (`MAGIC_TYPE5.Type 2`); Ice arrow `110615` `{3, 4}` `BuffType 6` `Duration 12` (kısa süreli debuff: süre sonu ölçümü için); leg cutting `106520` `{1, 4}` `BuffType 6` `Duration 10`. `Moral 2` = tek hedefli dost skill, bot `cast` ile hedef adıyla atılabilir (F4-42 §9.1).
- **Süre taban zamanı:** `ParseSkillEvent` olay zamanını `nowMs` ile damgalar (`std::chrono::steady_clock` ms, `BotSession.cpp:93-97`); `snap` aynı tabanda hesaplar (`BotManager.cpp:2624-2625`).

## 3. Kapsam

### 3.1 Gözlenen bilgi: ne KESİN (O), ne TAHMİNİ (E), ne BİLİNMEZ

| Kaynak olay (bota gelen paket) | **KESİN (O)**: paketin kendisinden | **TAHMİNİ (E)**: türetilmiş | **BİLİNMEZ**: tabloya yazılmaz / sonuç çıkarılmaz |
|---|---|---|---|
| Type4 `EFFECTING`, `data[1] = 1`, `data[3] > 0` | Skill `S` çağıran `C` tarafından hedef `T`'ye uygulandı; olay zamanı `t_obs`; süre `d` sn (paketteki `data[3]`) | Bitiş = `t_obs + d` (**tahmin**: sunucu süreyi `UNIXTIME + d` ile saniye çözünürlüğünde tutar ve `Update()` başına en çok bir kayıt kaldırır ⇒ gerçek bitiş tahminden **sonra** olabilir; cure/dispel/ölüm/süre uzatma ile **önce** olabilir); "şu an etkin" | `BuffType`/`isBuff` paketten değil statik tablodan gelir (istemcide de bulunan veri); etkinin gücü/oranı bilinmez |
| Type4 `EFFECTING`, `data[1] = 0` | Bu uygulama reddedildi (tek hedefte) | — | Alan/grup yolunda reddedilen kurbana yayın hiç gelmez (sessiz atlama): kayıt yokluğu "reddedildi" demek değildir |
| Type4 `{1,4}`/`{3,4}` çifti, Type1/Type3 yarısı | Yalnız vuruş/hasar olayı (`data[3] = 0` ya da `-104`) | — | Tablo **yazmaz** (`data[3] > 0` yalnız Type4 yarısını ayırır) |
| Type5 `REMOVE_TYPE4` (cure) `EFFECTING` | Cure `C` tarafından `T`'ye atıldı | Sunucu kuralı (MEC-MAG-19 `[D]`): `T`'deki **tüm debuff'lar** kalktı ⇒ tablodan debuff kayıtları silinir; buff'lar kalır (kural çıkarımı, güvenilir) | Cure öncesi hiç gözlenmemiş debuff'lar; cure'ün kaldırdığı sayı |
| Type5 `REMOVE_TYPE3` ve diğer Type5 | Olay gerçekleşti | — | DoT/HoT kaydı tutulmaz (kapsam dışı) |
| `WIZ_DEAD` (oyuncu/NPC) | Birim öldü | MEC-DTH-01 `[D]`: tüm buff/debuff silindi ⇒ `ClearTarget` (kural çıkarımı) | Ölüm sonrası (diriltme/yeniden doğuş) etkileri: yeni olayla öğrenilir |
| **Süre sonu** (hiçbir paket gelmez; bitiş yalnız hedefe gider) | **Gözlem yok** | `now >= endMs` ⇒ kayıt ölü sayılır (`Find`/`Collect` yalnız canlıyı döndürür, `Prune` siler) | Gerçek bitiş zamanı (±2 sn hedeflenir, ölçülür) |
| Birim görüşten çıktı (`WIZ_USER_INOUT` OUT, `WIZ_NPC_INOUT` OUT, `WIZ_REGIONCHANGE`/`WIZ_NPC_REGION` listesinde yok) | Birim artık görüş tablosunda yok | Kayıtlar **tutulur** ve `uncertain = true` işaretlenir: bitiş zamanı geçerli sayılabilir ama bu aralıkta gelen cure/üzerine yazma/yeni etki olayları **alınamadı** | Görüş dışındayken olanlar (yeni buff/debuff, cure, üzerine yazma) |
| Birim görüşe girdi (`WIZ_USER_INOUT` IN/WARP, `WIZ_NPC_INOUT` IN, `WIZ_REQ_USERIN`/`REQ_NPCIN` listesi) | Birim görüş tablosunda | Eski kayıtlar `uncertain = true` olarak kalır (**yeniden kullanım işaretli**); aynı `BuffType`'ta yeni olay taze kayıt (`uncertain = false`) yazar | Çıkış-giriş arasındaki olaylar |
| `INOUT_RESPAWN` (3) ile giriş | Birim yeniden doğdu/oturum açtı | Yeni yaşam: önceki kayıtlar silinir (`ClearTarget`; muhafazakâr yön: bilgi kaybı, yanlış bilgi değil) | Kayıtlı büyülerin (`RecastSavedMagic`) sonucu: yeni olayla öğrenilir |
| Hedef görüş tablosunda **yokken** gelen olay (çağıranın bölgesi yayını) | Olay gerçekleşti | Kayıt yazılır ama `uncertain = true` (hedefin gözlemi sürekli değil) | Hedefin görünürlüğü/konumu |
| **Kayıt yokluğu** | **Hiçbir şey söylemez** | — | "Etki yok" sonucu çıkarılamaz (gözlenmeden önceki buff'lar, görüş dışı olaylar, sessiz atlama). Karar katmanı yokluğu `O` sayamaz |

**Karar (Claude kararı, proje sahibi gözden geçirir; ADR-0018 Ek 28 olarak yazılacak):** görünürlük kaybında kayıt **atılmaz, işaretlenir** (`uncertain`). Gerekçe: buff/debuff süreleri 10-600 sn'dir; kısa görüş kaybında bitiş tahmini geçerliliğini korur (silmek doğru bilgiyi de atar, priest gereksiz yeniden buff atar); ama görüş dışında kaçırılabilen olay (cure, üzerine yazma) yanlış pozitif üretir, bu yüzden tüketici işareti görür. `HpTable` farklıdır: HP anlık değerdir, birim çıkınca `Invalidate` edilir (`BotSession.cpp:267`). Seçenek "at" bilgi kaybıdır, seçenek "tut (işaretsiz)" yanlış kesinlik verir: reddedildi.

**Yapılacaklar**

1. **`BotCore/Perception.h`** (yalnızca ekleme, F4-53 bloğunun içinde):
   - `constexpr uint16_t kObsInOutRespawn = 3;   // InOutType: INOUT_RESPAWN` (`kObsInOutOut` yanına; NPC için bir bayt aynı değer).
   - `StatusObs`'a alan: `bool uncertain;   // the unit left view or was never in view since the record: a cure / overwrite event may have been missed` ve `Observe` içinde yeni kayıt oluşturulurken `rec.uncertain = false;` (mevcut alan atamalarının yanına tek satır).
   - `void MarkUncertain(int16_t target)`: hedefin tüm kayıtlarını `uncertain = true` yapar; birim yoksa **hiçbir şey yapmaz** (birim oluşturmaz).
   - `int Targets(int16_t * out, int cap) const`: en az bir kaydı olan birimlerin hedef kimliklerini yuva sırasıyla yazar (≤ `cap`), sayıyı döndürür; `out == nullptr` ya da `cap <= 0` ⇒ 0.
2. **`Tests/BotCoreTests/PerceptionTests.cpp`** (dosya sonuna 3 `TEST_CASE`; §5 adım 3).
3. **`GameServer/Bot/BotSession.h`**: `BotCore::ObservedStatusTable m_status;` ve `BotCore::HealObsRing m_healObs;` — `m_skillEvents` satırının (`:162`) altında, aynı biçimde açıklamalı ("guarded by m_obsLock (the same mutex as m_obs)", ADR-0017 Ek F4-53/F4-60).
4. **`GameServer/Bot/BotSession.cpp`:**
   - Dosya-statik `static bool FillSkillMeta(uint32 skillId, BotCore::SkillMeta & out)` (ilk fonksiyondan önce): `_MAGIC_TABLE * m = g_pMain->m_MagictableArray.GetData(skillId)`; `nullptr` ⇒ `false`. `out` sıfırlanır, `skillId`, `type1 = m->bType[0]`, `type2 = m->bType[1]`. Type4 kısmı (`type1 == 4 || type2 == 4`): `_MAGIC_TYPE4 * t4 = ...m_Magictype4Array.GetData(skillId)`; `nullptr` ⇒ `false` (buffType bilinmeden kayıt anahtarı yoktur); `buffType = t4->bBuffType`, `isBuff = t4->bIsBuff`. Type3 kısmı: `t3` varsa `directType/firstDamage/timeDamage/type3DurationSec` kopyalanır (`nullptr` ⇒ 0, hata değil). `type1 == 5`: `t5` varsa `type5Kind = t5->bType`. Hiçbir işaretçi saklanmaz, tablo satırı değiştirilmez; `m_buffMap`/`GetUserPtr` kullanılmaz.
   - F4-52 bloğunda (`:92-102`), `ParseSkillEvent` başarılıysa ve `ev.op == BotCore::kMagicEffecting && ev.target >= 0` iken kilitten **önce** `FillSkillMeta` (CASTING/FLYING için çağrılmaz). Kilit altında mevcut `m_skillEvents.Add(ev)` aynen kalır; hemen ardından (yeni satırlar): `m_status.Prune(ev.tMs);` (süre sonu: kayıtlar olay geldikçe temizlenir, ayrı zamanlayıcı yok), `StatusUpdate upd = m_status.Observe(ev, haveMeta ? &meta : nullptr);`, `m_healObs.Observe(ev, haveMeta ? &meta : nullptr);` ve görünürlük işareti: `upd == kStatusRecorded` ve `ev.target != m_selfSid.load()` ve hedef `m_obs.Find((uint16)ev.target) == nullptr && m_npcs.Find((uint16)ev.target) == nullptr` ise `m_status.MarkUncertain(ev.target);`.
   - **Ölüm:** yeni küçük blok (F4-52 bloğunun altında): `opcode == WIZ_DEAD && pkt.size() >= 2` → `uint16 id = (uint16)pkt.contents()[0] | ((uint16)pkt.contents()[1] << 8);` kilit altında `m_status.ClearTarget((int16_t)id);`. Mevcut iki `WIZ_DEAD` dalına dokunulmaz (kimlik uzayları ayrı olduğundan tek blok yeter).
   - **Görünürlük kaybı:** mevcut bloklara ek satırlar (kilit zaten alınmış): (a) `WIZ_USER_INOUT` OUT dalında `m_hp.Invalidate(unit.sid);`'in altına `m_status.MarkUncertain((int16_t)unit.sid);`; (b) `WIZ_REGIONCHANGE` dalında `Retain` satırından **sonra** (`:308`'in altı, mevcut tanı döngüsünden bağımsız ayrı döngü): `for (int i = 0; i < beforeCount; i++) if (m_obs.Find(before[i]) == nullptr) m_status.MarkUncertain((int16_t)before[i]);`; (c) `WIZ_NPC_INOUT` OUT dalında `m_hp.Invalidate(npc.id);` altına `MarkUncertain((int16_t)npc.id)`; (d) `WIZ_NPC_REGION` dalında `Retain`'den önce `uint16 before[BotCore::kNpcMaxUnits]` ile mevcut kimlikleri kopyala (`m_npcs.Count()`/`At(i).id`), `Retain` sonrası tabloda bulunmayanlara `MarkUncertain`.
   - **Yeniden giriş:** `WIZ_USER_INOUT` IN/RESPAWN dalında (`m_obs.Upsert(unit)` yanında) `if (type == BotCore::kObsInOutRespawn) m_status.ClearTarget((int16_t)unit.sid);`; `WIZ_NPC_INOUT` aynı (`type == BotCore::kObsInOutRespawn` ⇒ `ClearTarget(npc.id)`). Diğer girişlerde (IN=1, WARP=4, summon=5) tabloya **dokunulmaz**: eski kayıtlar işaretli kalır.
   - `ResetForRespawn` içinde (`:484-485`'in yanında, aynı kilit bloğu): `m_status.Clear(); m_healObs.Clear();`.
5. **`GameServer/Bot/BotManager.cpp` — `CommandSnap`:** kullanım dizgeleri `snap <bot> [events|status]`; `wantStatus = (words.size() == 2 && words[1] == "status")`; `events` ve `status` birlikte verilemez (ikisinden biri değilse mevcut kullanım iletisi). `wantStatus` iken tablolar kilit altında **kopyalanır** (`statusCopy = s->m_status; healCopy = s->m_healObs;`, yalnız bu atamalar), biçimlendirme kilit dışında; `statusCopy.Prune(nowMs)` kopyada çağrılır. Çıktı (mevcut `snap` çıktısının sonuna, `events` dalının yerine; `events total=<n>` satırı `status` iken yazılmaz, `events` dalı değişmez):
   - `status units=<n> records=<n> uncertain=<n> heals_total=<n> heals_in_ring=<n>`;
   - her birim için (`Targets()` ile, en çok 10 birim) her canlı kayıt (`Collect(target, nowMs, out, 8)`): `status target=<id> view=<in|out|self> skill=<id> type=<buffType> <buff|debuff> caster=<id> remain=<ms>ms age=<ms>ms src=E unc=<0|1>` (`view`: hedef `obsCopy`/`npcCopy`'de varsa `in`, botun kendisiyse `self`, değilse `out`);
   - heal halkasının en yeni ≤ 5 olayı: `status heal age=<ms> skill=<id> caster=<id> target=<id> nominal=<n> hot=<0|1>`.
   Argümansız `snap` ve `snap <bot> events` çıktıları **değişmez**. Mesaj tamponu 320 baytı geçmemeli (`snprintf`).
6. **Çalışma zamanı doğrulaması** (Claude yapar, §6 K9): DeepSeek sunucu açmaz; yalnız derleme + birim test.

**Kapsam dışı (yapılmayacak)**

- `UnitView`, `NpcView`, `TeamMemberView`, `PerceptionSnapshot`'a durum alanı eklemek, `BuildSnapshot`/`AttachHp` değişikliği, `tools/check-perception-contract.py` R5 güncellemesi: **F4-61**.
- Type3 DoT/HoT hedef kaydı, `REMOVE_TYPE3` etkisi; `MAGIC_DURATION_EXPIRED`/iptal paketleri; periyodik zamanlayıcıyla `Prune`; bitiş tahmininin hata payı **istatistiği** (yalnız bir kez elle ölçülür, §6 K9).
- Karar/telemetri (`decisions`), takım içi paylaşım (`P`), `UseItem`/yeni cast dilimi, `ScriptPlan.h` fiil listesi, yeni ini anahtarı, yeni komut (yalnız `snap` ikinci argümanı), yeni thread, yeni paket isteği.
- `GameServer/` içinde `Bot/` dışındaki dosyalar, `shared/`, `AIServer/`, `docs/`, `tools/`: değişmez (Claude plan sonrası `docs/13` §5.2a'ya "durum kayıtları: kaynak sınıfı, `uncertain`, süre sonu" satırı ve ADR-0018 Ek 28'i yazar).
- Mevcut hiçbir satırın silinmesi/yeniden biçimlendirilmesi (kullanım dizgesi iki satırı hariç).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/Perception.h` | değiştir | yalnızca ekleme: `kObsInOutRespawn`, `StatusObs::uncertain` (+ `Observe`'ta `rec.uncertain = false;`), `MarkUncertain`, `Targets` |
| `Tests/BotCoreTests/PerceptionTests.cpp` | değiştir | yalnızca dosya sonuna 3 `TEST_CASE` |
| `GameServer/Bot/BotSession.h` | değiştir | `m_status`, `m_healObs` |
| `GameServer/Bot/BotSession.cpp` | değiştir | `FillSkillMeta`, besleme + `Prune` + görünürlük işareti, `WIZ_DEAD` bloğu, OUT/REGIONCHANGE/NPC işaretleri, RESPAWN temizliği, `ResetForRespawn` |
| `GameServer/Bot/BotManager.cpp` | değiştir | yalnızca `CommandSnap` |

Yeni dosya yok ⇒ `.vcxproj`/`.filters` değişmez. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F4-60 gece/2026-10-02`; `Durum` → `UYGULANIYOR`; `./tools/run-servers.sh status` `[UP]` ise `./tools/run-servers.sh stop`. Test sayısını not et: `./tools/run-tests.sh Release 2>&1 | tail -3` (beklenen `259 tests, 0 failed`). **Ön kontrol (satır numaraları kaymış olabilir):** `git grep -n -a -E "m_skillEvents|WIZ_DEAD|m_hp.Invalidate|m_obs.Retain|m_npcs.Retain" -- GameServer/Bot/BotSession.cpp`; `git grep -n -a "wantEvents" -- GameServer/Bot/BotManager.cpp`; `git grep -n -a -E "class ObservedStatusTable|struct StatusObs" -- BotCore/Perception.h`. Aranan yapılar planda anlatıldığı gibi değilse **dur ve raporla**.
2. `Perception.h` (CRLF, sekme, Allman, İngilizce yorum): `kObsInOutRespawn`; `StatusObs::uncertain` ve `Observe`'ta `rec.uncertain = false;`; `MarkUncertain`, `Targets` (imzalar §3.1 bağlayıcıdır).
3. Testler (`PerceptionTests.cpp` sonu; mevcut `MakeEvent`/`SkillMeta` yardımcı düzenini yeniden kullan; adlar bağlayıcı):
   1. `Perception_Status_Targets`: boş tablo ⇒ 0; `nullptr`/`cap 0` ⇒ 0; hedef `2985` (iki kayıt: `buffType 1`, `2`) ve `2986` (bir kayıt) ⇒ `Targets` 2, ikisi de listede (yuva sırası: ilk eklenen önce); `cap 1` ⇒ 1; `ClearTarget(2985)` sonrası yalnız `2986`; cure ile son debuff'ı silinen birim listede **yok**; 33. hedefte sayı 32'de kalır.
   2. `Perception_Status_Uncertain`: Malice benzeri debuff (`{112703, 4, 0, buffType 2, isBuff false}`, `t = 1000`, `data[1] = 1`, `data[3] = 150`) ve HP/MP benzeri buff (`{112654, 4, 0, buffType 1, isBuff true}`, `t = 2000`, `data[3] = 600`) hedef `2985` ⇒ `Collect` iki kayıt, ikisi `uncertain == false`; `MarkUncertain(2985)` ⇒ ikisi `true`, `Units()` aynı; `MarkUncertain(2999)` (kayıtsız hedef) ⇒ `Units()` hâlâ 1 ve `Find(2999, ...) == nullptr` (birim oluşmaz); aynı `buffType 2`'de yeni debuff olayı (`t = 9000`, `data[3] = 150`) ⇒ o kayıt `uncertain == false`, `endMs 159000`, buff kaydı hâlâ `true`; ardından `MarkUncertain` + cure (`Type5 kType5RemoveType4`, `t = 10000`, `data[1] = 0`) ⇒ `kStatusCured`, debuff gitti, buff kaldı ve `uncertain == true`; `Prune(700000)` ⇒ `Records() == 0`; kopya bağımsız (kopyadan sonra kaynakta `MarkUncertain` kopyayı değiştirmez).
   3. `Perception_Status_Lifecycle` (BotSession kanca sırasını modeller; yorumda "mirrors BotSession hooks" yaz): (a) **süre sonu**: 10 sn'lik debuff (`t = 0`, `data[3] = 10`): `Find(.., 9999)` dolu, `Find(.., 10000)` `nullptr`, `CountDebuffs(.., 10000) == 0`, `Records()` hâlâ 1, `Prune(10000)` ⇒ 0 (sınır: `endMs > nowMs`); (b) **görünürlük kaybı + yeniden giriş**: buff `t = 0`, `data[3] = 600`; `MarkUncertain` `t = 20000` (çıkış), girişte tabloya çağrı yok, `t = 40000`: `Find` dolu, `uncertain == true`, `StatusRemainingMs == 560000`; `t = 41000` yeni aynı-`buffType` olay ⇒ `uncertain == false`, `endMs 641000`; (c) **ölüm**: iki hedefte kayıt, `ClearTarget(2985)` ⇒ yalnız `2986` kalır, `Records()` düşer; (d) **respawn girişi**: kayıtlar, `kObsInOutRespawn == 3` ve `ClearTarget` ⇒ `Find == nullptr`; (e) **görüş dışında cure**: debuff `t = 0`, `MarkUncertain` `t = 5000`, cure olayı tabloya **hiç gelmez** (görüş dışı); `t = 30000` `Find` hâlâ dolu ve `uncertain == true` (bilinen yanlış pozitif: bayrak bunun içindir), sonra görüş içinde gelen cure `kStatusCured` ⇒ silinir; (f) **yokluk**: hiç gözlenmemiş hedefte `Find == nullptr`, `Units() == 0` (yorum: "absence is not 'no effect'").
   - Beklenen test sayısı `259 → 262`.
4. `BotSession.h/.cpp` (§3.3-§3.4). Kod iskeleti (imzalar bağlayıcı, gövdeler sende):

```cpp
// BotSession.cpp, file scope, before the constructor
static bool FillSkillMeta(uint32 skillId, BotCore::SkillMeta & out);

// inside OnPacket(), the F4-52 block (after ParseSkillEvent succeeded):
//   BotCore::SkillMeta meta;
//   bool haveMeta = false;
//   if (ev.op == BotCore::kMagicEffecting && ev.target >= 0)
//       haveMeta = FillSkillMeta(ev.skillId, meta);        // static tables, outside m_obsLock
//   { lock m_obsLock;  m_skillEvents.Add(ev);              // existing line stays
//     m_status.Prune(ev.tMs);
//     BotCore::StatusUpdate upd = m_status.Observe(ev, haveMeta ? &meta : nullptr);
//     m_healObs.Observe(ev, haveMeta ? &meta : nullptr);
//     if (upd == BotCore::kStatusRecorded && ev.target != m_selfSid.load()
//         && m_obs.Find((uint16)ev.target) == nullptr && m_npcs.Find((uint16)ev.target) == nullptr)
//         m_status.MarkUncertain(ev.target); }
```

5. `BotManager.cpp` `CommandSnap` (§3.5). Çıktı satırlarının önekini mevcut satırlarla aynı yaz: `"BotManager: cmd snap:   status ..."` (üç boşluk).
6. Derleme ve test (§7). Çalışma zamanı doğrulaması Claude'un işidir (K9): `Durum` → `UYGULANDI`, Uygulayıcı Raporu.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0; beş dosya `touch` edilip yeniden derlendiğinde değişen dosyalarda yeni uyarı yok (yalnızca eski `UpgradeHandler.cpp` C4789 olabilir)
- [ ] K2: `./tools/build.sh Debug` rc=0, yeni uyarı yok
- [ ] K3: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`, test sayısı plan başındakinden **3 fazla** (`259` → `262`); `Perception_Status_Targets`, `Perception_Status_Uncertain`, `Perception_Status_Lifecycle` her iki yapılandırmada `[ OK ]`
- [ ] K4 (`BotCore` saflığı, yalnızca ekleme): `git diff gece/2026-10-02...bot/F4-60 -- BotCore | grep '^+' | grep -E '#include|windows\.h|stdafx|GameServer|shared/'` boş; `Perception.h` farkında silinen satır 0 (`grep -c '^-[^-]'` = 0); F4-53'ün `Observe`/`Store` mantığı (satırlar) değişmedi, yalnız `rec.uncertain = false;` eklendi
- [ ] K5 (sözleşme): `python3 tools/check-perception-contract.py` `RESULT: PASS`, R1 0, R2 0 ihlal (28 izinli), R3 0 ihlal (18 izinli), R4 0, R5 0; `--selftest` rc=0; eklenen satırlarda `GetUserPtr|m_buffMap|m_pUser->|GetRegion|m_RegionUserArray|isInParty|GetMap(` yok; `BotSession.cpp`'de skill tablosu erişimi yalnız `FillSkillMeta` içinde (`grep -n 'm_Magictable\|m_Magictype' GameServer/Bot/BotSession.cpp` yalnız o fonksiyonun satırları)
- [ ] K6 (kapsam): `git diff --stat gece/2026-10-02...bot/F4-60` yalnızca §4'teki 5 dosya ve bu plan dosyasını gösterir (`shared/`, `AIServer/`, `tools/`, `docs/`, `ScriptPlan.h`, `UnitView` yok)
- [ ] K7 (kilit ve kanca): `grep -n 'm_status\|m_healObs' GameServer/Bot/*.cpp GameServer/Bot/*.h`: her kullanım `m_obsLock` altında (BotSession.cpp besleme/`WIZ_DEAD`/OUT/REGIONCHANGE/RESPAWN/`ResetForRespawn`; BotManager.cpp yalnız kilit altındaki kopya ataması); `FillSkillMeta` çağrısı kilit **dışında** ve yalnız `kMagicEffecting` + hedefli olaylarda; `MarkUncertain` çağrıları tam olarak §3.4'teki dört görünürlük kaybı noktasında (USER OUT, REGIONCHANGE, NPC OUT, NPC_REGION) ve besleme bloğunda; `ClearTarget` çağrıları `WIZ_DEAD`, iki RESPAWN girişi ve `Clear`'da
- [ ] K8 (kurallar): eklenen satırlar ASCII; beş dosya baştan sona CRLF (`file`; satır sayısı = CRLF sayısı); sekme girinti, Allman; `git diff --check` boş; yeni `printf` (`snprintf` dışı)/`Sleep`/`CreateThread`/`rand(` yok; yeni ini anahtarı/komut/thread/paket isteği yok; mevcut `WIZ_DEAD` dalları, `m_castEcho` bloğu ve `m_skillEvents.Add` satırı değişmedi (`-` satırları yalnızca `snap` kullanım dizgesi)
- [ ] K9 (çalışma zamanı, Claude yapar; `[BOT] ENABLED=1`, `TELEMETRY=decisions`, zone 71, hepsi yakın; iş bitince `run-servers.sh stop`; "gerçek" = hedef botun kendi `snap <bot>` buff satırı `buff skill=… remain=…s` (saniye çözünürlüğü) ve `list`; hata bütçesi: sunucu saniye çözünürlüğü + liste yuvarlaması ≈ 2 sn). Hedef: gözlemci tablosundaki `remain` ile gerçek liste `remain` farkı **≤ 2 sn**; ≤ 2 sn aşılırsa sapma ölçülür, plan başarısız sayılmaz ama bulgu (kaynak: tahmin hata payı) rapora yazılır ve ±sınırı revize önerisi çıkar. Senaryolar:
  - **S1 (kayıt ve ±2 sn):** `cast BotPHD_K 112645 BotWP_K` sonrası gözlemci `BotPHB_K` ve `BotWG_K` için `snap <bot> status`: `target=<BotWP_K sid> view=in skill=112645 type=8 buff caster=<BotPHD_K sid> src=E unc=0`, `remain` ≈ 600000 − yaş, `BotWP_K`'nın gerçek `remain`'i ile fark ≤ 2 sn.
  - **S2 (debuff):** `cast BotPHD_K 112703 BotWP_E` (düşman): gözlemcide `debuff type=2 remain` ≈ 150000, `BotWP_E`'nin gerçek `type=2 debuff` satırıyla fark ≤ 2 sn; reddedilen/hatalı atışta (`CastOutcome` `srv_fail`) kayıt oluşmaz.
  - **S3 (süre sonu, Prune):** kısa süreli debuff (Ice arrow `110615`, 12 sn; uygun büyücü bot Claude'un skill kataloğundan seçilir, `[Ö]` bot adı) bir düşmana: `remain` ≈ 12000'den azalır; `t_obs + 12 sn ± 2 sn` içinde `status target=<id> ...` satırı **kaybolur** ve gerçek listeden de aynı ±2 sn içinde kalkar; sonraki bir `EFFECTING` olayından sonra `records=` sayısı düşer (olay beslemesindeki `Prune`); yeni bir olay gelmeden `snap status` yine yalnız canlıyı gösterir.
  - **S4 (cure):** El Morad priest (`BotPHB_E`; `bad_skill` ise başka El Morad priest) `212703`'ü `BotWP_K`'ya atar (menzil 90), ardından `BotPHD_K` `112525` (Cure curse) `BotWP_K`'ya: gözlemcide debuff kaydı **silinir**, buff'lar (`type=8`) kalır; `BotWP_K` gerçek listesinde `type=2` yok.
  - **S5 (ölüm):** buff'lı bir bot öldürülür (rakip bot `attack`/skill): `WIZ_DEAD` sonrası gözlemcide o hedefin tüm `status target=` satırları kaybolur (`units` azalır); `regene` ile dirilince eski kayıt **geri gelmez**; yeni cast taze kayıt (`unc=0`) yazar. Gerçek listede ölüm sonrası buff yok (MEC-DTH-01).
  - **S6 (görünürlük kaybı):** gözlemci `O`, hedef `X` (`BotWP_K`, S1'den 600 sn'lik buff'lı): `X` `move` ile `O`'nun 3×3 bölgesinin dışına çıkarılır (Claude `see` ile `O`'nun artık `X`'i listelemediğini doğrular): `snap O status` ⇒ `target=X` satırı **durur** (`view=out unc=1`, `remain` sürer, atılmaz). Görüş dışındayken, `O`'dan da uzak bir yerde başka bir bot `X`'e farklı bir buff atar (`O` için `events total` değişmez: olay gelmedi): dönüşte bu yeni buff `O`'nun tablosunda **yok** (yokluk ≠ yok), gerçek listede **var** ⇒ "kayıt yokluğu bilgi değildir" satırının kanıtı.
  - **S7 (yeniden giriş):** `X` geri `move` ile `O`'nun görüşüne döner: `see O` `X`'i listeler ⇒ `snap O status`: eski kayıt `view=in unc=1`, `remain` gerçek listeyle ≤ 2 sn (bitiş tahmini görüş kaybından etkilenmedi); aynı `buffType`'ı yeniden atınca (`unc=0`, yeni `endMs`, gerçek listeyle ≤ 2 sn) taze kayıt eskiyi **değiştirir** (tek satır). Görüş dışında bir **cure** atılıp (bir debuff'lı `X` + `O` görüş dışı iken cure) dönüşte eski debuff kaydı `unc=1` olarak **hâlâ görünür** ama gerçek listede yoktur: bayrak bu yanlış pozitifi işaretlemek içindir (kayıt raporlanır, hata sayılmaz).
  - **S8 (heal):** `BotWP_E` `BotWP_K`'ya birkaç vurduktan sonra `cast BotPHD_K 112527 BotWP_K`: `status heal ... skill=112527 target=<BotWP_K sid> nominal=960 hot=0`, `heals_total` 1 artar (hedef tam canlıysa sunucu reddedebilir: `[Ö]`).
  - **S9 (kopya ve gerileme):** iki ardışık `snap <bot> status` aynı kayıtları gösterir (`remain` azalır); argümansız `snap` (`events total=<n>` satırı aynı), `snap <bot> events`, `see`, `cast`, `list` çalışır; hatalı `snap <bot> foo` kullanım iletisini (`[events|status]`) yazar; `Bot_*.log`'da bu oturumda `WARN`/`ERROR` 0.
  - **S10 (hata payı kaydı):** S1, S2, S3, S7'deki `E` bitiş ile gerçek `remain` farkları tek tabloya yazılır (doğrulama raporuna; `docs/05`'e yazılmaz).
- [ ] K10 (Claude, doğrulama sonrası): `docs/13` §5.2a'ya "durum kayıtları" satırı (O/E/uncertain, süre sonu, ölüm, görünürlük kaybı, yeniden giriş tablosu §3.1) ve ADR-0018 Ek 28 yazılmış; `docs/STATUS.md` güncel (DeepSeek `docs/`'a dokunmaz)

## 7. Doğrulama komutları

```bash
./tools/run-tests.sh Release 2>&1 | tail -3     # plan başında sayıyı not et (259)
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release && ./tools/run-tests.sh Debug
python3 tools/check-perception-contract.py | sed -n '1,14p'
python3 tools/check-perception-contract.py --selftest; echo rc=$?
git diff --stat gece/2026-10-02...bot/F4-60
git diff gece/2026-10-02...bot/F4-60 -- BotCore/Perception.h | grep -c '^-[^-]'      # beklenen 0
grep -n 'm_status\|m_healObs' GameServer/Bot/BotSession.cpp GameServer/Bot/BotManager.cpp GameServer/Bot/BotSession.h
grep -n 'MarkUncertain\|ClearTarget' GameServer/Bot/BotSession.cpp
grep -n 'm_Magictable\|m_Magictype' GameServer/Bot/BotSession.cpp
file BotCore/Perception.h Tests/BotCoreTests/PerceptionTests.cpp GameServer/Bot/BotSession.h GameServer/Bot/BotSession.cpp GameServer/Bot/BotManager.cpp
git diff --check gece/2026-10-02...bot/F4-60
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3 (ASCII, CRLF, sekme, Allman, İngilizce yorum). Dosyalar CRLF'dir: ekleme araçları satır sonunu bozmamalı (`file` ile denetle).
- Tablo okuma (`m_Magictable*Array.GetData`) `m_obsLock` **dışında** yapılır; `m_obsLock` altında yalnızca kopyalama/ekleme/işaretleme (tick süresini uzatma). `FillSkillMeta` yalnızca değer kopyası döndürür, tablo satırı işaretçisi saklamaz.
- Tüm kayıtlar `E` sınıfıdır ve **tahmindir** (başkasının buff'ı sunucuda tahminden önce ya da sonra bitebilir: MEC-BUF-07; iptal görünmez). `snap status` çıktısında `src=E` yazılır; bunu gerçek buff listesi saymayın (kod yorumuna yaz). Kayıt yokluğu "etki yok" değildir (§3.1 son satır).
- Bot yalnızca kendisine gelen paketlerden öğrenir: `FillSkillMeta` oyuncu kimliği ya da konumu okumaz; başka oturumun `CUser`'ına erişmez; görünürlük işareti yalnız botun **kendi** görüş tablolarını (`m_obs`, `m_npcs`) okur. Bot kendi hedefi için (`ev.target == m_selfSid`) işaret koymaz (kendi buff'ı için gerçek kaynak `SelfState`, `FillSelfExtras`).
- `INOUT_RESPAWN` ile temizleme **muhafazakâr** bir seçimdir: bu aynı zamanda bölge değişimi/diriltme becerisi gibi canlı birimlere de gelebilir (`MagicInstance.cpp:2384`, `CharacterMovementHandler.cpp:686`); sonuç bilgi kaybıdır (yanlış bilgi değil), yeni olayla yeniden öğrenilir. Uygulayıcı bunu değiştirmez.
- Heal olaylarında `ev.data[]`'ya güvenilmez (F4-53 §8); heal beslemesi yalnız `skillId/caster/target`'a dayanır.
- Kapsam büyütme: `UnitView` alanı, karar/telemetri, periyodik `Prune`, DoT/HoT kaydı. Gerekirse Uygulayıcı Raporu'nda "açık soru" olarak yaz.
- F4-55 (giriş el sıkışması, `BotManager.cpp` `TickSessions`) ile aynı dosyada farklı fonksiyonlara dokunur; sıra bağımsız birleştirilebilir.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum:
- Branch / commit'ler:
- Değişen dosyalar ve neden:
- Derleme sonucu:
- Kabul kriterleri öz-değerlendirme (K9, K10: Claude yapar):
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
