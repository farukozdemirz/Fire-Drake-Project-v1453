# F4-17: `Perception` dilim 6 — `SelfState` genişletme (pot stoku, pot/cast soğuması, buff listesi) ve `/bot snap` çıktısı

| Alan | Değer |
|---|---|
| Durum | DOĞRULANDI |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2) |
| Branch | `bot/F4-17` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-16 (`PerceptionSnapshot`, `SelfState`, `/bot snap`) — `KAPANDI` (merge `bee4fe4`); F4-04 (pot dilimi, `BeginPotion`, `PotionWaitMs`) — `KAPANDI`; F4-03 (cast dilimi, `m_castSkillLast`, `CastRecastMs`, `kCastGapMs`) — `KAPANDI`; F3-05 (`BotCore`, birim test çatısı) — `KAPANDI` |
| İlgili gereksinim / kabul | `docs/13` §5.2 (`SelfState`: buffs, cooldowns, stock), `docs/14` §5.1 "Öz durum" ve §5.2 gözlem sözleşmesi (botun **kendi** pot stoku/soğuması/buff'ı serbest, **başkalarınınki** yasak), AC-LRN-03 / AC-ARCH-06 (statik denetim) |
| Tahmini büyüklük | S–M (5 kod dosyası; yeni dosya yok, `*.vcxproj` değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-02 (otonom gece döngüsü) |

---

## 1. Amaç

F4-16'daki `SelfState` botun yalnızca HP/MP/konum/ulus/sınıf/seviye/ölü/oturuyor bilgisini taşıyor. Karar katmanı (sonraki fazlar) "pot içebilir miyim, bu skill hazır mı, üzerimde hangi buff var" sorularını **kendi durumundan** cevaplayabilmeli (`docs/13` §5.2). Bu plan `SelfState`'e şunları ekler:

- **pot stoku:** botun kendi çantasındaki HP ve MP pot adedi (yalnızca botun bugün gerçekten içebileceği pot türleri sayılır: `BeginPotion`'ın kabul ettiği türler);
- **soğuma:** ortak pot zamanlayıcısından kalan süre (`potWaitMs`), iki cast arası boşluktan kalan süre (`castGapWaitMs`) ve skill başına kalan yeniden-kullanım süresi (`cooldowns[]`, yalnızca kalan süresi > 0 olanlar);
- **buff listesi:** botun üzerindeki Type4 buff/debuff'lar (`buffs[]`: skill kimliği, buff tipi, buff mı debuff mı, kalan saniye).

Saf mantık (`BotCore/Perception.h`) yalnızca veri yapılarını ve küçük, birim testli yardımcıları taşır; sunucu nesnelerini okuyan kısım `BotManager.cpp`'de **tek bir dosya-statik fonksiyondadır** ve `/bot snap` çıktısı bu alanları yazdırır. Karar/guard/telemetri bu planda **yoktur**.

## 2. Bağlam (okunması zorunlu)

- `docs/14_LEARNING_AND_ADAPTATION.md` §5.2 — gözlem sözleşmesi: yasak olan **düşmanın** MP'si/cooldown'u/envanteri/pot stoku; botun kendi durumu (§5.1 "Öz durum") serbesttir. Bu plan yalnızca botun kendi `CUser`'ını ve kendi `BotSession`'ını okur.
- `docs/13_BOT_ARCHITECTURE_AND_DATA_MODEL.md` §5.2 — `PerceptionSnapshot`/`SelfState` taslağı (buffs, cooldowns, stock). Uygulama notu F4-16'dan kalma; Claude günceller.
- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` Ek F4-16 madde 5 ve 8 — `SelfState`'in buff/cooldown/stok alanları "sonraki dilim" olarak ayrılmıştı; Claude Ek F4-17'yi yazar.
- İlgili kod (hepsini açıp doğrula; satırlar `bee4fe4` itibarıyla):
  - `BotCore/Perception.h:783-798` — `kSnapMaxUnits`/`kSnapMaxNpcs`/`kSnapUserSit` ve mevcut `SelfState`; `:878-` `BuildSnapshot` (`memset(&out,0,…)` sonra `out.self = self`, yani `SelfState`'e eklenen her alan kopyalanır).
  - `BotCore/BotCombat.h:260-299` — `kPotCooldownMs = 2500`, `PotSupported`, `PotionCheck`, `PotionWaitMs`; `:126` `kCastGapMs = 140`, `CastRecastMs(reCastTime)` (0,1 sn birimi → ms).
  - `GameServer/Bot/BotSession.h:98` `m_castSkillLast` (skill kimliği → son EFFECTING zamanı), `:101-102` `m_castAnyHas`/`m_castAnyLast`, `:111-112` `m_potHasLast`/`m_potLast`. Hepsi "IOCP thread only"; `/bot` komutları IOCP thread'inde çalışır (ADR-0015), yani `CommandSnap` bunları kilitsiz okuyabilir. `BotSession.cpp:317-328` (`ResetForRespawn`) bu alanları temizler.
  - `GameServer/Bot/ActionExecutor.cpp:962-974` `CountInBag` (çanta slotları `INVENTORY_INVENT .. INVENTORY_INVENT + HAVE_MAX`), `:1077-1170` `BeginPotion` (pot türü doğrulaması; `:1135-1149` `supported` ifadesi), `:1198-1210` guard'ın pot "since" hesabı (örnek).
  - `GameServer/Unit.h:320-322` `m_buffMap` (`Type4BuffMap` = `std::map<uint8, _BUFF_TYPE4_INFO>`, anahtar buff tipi) ve `m_buffLock` (`std::recursive_mutex`); `shared/database/structs.h:314-327` `_BUFF_TYPE4_INFO` (`m_nSkillID`, `m_bBuffType`, `m_bIsBuff`, `m_tEndTime`); `GameServer/MagicInstance.cpp:1812` `m_tEndTime = UNIXTIME + sDuration` (hep sonlu); `shared/TimeThread.h:8` `extern time_t UNIXTIME` (saniye); `GameServer/User.cpp:3485` `m_tEndTime > UNIXTIME` örneği.
  - `GameServer/Bot/BotManager.cpp:2416-2535` `CommandSnap` (F4-16): `SelfState` doldurma `:2457-2472`, yazdırma `:2480-2535`.
- **Önemli:** F4-16'nın statik sözleşme denetimi (K5) `CommandSnap` gövdesinde `g_pMain|GetItem(|m_buffMap|m_CoolDownList` aramasının **boş** çıkmasını istiyordu. Bu plan o denetimi bozmaz: botun kendi çantasına/buff'ına/skill tablosuna erişen kod `CommandSnap` gövdesinde **değil**, ayrı dosya-statik `FillSelfExtras` fonksiyonundadır; `CommandSnap` yalnızca onu çağırır.

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/Perception.h`):** sabitler `kSnapMaxBuffs = 16`, `kSnapMaxCooldowns = 16`; yapılar `BuffView`, `CooldownView`; `SelfState`'e eklenen alanlar; yardımcılar `SnapRemainingSec`, `SnapRemainingMs`, `SelfAddBuff`, `SelfAddCooldown`. Dört birim testi (`PerceptionTests.cpp`'ye).
2. **Pot sınıflandırması (`ActionExecutor`):** `BeginPotion`'ın `supported` ifadesi dosya-statik `PotMagicSupported(...)` fonksiyonuna taşınır (davranış aynı kalır) ve yeni `public static uint8 ActionExecutor::PotKindOf(CUser *, uint32 itemId)` aynı kuralı kullanır (0 = bu bot bu itemi pot olarak içemez, 1 = HP, 2 = MP).
3. **Komut (`BotManager.cpp`):** dosya-statik `FillSelfExtras(BotSession *, CUser *, now, SelfState &)` yeni alanları doldurur; `CommandSnap` onu çağırır ve günlüğe bir özet satırı + her listenin ≤ 10 kaydını yazar.
4. **Dokümantasyon ve kayıtlar Claude'un işi** (DeepSeek dokunmaz): ADR-0017 Eki F4-17, `docs/13` §5.2 notu, STATUS, README.

**Kapsam dışı (yapılmayacak)**

- Alanları kullanan **karar, guard, politika, telemetri olayı** yok. `PerceptionSnapshot` periyodik kurulmaz; yalnızca `/bot snap` kurar. `Tick()`/`TickSessions()` değişmez.
- **Başkalarının** buff/cooldown/stok bilgisi yok (sözleşme yasağı). `UnitView`/`NpcView` değişmez.
- Tip kapısı (MEC-MAG-03, aynı tipli skill'ler arası 1000 ms) kalan süresi **eklenmez**: yalnızca skill başına yeniden-kullanım süresi, iki cast arası boşluk ve ortak pot süresi vardır. (Karar katmanı isterse sonraki dilimde eklenir; ADR Eki'nde not düşülür.)
- Pot dışı envanter (silah, okçu oku/taş, scroll, kristal), SP, hız, Type9 buff'lar, `m_CoolDownList` (sunucunun kendi soğuma tablosu: bot kendi `m_castSkillLast`'ını kullanır, guard ile aynı kaynak) **okunmaz**.
- `team` (`TeamView`), `nav` (`NavView`), party HP, başkalarının `WIZ_STATE_CHANGE` yayını yok.
- `BotSession.*`, `Telemetry.*`, `ScenarioRunner.*`, `BotManager.h` **değişmez**. `ActionExecutor`'da `BeginPotion`'ın dönüş değerleri, neden metinleri ve sırası değişmez (yalnızca `supported` ifadesinin yeri değişir).
- Yeni `GameServer.ini` anahtarı yok. `ENABLED=0` iken davranış değişmez. Yeni dosya, yeni `vcxproj` satırı yok.
- Dokümanlar (`docs/03`, `docs/13`, `docs/14`, `docs/16`, `docs/KNOWN_ISSUES.md`, ADR): Claude'un işi.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/Perception.h` | değiştir | yalnızca ekleme: yeni sabitler/yapılar `SelfState`'in hemen üstüne, `SelfState`'e alan eklemesi, yardımcılar `BuildSnapshot`'tan sonra dosya sonuna (`namespace BotCore` içinde) |
| `Tests/BotCoreTests/PerceptionTests.cpp` | değiştir | yalnızca ekleme: dört `TEST_CASE` (dosya sonuna) |
| `GameServer/Bot/ActionExecutor.h` | değiştir | yalnızca ekleme: `PotKindOf` bildirimi (`BeginPotion`'ın üstüne) |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | `PotMagicSupported` (yeni, dosya-statik), `BeginPotion` içinde `supported` çağrısı, `PotKindOf` tanımı |
| `GameServer/Bot/BotManager.cpp` | değiştir | `FillSelfExtras` (yeni, dosya-statik, `CommandSnap`'in hemen üstüne), `CommandSnap` içinde çağrı + yazdırma |

(Dokunulacak dosya sayısı 5; plan dosyası dahil 6.) `BotCore.vcxproj`, `BotCoreTests.vcxproj`, `proj-GameServer.vcxproj*`, `BotManager.h`, `BotSession.*`, `Telemetry.*`, `ScenarioRunner.*` **değişmez**. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

### 5.1 Branch

`git switch -c bot/F4-17 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop` (exe kilidi, `AGENTS.md` §4).

### 5.2 `BotCore/Perception.h` ve birim testleri

Biçim: dosyadaki mevcut kod gibi (tab, Allman, `inline`, İngilizce kısa yorum, `namespace BotCore` içinde, ASCII + CRLF). **Yeni include yok** (`<cstdint>`, `<cstring>` zaten var). `std::min/max`, `std::vector`, `std::string`, `new`, `malloc` yok. Dosya sunucusuz kalır.

**(a) Sabitler ve yapılar** (`SelfState`'in hemen üstüne; mevcut `kSnap*` sabitlerinin yanına `kSnapMaxBuffs`/`kSnapMaxCooldowns` da konabilir):

```cpp
constexpr int kSnapMaxBuffs     = 16;   // type 4 buffs/debuffs kept in the self state (design limit)
constexpr int kSnapMaxCooldowns = 16;   // skills with a running reuse timer kept in the self state

// One type 4 buff or debuff on the bot itself (the client shows both as status icons).
struct BuffView
{
	uint32_t skillId;
	uint8_t  buffType;       // BUFF_TYPE_* (key of the server's buff map)
	bool     isBuff;         // false = debuff
	uint32_t remainingSec;   // > 0 (expired entries are never stored)
};

// One skill whose reuse timer is still running.
struct CooldownView
{
	uint32_t skillId;
	uint32_t remainingMs;    // > 0
};
```

**(b) `SelfState`'e alan ekle** (mevcut alanların **sonuna**, `bool sitting;`'ten sonra; mevcut alanların sırası ve adları değişmez):

```cpp
	uint32_t     hpPotStock;        // pots in the own bag that BeginPotion would accept: HP kind
	uint32_t     mpPotStock;        // same, MP kind
	uint32_t     potWaitMs;         // shared pot timer: ms until the next pot may go out; 0 = now
	uint32_t     castGapWaitMs;     // gap after the last EFFECTING (kCastGapMs): ms until the next cast; 0 = now
	BuffView     buffs[kSnapMaxBuffs];
	int          buffCount;         // entries stored (<= kSnapMaxBuffs)
	int          buffTotal;         // buffs/debuffs with remainingSec > 0 offered to SelfAddBuff
	CooldownView cooldowns[kSnapMaxCooldowns];
	int          cooldownCount;
	int          cooldownTotal;
```

Üstüne kısa bir yorum: `// own state only: the contract (docs/14 5.2) forbids the same fields for OTHER players`. `BuildSnapshot` **değişmez** (`out.self = self` hepsini kopyalar; test bunu doğrular).

**(c) Yardımcılar** (`BuildSnapshot`'tan sonra, namespace kapanışından önce; başlık yorumu `// --- self state extras (ADR-0017 Ek F4-17) ---`):

```cpp
// Whole seconds left until 'endSec' (a time_t value); 0 when it is not in the future. Clamped to 0xFFFFFFFF.
inline uint32_t SnapRemainingSec(int64_t endSec, int64_t nowSec);

// Milliseconds left of a 'spanMs' timer that started 'sinceMs' ago; 0 when it has run out.
inline uint32_t SnapRemainingMs(uint32_t spanMs, uint64_t sinceMs);

// Appends a buff. remainingSec == 0 -> nothing happens (returns false, not counted). Otherwise buffTotal++ and the
// entry is stored while buffCount < kSnapMaxBuffs (returns true when stored).
inline bool SelfAddBuff(SelfState & s, uint32_t skillId, uint8_t buffType, bool isBuff, uint32_t remainingSec);

// Same rules for cooldowns (remainingMs == 0 -> ignored).
inline bool SelfAddCooldown(SelfState & s, uint32_t skillId, uint32_t remainingMs);
```

**(d) Dört yeni `TEST_CASE`** (`PerceptionTests.cpp` dosya sonuna; `MakeSelf()` zaten var, onu kullan):

1. `Perception_Self_Remaining`: `SnapRemainingSec(100, 40) == 60`; `(40, 100) == 0`; `(100, 100) == 0`; `(4000000000000, 0) == 0xFFFFFFFF` (kelepçe); `SnapRemainingMs(2500, 1000) == 1500`; `(2500, 2500) == 0`; `(2500, 9999999999ULL) == 0`; `(0, 0) == 0`.
2. `Perception_Self_AddBuff`: sıfırlanmış `MakeSelf()`'e üç buff ekle → `buffCount == 3`, `buffTotal == 3`, alanlar sırasıyla korunur (`skillId`, `buffType`, `isBuff`, `remainingSec`); `remainingSec == 0` ekleme `false` döner ve sayaçlar değişmez; 16'ya tamamla, 17. ve 18. ekleme `false` döner → `buffCount == 16`, `buffTotal == 18`; ilk 16 kayıt değişmemiş.
3. `Perception_Self_AddCooldown`: aynı kurallar `cooldowns` için (`remainingMs == 0` yok sayılır; kapasite 16; `cooldownTotal` taşanı da sayar).
4. `Perception_Snapshot_SelfExtras`: `MakeSelf()`; `hpPotStock = 12`, `mpPotStock = 7`, `potWaitMs = 1500`, `castGapWaitMs = 90`, bir buff ve iki cooldown ekle; boş `ObsTable`/`NpcTable` ile `BuildSnapshot` → `snap.self` bu alanların hepsini aynen taşır; `enemyCount == allyCount == npcCount == 0`.

Toplam test sayısı **67 → 71**.

### 5.3 `GameServer/Bot/ActionExecutor.{h,cpp}`

**(a)** `ActionExecutor.cpp`'de `BeginPotion`'ın üstüne (pot bölümünde, `SubmitPotion`'dan sonra) dosya-statik:

```cpp
// The server-side shape of a pot the bot can drink (ADR-0017 Ek F4-04). Shared by BeginPotion and PotKindOf.
static bool PotMagicSupported(const _MAGIC_TABLE * m, const _MAGIC_TYPE3 * t3, uint32 itemId);
```

Gövde, `BeginPotion`'daki mevcut `supported` ifadesinin (`:1135-1149`) **aynısı**dır (`m` null değil varsayımıyla; `t3 != nullptr` kontrolü fonksiyonun içinde kalır). `BeginPotion`'da `bool supported = …` yerine `bool supported = PotMagicSupported(m, t3, itemId);`. Başka hiçbir satır değişmez (neden metinleri, sıra, `m == nullptr` → `bad_item` dalı aynı).

**(b)** `ActionExecutor.h`'ye (`BeginPotion` bildiriminin hemen üstüne, kısa İngilizce yorumla):

```cpp
	// Classifies one bag item for the perception snapshot: 1 = HP pot, 2 = MP pot, 0 = this bot cannot drink it
	// (unknown item, no Effect1, class/level mismatch, unknown skill, or not a supported pot shape). Applies the same
	// rules as BeginPotion (keep them in sync). Touches nothing; the stock is NOT checked.
	static uint8 PotKindOf(CUser * user, uint32 itemId);
```

`ActionExecutor.cpp`'de tanımı `BeginPotion`'dan sonra: `user == nullptr` → 0; `g_pMain->GetItemPtr(itemId)` null veya `m_iEffect1 == 0` → 0; sınıf/seviye kontrolü (`BeginPotion`'dakinin aynısı: `m_bClass != 0 && !user->JobGroupCheck(...)`, `m_bReqLevel != 0 && user->GetLevel() < ...`) → 0; `m_MagictableArray.GetData(it->m_iEffect1)` null → 0; `PotMagicSupported(...)` değilse 0; aksi halde `t3->bDirectType` (1 veya 2).

### 5.4 `GameServer/Bot/BotManager.cpp`

**(a) `FillSelfExtras`** — `CommandSnap`'in hemen üstüne, dosya-statik:

```cpp
// Fills the own-state extras of the snapshot (docs/14 5.1/5.2: the bot's own bag, buffs and timers are allowed).
// IOCP thread only. Reads nothing that belongs to another player.
static void FillSelfExtras(BotSession * s, CUser * me, std::chrono::steady_clock::time_point now,
	BotCore::SelfState & self);
```

Sırayla:

1. **Stok:** `for (uint8 i = INVENTORY_INVENT; i < INVENTORY_INVENT + HAVE_MAX; i++)` (`CountInBag` ile aynı aralık: yalnızca çanta, ekipman/cospre/sihir çantası değil); `_ITEM_DATA * item = me->GetItem(i)`; `item->nNum == 0 || item->sCount == 0` atla; `uint8 kind = ActionExecutor::PotKindOf(me, item->nNum)`; `kind == 1` → `hpPotStock += item->sCount`; `kind == 2` → `mpPotStock += item->sCount`.
2. **Pot süresi:** `BotCore::PotionCheck c; memset(&c, 0, sizeof(c)); c.hasLast = s->m_potHasLast; if (c.hasLast) c.sinceLastMs = (uint32)duration_cast<milliseconds>(now - s->m_potLast).count();` → `self.potWaitMs = BotCore::PotionWaitMs(c)` (`ActionExecutor.cpp:1202-1210`'daki hesabın aynısı).
3. **Cast boşluğu:** `s->m_castAnyHas` ise `since = duration_cast<milliseconds>(now - s->m_castAnyLast).count()` → `self.castGapWaitMs = BotCore::SnapRemainingMs(BotCore::kCastGapMs, since)`.
4. **Skill soğumaları:** `for (auto & kv : s->m_castSkillLast)`: `_MAGIC_TABLE * m = g_pMain->m_MagictableArray.GetData(kv.first)`; `m == nullptr` ise atla; `since` = `now - kv.second` (ms); `BotCore::SelfAddCooldown(self, kv.first, BotCore::SnapRemainingMs(BotCore::CastRecastMs(m->sReCastTime), since))`. (`std::map` kimliğe göre artan: sıra belirlenimli.)
5. **Buff'lar:** `std::lock_guard<std::recursive_mutex> lock(me->m_buffLock);` yalnızca `for (auto & kv : me->m_buffMap)` döngüsünü kapsayan blokta; `BotCore::SelfAddBuff(self, kv.second.m_nSkillID, kv.second.m_bBuffType, kv.second.m_bIsBuff, BotCore::SnapRemainingSec((int64_t)kv.second.m_tEndTime, (int64_t)UNIXTIME))`. Kilit tutulurken günlük yazma, başka kilit alma, `ActionExecutor` çağrısı **yok**. (`m_buffLock` adı fonksiyonda **bir kez** geçer.)

**(b) `CommandSnap`'te:** `self.sitting = …` satırından sonra `FillSelfExtras(s, me, std::chrono::steady_clock::now(), self);` ekle. Yazdırma: mevcut `self` satırından sonra, `enemies/allies/npcs` toplam satırından **önce** üç tür satır ekle (mevcut satırların metni değişmez):

```
BotManager: cmd snap:   stock hp_pot=%u mp_pot=%u wait pot=%ums cast_gap=%ums
BotManager: cmd snap:   buffs %d (total %d), cooldowns %d (total %d)
BotManager: cmd snap:   buff skill=%u type=%u %s remain=%us          (en çok 10, "buff" / "debuff")
BotManager: cmd snap:   cooldown skill=%u remain=%ums                 (en çok 10)
```

Satır sırası: stock satırı, `buffs … cooldowns` satırı, ardından buff satırları ve cooldown satırları; sonra mevcut `enemies … npcs` toplam satırı ve listeler. Tampon boyutu mevcut `char message[320]`; `snprintf` kullan.

### 5.5 Derleme ve test

`./tools/build.sh Release`, `./tools/build.sh Debug`, `./tools/run-tests.sh Release`, `./tools/run-tests.sh Debug`. Değişen dosyaları `touch` edip yeniden derleyerek uyarı çıktısının boş olduğunu doğrula. Commit mesajı: `[F4-17] SelfState: pot stoku, soğuma ve buff listesi uygulandı`. Uygulayıcı Raporu'nu doldur, `Durum` satırını `UYGULANDI` yap.

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter; `Perception.h`, `PerceptionTests.cpp`, `ActionExecutor.h`, `ActionExecutor.cpp`, `BotManager.cpp` için uyarı çıktısı boş (`touch` ile yeniden derlenip bakılır; `UNIXTIME` ya da `INVENTORY_INVENT`/`HAVE_MAX` görünmezse **durup** raporda sor, yeni include eklemeden önce).
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı dört yeni test adını (`Perception_Self_Remaining`, `Perception_Self_AddBuff`, `Perception_Self_AddCooldown`, `Perception_Snapshot_SelfExtras`) içerir, toplam test sayısı **71**; `Debug` aynı; önceki 67 testin tamamı değişmeden geçer.
- [ ] K4: `BotCore/Perception.h` hâlâ sunucusuz: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/Perception.h` boş; `grep -n "^#include" BotCore/Perception.h` yalnızca `<cstddef>`, `<cstdint>`, `<cstring>`, `<cmath>`; `grep -n "std::min\|std::max\|new \|malloc\|std::vector\|std::string" BotCore/Perception.h` boş.
- [ ] K5: **sözleşme denetimi (AC-LRN-03) bozulmadı:** `sed -n '/struct UnitView/,/^\t};/p' BotCore/Perception.h | grep -inE "\b(max)?(hp|mp)\b|name|cooldown|stock|invent"` boş; aynı komut `struct NpcView` için boş; `sed -n '/void BotManager::CommandSnap/,/^}/p' GameServer/Bot/BotManager.cpp | grep -E "GetUserPtr|GetNpcPtr|m_arNpcArray|m_RegionUserArray|m_RegionNpcArray|GetRegion\(|GetMap\(|g_pMain|CNpc|m_sItemArray|GetItem\(|GetPartyID|m_buffMap|m_CoolDownList"` boş (erişim `FillSelfExtras`'ta); `sed -n '/static void FillSelfExtras/,/^}/p' GameServer/Bot/BotManager.cpp | grep -E "GetUserPtr|GetNpcPtr|m_arNpcArray|m_RegionUserArray|m_RegionNpcArray|GetRegion\(|GetMap\(|CNpc|GetPartyID|m_CoolDownList|m_MagicTypeCooldownList"` boş; `FillSelfExtras` içinde `GetItem(`, `m_buffMap`, `m_buffLock` yalnızca `me->` ile geçer (`grep -n "GetItem(\|m_buffMap\|m_buffLock"` çıktısının her satırı `me->` içerir).
- [ ] K6: kilit disiplini: `sed -n '/static void FillSelfExtras/,/^}/p' GameServer/Bot/BotManager.cpp | grep -c m_buffLock` = 1; kilit yalnızca `m_buffMap` döngüsünü kapsayan blokta (`WriteBotLog`, `snprintf`, `ActionExecutor::` çağrısı o bloğun içinde yok); `CommandSnap`'te `m_obsLock` sayısı hâlâ 1; `grep -c "std::mutex" GameServer/Bot/BotSession.h` = 1 (değişmedi).
- [ ] K7: `BeginPotion` davranışı aynı: `sed -n '/PotionOutcome ActionExecutor::BeginPotion/,/^}/p' GameServer/Bot/ActionExecutor.cpp | grep -c '"bad_item"'` = 4 ve `"unsupported_item"` = 1 (değişmedi); `grep -c "PotMagicSupported" GameServer/Bot/ActionExecutor.cpp` = 3 (tanım + `BeginPotion` + `PotKindOf`); `git diff gece/2026-10-02...bot/F4-17 -- GameServer/Bot/ActionExecutor.cpp | grep '^-' | grep -v '^---'` yalnızca `supported` ifadesinin eski satırlarıdır (en çok 15 satır, hepsi `&&` zincirine ait).
- [ ] K8: mevcut kod yalnızca ekleme (yukarıdaki `ActionExecutor.cpp` istisnası dışında): `git diff gece/2026-10-02...bot/F4-17 -- BotCore/Perception.h Tests/BotCoreTests/PerceptionTests.cpp GameServer/Bot/ActionExecutor.h | grep '^-' | grep -v '^---'` **boş**; `BotManager.cpp` farkında `-` satırı **yok**.
- [ ] K9: `SelfState` mevcut alanları değişmedi: `git diff` içinde `sid`, `nation`, `cls`, `level`, `x, z`, `hp, maxHp, mp, maxMp`, `dead`, `sitting` satırlarında `-` yok (K8 ile aynı); `BuildSnapshot` gövdesi değişmedi.
- [ ] K10: `ENABLED=0` ile davranış değişmez: yeni kod yalnızca `/bot snap` komutuyla çalışır; `Startup()`/`Tick()`/`TickSessions()`/`BuildStatusLines()`/ini okuma değişmedi; yeni ini anahtarı yok; `GameServer/` içinde `Bot/` dışında dosya değişmemiş.
- [ ] K11: `git diff --stat gece/2026-10-02...bot/F4-17` yalnızca §4'teki 5 dosyayı (ve plan dosyasını) gösterir; `*.vcxproj*`, `BotManager.h`, `BotSession.*`, `Telemetry.*`, `ScenarioRunner.*` farkta yok.
- [ ] K12: satır sonu/BOM bozulmadı (`file` çıktısı değişiklik öncesiyle aynı: ASCII + CRLF; `ActionExecutor.*` ve `BotManager.cpp` için de önceki durum korunur); `git diff --check` boş.
- [ ] K13: `GameServer/Bot/` içinde yeni `printf` (yalnızca `snprintf`), `Sleep`, `CreateThread`, `rand(` yok.
- [ ] K14: F4-01..F4-16 gerilemesiz: `grep -c "CheckMoveStep" GameServer/Bot/ActionExecutor.cpp` ≥ 2; `CheckAttack`, `CheckCastStart`, `CheckPotion`, `CheckStance`, `CheckTargetHp`, `CheckRegene`, `CheckPartyInvite`, `CheckPartyAccept`, `CheckPartyDecline`, `CheckPartyLeave`, `CheckPartyManage`, `CheckChat`, `CheckUserIn`, `CheckNpcIn` her biri ≥ 1; `BuildSnapshot` hâlâ `CommandSnap`'ten çağrılır.
- [ ] K15 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları 1–5 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-17
git diff gece/2026-10-02...bot/F4-17 -- BotCore/Perception.h Tests/BotCoreTests/PerceptionTests.cpp GameServer/Bot/ActionExecutor.h GameServer/Bot/BotManager.cpp | grep '^-' | grep -v '^---'
git diff gece/2026-10-02...bot/F4-17 -- GameServer/Bot/ActionExecutor.cpp | grep '^-' | grep -v '^---'
git diff gece/2026-10-02...bot/F4-17 -- GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters BotCore/BotCore.vcxproj Tests/BotCoreTests/BotCoreTests.vcxproj GameServer/Bot/BotManager.h GameServer/Bot/BotSession.h GameServer/Bot/BotSession.cpp
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/Perception.h
grep -n "^#include" BotCore/Perception.h
grep -n "std::min\|std::max\|new \|malloc\|std::vector\|std::string" BotCore/Perception.h
sed -n '/struct UnitView/,/^\t};/p' BotCore/Perception.h | grep -inE "\b(max)?(hp|mp)\b|name|cooldown|stock|invent"
sed -n '/struct NpcView/,/^\t};/p' BotCore/Perception.h | grep -inE "\b(max)?(hp|mp)\b|name|cooldown|stock|invent"
sed -n '/void BotManager::CommandSnap/,/^}/p' GameServer/Bot/BotManager.cpp | grep -E "GetUserPtr|GetNpcPtr|m_arNpcArray|m_RegionUserArray|m_RegionNpcArray|GetRegion\(|GetMap\(|g_pMain|CNpc|m_sItemArray|GetItem\(|GetPartyID|m_buffMap|m_CoolDownList"
sed -n '/static void FillSelfExtras/,/^}/p' GameServer/Bot/BotManager.cpp | grep -E "GetUserPtr|GetNpcPtr|m_arNpcArray|m_RegionUserArray|m_RegionNpcArray|GetRegion\(|GetMap\(|CNpc|GetPartyID|m_CoolDownList|m_MagicTypeCooldownList"
sed -n '/static void FillSelfExtras/,/^}/p' GameServer/Bot/BotManager.cpp | grep -n "GetItem(\|m_buffMap\|m_buffLock"
sed -n '/static void FillSelfExtras/,/^}/p' GameServer/Bot/BotManager.cpp | grep -c m_buffLock
sed -n '/void BotManager::CommandSnap/,/^}/p' GameServer/Bot/BotManager.cpp | grep -c m_obsLock
sed -n '/PotionOutcome ActionExecutor::BeginPotion/,/^}/p' GameServer/Bot/ActionExecutor.cpp | grep -c '"bad_item"'
grep -c "PotMagicSupported" GameServer/Bot/ActionExecutor.cpp
grep -c "std::mutex" GameServer/Bot/BotSession.h
grep -n "printf\|Sleep\|CreateThread\|rand(" GameServer/Bot/BotManager.cpp | grep -v snprintf
file BotCore/Perception.h Tests/BotCoreTests/PerceptionTests.cpp GameServer/Bot/BotManager.cpp GameServer/Bot/ActionExecutor.cpp GameServer/Bot/ActionExecutor.h
git diff --check gece/2026-10-02...bot/F4-17
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=summary`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. **AIServer de açık olmalıdır** (`status` ile üç `[UP]`). Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`). Botlar: Karus `BotWP_K`, `BotMF_K`; El Morad `BotWP_E`, `BotPHD_E`; zone 71; spawn'lar arasında ~14 sn bırak. Gözlem `Logs/Bot_*.log` ve `Logs/bots/*.jsonl`. Envanter tablolarına (USERDATA vb.) **bakılmaz**; stok karşılaştırması snap farkı ve telemetrideki `stock`/`stock_after` ile yapılır. Senaryolar:

1. **Başlangıç görüntüsü:** `spawn BotWP_K,BotWP_E` → `snap BotWP_K`: `stock hp_pot=… mp_pot=…` satırı var (taze spawn: `wait pot=0ms cast_gap=0ms`, `buffs 0 … cooldowns 0 …` veya spawn'dan kalan buff'lar), F4-16 satırları (`self …`, `enemies …`) **aynen** duruyor.
2. **Pot stoku ve ortak pot süresi:** `pot BotWP_K 389015000 3` sonra, pot bittikten sonra (≥ 8 sn) `snap BotWP_K`: `hp_pot` öncekinden **3 az** (tüketilen HP potu; `ACTION_SUBMIT` `stock`/`stock_after` ile uyumlu); ardından `pot BotWP_K 389015000 1` ve gönderiminden sonraki 2 sn içinde `snap`: `wait pot` > 0 ve ≤ 2500. MB-01 potu (`389014000`) içilince ilgili stok **azalmaz** (K-5). `pot BotWP_K 389020000 1` sonrası `mp_pot` farkı beklenen yönde.
3. **Skill soğuması:** `cast BotMF_K 110518 BotWP_E 1` (F4-03 mutlu yolu) bittikten hemen sonra `snap BotMF_K`: `cooldown skill=110518 remain=…ms` satırı var ve `remain` ≤ `MAGIC.ReCastTime * 100` (Claude değeri `SELECT ReCastTime FROM MAGIC WHERE MagicNum = 110518` ile okur); `cast_gap` ≤ 140. Soğuma süresi dolduktan sonra `snap`: cooldown satırı yok (`cooldowns 0`).
4. **Buff:** Claude, bota kendi sınıfından kendine atılabilir bir Type4 skill bulur (`MAGIC`/`MAGIC_TYPE4` tablolarından `SELECT`; kişisel veri tabloları değil), `cast <bot> <skill> self` → `snap` içinde `buff skill=<id> type=… remain=…s` satırı, `remain` ≤ `MAGIC_TYPE4.Duration`; süre bitince satır kaybolur. Uygun skill bulunamazsa veya sunucu skill'i reddederse bu senaryo "gözlenmedi" yazılır ve birim testleri (`Perception_Self_AddBuff`) + kod okuması kanıt olur; kriteri düşürmez.
5. **Gerilemesiz:** F4-01..F4-16 komutları çalışır (`move`/`attack`/`cast`/`pot`/`sit`/`target`/`regene`/`pinvite`/`paccept`/`see`/`npcs`/`snap`); pot reddi hâlâ aynı: `pot BotWP_K 999999` → `refused (bad_item)`, desteklenmeyen pot → `refused (unsupported_item)`; `snap` bilinmeyen/çıkmış bot ve bot adsız çağrıda yalnızca kullanım/hata satırı yazar; `despawn all` temiz; `tick_p95_us` ≤ 500; `ENABLED=0` → komut dosyası tüketilmez/log yok. İnsan istemcisi gerekmez.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §Kod kuralları. `Perception.h`, `PerceptionTests.cpp`, `ActionExecutor.*`, `BotManager.cpp` ASCII + CRLF; yeni dosya yok.
- **Thread kuralı (`docs/13` §3):** `CommandSnap`/`FillSelfExtras` IOCP thread'inde çalışır (komut kanalı, ADR-0015); `BotSession`'ın "IOCP thread only" alanlarını (`m_castSkillLast`, `m_castAny*`, `m_pot*`) kilitsiz okuyabilir. `m_buffMap` başka thread'lerce de değiştirilebilir (`MagicInstance`), bu yüzden **yalnızca** `m_buffLock` altında ve kısa blokta okunur. `BuildSnapshot` ve `WriteBotLog` kilit tutulurken çağrılmaz.
- **Bağımsız pot kuralı yok:** `PotKindOf` kendi başına bir kural **icat etmez**; `BeginPotion` ile aynı ifadeyi (`PotMagicSupported`) paylaşır. İkisinin sınıf/seviye/skill kontrolleri aynı kalmalı; birini değiştirirsen diğerini de değiştir ve raporda yaz.
- **`SelfState` büyüdü** (~320 bayt ek). `PerceptionSnapshot` yerel değişkendir; `static`/yığın dışı yapma, kapasite sabitlerini artırma.
- **Zaman kaynakları:** buff bitişi `UNIXTIME` (saniye çözünürlük, sunucunun kendi saati), soğumalar `steady_clock` (ms). İkisini karıştırma; birbirine çevirme.
- **Bilinçli eksik:** tip kapısı (MEC-MAG-03) süresi, `potWaitMs`/`castGapWaitMs` dışındaki soğumalar ve pot dışı stok görüntüde yok; karar katmanı gelince ihtiyaç netleşir.
- **Sözleşme notu `[Ö]`:** botun kendi debuff'larının istemcide gösterilip gösterilmediği doğrulanmadı (ikon davranışı); bot bunları sunucu durumundan okuyor, insanın gördüğüyle aynı olduğu varsayımı muhafazakârdır (insan görür). İnsan ölçümü gerekirse ayrı kayıt açılır, bu planın işi değildir.
- Beklenmedik bir şey görürsen **uydurma**: Uygulayıcı Raporu'na yaz.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-17` (taban: `gece/2026-10-02`), kod commit'i `05d7c64` `[F4-17] SelfState: pot stoku, soğuma ve buff listesi uygulandı`; bu rapor + `Durum` commit'i arkadan.
- Değişen dosyalar ve neden:
  - `BotCore/Perception.h`: `kSnapMaxBuffs`/`kSnapMaxCooldowns`; `BuffView`/`CooldownView`; `SelfState`'e `hpPotStock`, `mpPotStock`, `potWaitMs`, `castGapWaitMs`, `buffs[]`, `buffCount`, `buffTotal`, `cooldowns[]`, `cooldownCount`, `cooldownTotal`; dosya sonuna `SnapRemainingSec`, `SnapRemainingMs`, `SelfAddBuff`, `SelfAddCooldown` (hepsi saf, sunucusuz). `BuildSnapshot` değişmedi (`out.self = self` yeni alanları kopyalar).
  - `Tests/BotCoreTests/PerceptionTests.cpp`: dört yeni `TEST_CASE` (`Perception_Self_Remaining`, `Perception_Self_AddBuff`, `Perception_Self_AddCooldown`, `Perception_Snapshot_SelfExtras`). 67 → 71.
  - `GameServer/Bot/ActionExecutor.h`: `static uint8 PotKindOf(CUser *, uint32)` bildirimi.
  - `GameServer/Bot/ActionExecutor.cpp`: `BeginPotion`'ın `supported` ifadesi dosya-statik `PotMagicSupported(const _MAGIC_TABLE *, const _MAGIC_TYPE3 *, uint32)`'a taşındı (ifade birebir aynı); `PotKindOf` tanımı aynı sınıf/seviye/skill kapılarını kullanır, `PotMagicSupported` değilse 0, aksi halde `t3->bDirectType`.
  - `GameServer/Bot/BotManager.cpp`: dosya-statik `FillSelfExtras` (yalnızca botun kendi çantası `INVENTORY_INVENT..+HAVE_MAX`, kendi `m_pot*`/`m_cast*` zamanlayıcıları, kendi `m_buffMap`'i `m_buffLock` altında); `CommandSnap` çağırır ve bir özet + her listenin ≤ 10 kaydını yazar. Yeni include/ini/vcxproj satırı yok.
- Derleme sonucu:
  - `./tools/build.sh Release` rc=0 (beş dosya `touch` ile yeniden derlendi; bu dosyalarda uyarı **yok**; kalan uyarılar eski `GameServerDlg.cpp` satırları).
  - `./tools/build.sh Debug` rc=0 (aynı; bu dosyalarda uyarı yok).
  - `./tools/run-tests.sh Release` → `71 tests, 0 failed` (dört yeni test adı çıktıda).
  - `./tools/run-tests.sh Debug` → `71 tests, 0 failed`.
- Kabul kriterleri öz-değerlendirme: K1–K14 statik/derleme düzeyinde öz-değerlendirildi, hepsi ✔:
  - K1–K2: iki derleme rc=0, beş dosyada uyarı yok (`touch` edilip bakıldı; `UNIXTIME` `shared/stdafx.h`→`TimeThread.h`, `INVENTORY_INVENT`/`HAVE_MAX` ve `_BUFF_TYPE4_INFO`/`Type4BuffMap` zaten `stdafx.h`/`Unit.h` üzerinden görünür; yeni include gerekmedi).
  - K3: Release+Debug `71 tests, 0 failed`, dört yeni test adı çıktıda, önceki 67 test korundu.
  - K4: `Perception.h` sunucusuz; include'lar yalnızca `<cstddef>`, `<cstdint>`, `<cstring>`, `<cmath>`; yasaklı `std::min/max/new/malloc/vector/string` grep'i boş.
  - K5: `UnitView`/`NpcView` sözleşme grep'leri boş; `CommandSnap` gövdesinde yasak okuma yok (hepsi `FillSelfExtras`'ta); `FillSelfExtras`'ta `GetItem(`/`m_buffMap`/`m_buffLock` yalnızca `me->` ile.
  - K6: `FillSelfExtras`'ta `m_buffLock` sayısı 1; kilit yalnızca `m_buffMap` döngüsünü kapsıyor (içinde günlük/`snprintf`/`ActionExecutor::` yok); `CommandSnap`'te `m_obsLock` 1; `BotSession.h` `std::mutex` 1.
  - K7: `BeginPotion`'da `"bad_item"` 4, `"unsupported_item"` 1; `PotMagicSupported` sayısı 3; ActionExecutor.cpp'deki `-` satırları yalnızca eski `supported` zinciri (15 satır).
  - K8–K9: diğer dört dosyada `-` satırı yok (yalnızca ekleme), `BotManager.cpp`'de `-` yok; `BuildSnapshot` gövdesi ve `SelfState`'in eski alanları değişmedi.
  - K10: yeni kod yalnızca `/bot snap` ile çalışır; yeni ini anahtarı yok; `GameServer/` içinde `Bot/` dışı değişmedi; `Startup/Tick/TickSessions/BuildStatusLines` değişmedi.
  - K11: `git diff --stat gece/2026-10-02...bot/F4-17` yalnızca §4'teki 5 dosya + plan; vcxproj/BotManager.h/BotSession/Telemetry/ScenarioRunner farkta yok.
  - K12: `file` çıktısı beş dosyada ASCII + CRLF (değişmeden); `git diff --check` boş.
  - K13: yeni `printf`/`Sleep`/`CreateThread`/`rand(` eklenmedi (mevcut `printf`/`fprintf` satırları önceden vardı).
  - K14: guard fonksiyonlarının tümü ≥ 1 (`CheckMoveStep`=2), `BuildSnapshot` hâlâ `CommandSnap`'ten çağrılıyor.
  - K15 çalışma zamanı doğrulaması Claude'da (`/plan-dogrula`), DeepSeek yapmaz.
- Plandan sapmalar ve gerekçeleri:
  - Yazdırma bloğunda `kPrintMax` (10) fonksiyonun sonraki satırında tanımlı olduğundan ve `BotManager.cpp` farkında `-` satırı yasak olduğundan, mevcut `kPrintMax` taşınmadı; eklenen blokta ayrı bir `const int kPrintMaxSelf = 10;` kullanıldı (plan "en çok 10" diyor, değer aynı). Plan metni tek bir sabit adı dayatmıyor.
- Açık sorular: yok.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-02

**Karar:** DOĞRULANDI (gece modu; birleştirmeyi döngü betiği yapar, push yok). İncelenen commit: `f90b61b` (`bot/F4-17`; kod `05d7c64`, taban `gece/2026-10-02`). Çalışma ağacı temiz.

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `build.sh Release` rc=0; beş dosya `touch` edilip yeniden derlendi (`ActionExecutor.cpp`, `BotManager.cpp`, `PerceptionTests.cpp` derleme çıktısında); günlükte `warning` satırı 0 |
| K2 | ✔ | `build.sh Debug` rc=0; bu dosyalarda uyarı/hata satırı yok |
| K3 | ✔ | `run-tests.sh Release` ve `Debug`: `71 tests, 0 failed`; dört yeni ad `[ OK ]`. Test gövdeleri okundu: plandaki değerler (60/0/0/0xFFFFFFFF kelepçe, 1500/0/0/0, 3 buff + 0'lı ekleme + 16 kapasite + 18 toplam, aynısı cooldown, `BuildSnapshot` ile alan taşıma) birebir sınanıyor |
| K4 | ✔ | `windows.h\|stdafx\|GameServer\|shared/` grep'i boş; `#include` yalnızca `<cstddef>`, `<cstdint>`, `<cstring>`, `<cmath>`; `std::min/max/new/malloc/vector/string` grep'i boş |
| K5 | ✔ | `UnitView`/`NpcView` grep'leri boş; `CommandSnap` yasak-erişim grep'i boş; `FillSelfExtras` yasak-erişim grep'i boş; `GetItem(`/`m_buffMap`/`m_buffLock` satırlarının hepsi `me->` ile (`BotManager.cpp` `FillSelfExtras` 6, 41, 42. satırlar göreli) |
| K6 | ✔ | `FillSelfExtras`'ta `m_buffLock` 1; kilit yalnızca `m_buffMap` döngüsünü kapsayan blokta (içinde `WriteBotLog`/`snprintf`/`ActionExecutor::` yok); `CommandSnap`'te `m_obsLock` 1; `BotSession.h` `std::mutex` 1 |
| K7 | ✔ | `BeginPotion`'da `"bad_item"` 4, `"unsupported_item"` 1; `PotMagicSupported` 3; `ActionExecutor.cpp` farkında `-` satırı 15 (hepsi eski `supported` `&&` zinciri); `PotKindOf` sınıf/seviye/`m == nullptr` kapıları `BeginPotion` ile aynı, `t3` null olamaz (`PotMagicSupported` `t3 != nullptr` ister) |
| K8 | ✔ | `Perception.h`, `PerceptionTests.cpp`, `ActionExecutor.h`, `BotManager.cpp` farkında `-` satırı yok |
| K9 | ✔ | `SelfState` mevcut alanları ve sırası değişmedi (yeni alanlar sona eklendi); `BuildSnapshot` gövdesi farkta yok |
| K10 | ✔ | yeni kod yalnızca `snap` yolunda (`CommandSnap` → `FillSelfExtras`); `Startup/Tick/TickSessions/BuildStatusLines` farkta yok; yeni ini anahtarı yok; `GameServer/` içinde yalnızca `Bot/`. `ENABLED=0` çalışma zamanında bu turda denenmedi (statik denetim; komut kanalı kapalıyken tüketilmez, önceki turlarda çalışma zamanında doğrulanmış kalıp) |
| K11 | ✔ | `--stat`: 5 kod dosyası + plan; `*.vcxproj*`, `BotManager.h`, `BotSession.*`, `Telemetry.*`, `ScenarioRunner.*` farkı 0 satır |
| K12 | ✔ | beş dosya `ASCII text, with CRLF line terminators`; `git diff --check` boş |
| K13 | ✔ | `GameServer/Bot` farkının eklenen satırlarında `printf/Sleep/CreateThread/rand(` yok (yalnızca `snprintf`) |
| K14 | ✔ | `CheckMoveStep` 2; diğer 14 `Check*` her biri 1; `BuildSnapshot` `BotManager.cpp:2530`'da `CommandSnap`'ten çağrılıyor |
| K15 | ✔ (S4 gözlenmedi) | çalışma zamanı, aşağıda |

**Çalışma zamanı (K15; `Release`, `ENABLED=1`, `MAX_BOTS=16`, `TELEMETRY=decisions` (ini değiştirilmedi; plan `summary` demişti, `decisions` yalnızca daha çok jsonl satırı üretir), `SPAWN_ON_START` boş, zone 71; üç sunucu `[UP]`, `AI=bağlı`; `GameServer.ini` md5 öncesi/sonrası aynı `265a8e1c...`; iş bitince `run-servers.sh stop`, `BotCommands.txt` kalmadı; `Logs/bots/` eski dosyaları silinmedi (silme izni verilmedi), gözlem satır ofsetinden ve yeni `live-173728.jsonl`'den yapıldı):**

1. **Başlangıç görüntüsü:** `spawn BotWP_K,BotWP_E` → `snap BotWP_K`: `self sid=2984 ... hp=5650/5650 mp=5370/5370 alive standing`, ardından `stock hp_pot=81 mp_pot=1 wait pot=0ms cast_gap=0ms`, `buffs 0 (total 0), cooldowns 0 (total 0)`, sonra F4-16 satırı `enemies 0 (total 0), allies 0 (total 0), npcs 25 (total 25)` ve `npc id=...` satırları aynen. Satır sırası plandaki gibi.
2. **Pot stoku ve ortak pot süresi:** `pot BotWP_K 389015000 3` sonrası `hp_pot` 81 → **78** (3 az). Telemetri `ACTION_SUBMIT stock:78` / `ACTION_RESULT stock_after:77` (389015000 yığını); snap `hp_pot` = o yığın + 1: `BotWP_K`'de HP türü ikinci bir pot (MB-01 `389014000`, `stock:1`) var ve `BeginPotion`'ın kabul ettiği tür olduğu için sayılıyor (81 = 80 + 1, 78 = 77 + 1). `pot ... 389015000 1` sonrası 1,6 sn'de snap: `hp_pot=77`, `wait pot=1408ms` (> 0 ve ≤ 2500). `pot BotWP_K 389014000 1` → `effected` ama telemetri `stock:1`, `stock_after:1` (K-5: MB-01 potu tüketilmiyor) ve snap `hp_pot=77` **azalmadı** ✔. `pot BotWP_K 389020000 1` → `effected`, `stock:1`, `stock_after:1` (bu pot da tüketilmiyor), snap `mp_pot=1` aynı; telemetri ile snap tutarlı. Plan "mp_pot farkı beklenen yönde" diyordu: bu pot için beklenen yön "değişmez"dir (telemetri aynı).
3. **Skill soğuması:** plandaki `110518`'in `ReCastTime` değeri 1 (= 100 ms) olduğundan snap ile yakalanamaz; gözlenebilir olması için `110539` (mage, Type3, `Moral 7`, `Range 56`, `CastTime 15`, `ReCastTime 43` = 4300 ms) kullanıldı. `BotMF_K` `BotWP_E`'ye 30,6 m'ye yürütüldü; `cast BotMF_K 110539 BotWP_E 1` → `cast finished (effected) after 1 cycle(s), 1 ok, 2 packet(s) sent`. Peş peşe üç snap: `cooldowns 1 (total 1)` + `cooldown skill=110539 remain=2650ms`; 2,2 sn sonra `remain=458ms` (2650 − 2192 = 458 ✔, `t` farkı 2192 ms); sonraki snap `cooldowns 0 (total 0)`; ≥ 6 sn sonra yine `cooldowns 0`. `remain` ≤ 4300 ✔; `cast_gap=0ms` (≤ 140; 140 ms penceresi komut yoklama aralığından kısa olduğu için yakalanamadı, değer sınır içinde).
4. **Buff: gözlenmedi.** Bot cast komutu yalnızca Type1/Type3 skill kabul eder (`ActionExecutor.cpp:723`); Type4 buff'ı bota atacak bot yolu yok (`BotWP_K`/`BotMF_K` spawn'da `buffs 0`). Plan bu durumda "gözlenmedi" yazılmasına ve birim test + kod okumasına izin veriyor: `Perception_Self_AddBuff` (3 kayıt, sıfır saniye yok sayma, 16 kapasite, `buffTotal` taşan), `BotManager.cpp` `FillSelfExtras` buff döngüsü `m_buffLock` altında `m_nSkillID/m_bBuffType/m_bIsBuff/m_tEndTime` okur; `m_tEndTime = UNIXTIME + sDuration` (`MagicInstance.cpp:1812`) ve `SnapRemainingSec` süresi dolmuşları (`<= UNIXTIME`) eler. Çalışma zamanı buff gözlemi, bota Type4 atabilen bir dilim (veya insan istemcisi) gelince yapılmalı.
5. **Gerilemesiz:** `pot BotWP_K 999999` → `refused (bad_item)`; `pot BotWP_K 800003000 1` (Effect1 = Type4 skill) → `refused (unsupported_item)`; `snap` argümansız ve `snap BotWP_K extra` → `usage: snap <bot>`; `snap NoSuch` → `unknown or not spawned bot '?'`; `sit` → snap `alive sitting`, `stand`, `see`, `npcs`, `list`, `move`, `cast` çalıştı; `despawn all` → `3 despawning`, `pool free 16/16`; despawn sonrası `snap BotWP_K` → `not in game (phase despawned)`. Sunucu çökmedi, `FAIRNESS_REJECT` 0. `PERF_SAMPLE` 65 pencere: `tick_p95_us` medyan 94, 63/65 pencerede ≤ 500; 2 pencere 800 ve 1800 µs (komut yürütme pencereleri; aşağıda bulgu 2). `ENABLED=0` çalışma zamanında denenmedi (yukarıda K10).

**Bulgular (önem sırasıyla; hiçbiri engel değil):**
1. `BotWP_K` snap'inde `hp_pot` = telemetrideki 389015000 yığını + 1 (MB-01 `389014000`, tüketilmeyen ama `BeginPotion`'ın kabul ettiği tür): tasarımla uyumlu (plan "BeginPotion'ın kabul ettiği türler" diyor; ADR-0017 Ek F4-17 madde 2 MB-01'in sayıldığını zaten yazıyor). Karar katmanı gelince "kabul edilen ama tüketilmeyen pot (MB-01 `389014000`, `389020000`)" ile tüketilen pot ayrımı gerekebilir; o zaman ayrı alan açılır.
2. `tick_p95_us` 65 pencerenin ikisinde 500'ü aştı (800, 1800 µs); F4-16 turundaki örüntüyle aynı (komut yoklaması + çok satırlı günlük yazımı; `snap` artık 4+ ek satır yazıyor). Komutsuz pencerelerde sınır karşılanıyor. İzlenecek: periyodik snapshot kurulursa maliyet ayrıca ölçülmeli.
3. Plandaki senaryo 3 örneği (`110518`) 100 ms'lik yeniden-kullanımla gözlenemez; ileride senaryo yazarken `ReCastTime` ≥ 20 olan skill (ör. `110539`) seçilmeli.
4. Buff yolu çalışma zamanında gözlenmedi (bot Type4 atamıyor); birim test + kod okuması ile desteklendi.
5. Uygulayıcı Raporu'ndaki iddialar (commit'ler, derleme rc, 71 test, K1–K14, tek sapma `kPrintMaxSelf`) bağımsız olarak doğrulandı. Sapma uygun: `kPrintMax` taşınmasını yasaklayan `BotManager.cpp` "`-` satırı yok" kuralı nedeniyle ayrı sabit; değer aynı (10), kabul.

**Düzeltme talimatı:** yok (karar `DOĞRULANDI`).
