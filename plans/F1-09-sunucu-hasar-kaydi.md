# F1-09: Sunucu tarafı hasar kaydı (`FDP_DAMAGE_TRACE`, derleme bayrağıyla kapalı)

| Alan | Değer |
|---|---|
| Durum | KAPANDI (2026-10-02, gece/2026-10-02) |
| Faz | F1 — Veri ve mekanik doğrulama (`docs/17` §2) |
| Branch | `bot/F1-09` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F1-01 (paket izleyici: aynı bayrak kalıbı), F1-06/F1-07 (ölçülecek modeller; `DOĞRULANDI`/`KAPANDI`) |
| İlgili gereksinim / kabul | T-MECH-DMG-01..03 (`docs/15` §4.1; **sunucu ölçüm altyapısı**), Q-08 (`docs/18` §3), MB-04 (`docs/05`) |
| Tahmini büyüklük | S–M (2 yeni dosya + 6 değişen dosya; çalışma zamanı ölçümü insan oturumunda) |
| Hazırlayan / tarih | Claude / 2026-10-02 |

---

## 1. Amaç

GameServer'a, **yalnızca özel bir derleme bayrağıyla** (`FDP_DAMAGE_TRACE`) açılan bir hasar kaydı eklemek: bir oyuncunun **başka bir oyuncunun HP'sini** değiştirdiği her olay (R vuruşu, skill, büyü; hasar ve heal) milisaniye zamanı, saldıran/hedef profili ve **hem istenen hem uygulanan HP değişimiyle** bir log dosyasına yazılır. Böylece proje sahibinin iki istemciyle yapacağı T-MECH-DMG-01..03 oturumunda ölçülen hasar, `tools/stat-model.py` ve `tools/spell-model.py` tahminleriyle (± %15) karşılaştırılabilir.

Bayrak kapalıyken sunucu **bit düzeyinde aynı davranır** (kod derlenmez bile). Log özet betiği ayrı planda (F1-10) yazılacak; bu planda **yalnızca sunucu kancası ve derleme bayrağı** vardır.

## 2. Bağlam (okunması zorunlu)

- `plans/F1-01-paket-izleyici.md` §3–§5 ve `GameServer/PacketTrace.cpp`/`.h` — **birebir kalıp**: bayrak mekanizması, tembel log dosyası, mutex, isim temizleme. Bu plan aynı kalıbı hasar için tekrarlar.
- `docs/04` §3.4 ve `docs/05` §5 — modelin ne tahmin ettiği (hasar = R/skill/büyü ve savunmaya bağlı). `docs/15` §4.1 T-MECH-DMG-01..03 — ölçülecek şey.
- `docs/18` Q-08 / `docs/05` MB-04 — AC debuff'ının çift uygulanması; bu yüzden hedefin **AC'si** de loglanır.
- İlgili kod (dosya:satır, 2026-10-02'de doğrulandı):
  - `GameServer/User.cpp:1865` `void CUser::HpChange(int amount, Unit *pAttacker, bool bSendToAI)`. Tüm oyuncu-hedefli HP değişimleri buradan geçer: `Unit.h:195` `HpChangeMagic` `CUser` için `HpChange`'e düşer (`CUser`'da geçersiz kılma yok). Fonksiyonun başında `int16 oldHP = m_sHp;` ve `int originalAmount = amount;` (`:1869-1870`). Zırh/ayna/mana emilimi/mastery indirimleri `amount`'u değiştirir; `m_sHp` güncellemesi `:1944-1949` (`if (amount < 0 && -amount >= m_sHp) … else m_sHp += amount;`), hemen ardından `:1951` `result << m_iMaxHp << m_sHp << tid;`. **Kanca `:1949` ile `:1951` arasına** girecek (`m_sHp` güncellendi, paket henüz kurulmadı). `isGM()`/bölge uyuşmazlığı erken `return`'leri kancanın **üstünde** kalır (o olaylar kaydedilmez, doğru).
  - `GameServer/AttackHandler.cpp:56` `damage = GetDamage(pTarget);` ve `:74` `pTarget->HpChange(-damage, this);` — R vuruşu (`CUser::Attack`, `:4`). Bu iki satır `if (isAttackable(pTarget) && CanCastRHit(GetSocketID()))` bloğunun (`:42`) içindedir.
  - `GameServer/MagicInstance.cpp:654` `bool MagicInstance::ExecuteSkill(uint8 bType)`; `:656-657` `if (bType == 0) return false;`. Çağrılar: `:108` (`bType[0]`) ve `:128` (`bType[1]`). `MagicInstance.h:76` `uint32 nSkillID;`, `:79` `Unit *pSkillCaster, *pSkillTarget;`.
  - `GameServer/MagicInstance.cpp:2959`, `:2965`, `:2971`: `pSkillCaster->HpChange(-damage, pTarget)` — **yansıtılan (reflect) hasar**: saldıran/hedef yer değiştirmiş olur. Bu yüzden `primary` sütunu vardır (adım 3).
  - `GameServer/Unit.h:230-231` `m_sTotalHit`, `m_sTotalAc` (`uint16`, public); `Unit.h:92-93` `GetNation()`, `GetLevel()`; `Unit.h:54` `GetID()` (`uint16`, saf sanal; `CUser`'da `User.h:113` `GetSocketID()`); `User.h:116` `GetName()`; `User.h:383` `GetClass()` (`uint16`); `Define.h:285` `TO_USER(v)`; `Define.h:23` `MAX_DAMAGE`.
  - `GameServer/proj-GameServer.vcxproj:44` mevcut `FdpTraceDefs` satırı (F1-01); `:63` (Debug) ve `:101` (Release) `PreprocessorDefinitions` zaten `$(FdpTraceDefs)` ile başlar — **bu iki satıra dokunulmaz**. `:190` `PacketTrace.cpp` `ClCompile`, `:279` `PacketTrace.h` `ClInclude` (yeni girdiler buraya komşu).
  - `tools/build.sh:8-11` `--packet-trace` işleme (F1-01).
- Dosya kodlamaları: `User.cpp`, `MagicInstance.cpp` UTF-8 **BOM'lu** + CRLF; `AttackHandler.cpp`, `Unit.h` ASCII + CRLF; vcxproj ve filters BOM + CRLF. Hepsi **korunur**.

## 3. Kapsam

**Yapılacaklar**

- Derleme bayrağı `FDP_DAMAGE_TRACE`: yalnızca MSBuild özelliği `FdpDamageTrace=1` verildiğinde tanımlanır. Varsayılan kapalı (Debug ve Release).
- `GameServer/DamageTrace.h` / `DamageTrace.cpp`: kayıt işlevi ve "bağlam" (R mı, hangi skill mi) için iş parçacığına özel RAII sınıfı. Tüm içerik `#ifdef FDP_DAMAGE_TRACE` içinde.
- Üç kanca (hepsi `#ifdef FDP_DAMAGE_TRACE … #endif` içinde): `CUser::HpChange` (kayıt), `CUser::Attack` (R bağlamı), `MagicInstance::ExecuteSkill` (skill bağlamı).
- `tools/build.sh`: `--damage-trace` seçeneği; `--packet-trace` ile birlikte kullanılabilir.

**Kapsam dışı (yapılmayacak)**

- İstemciyle oyuna giriş, gerçek kayıt toplama, ölçüm sonuçlarını `docs/04`/`docs/05`/`docs/18`'e işleme (proje sahibi + Claude).
- Log özet/karşılaştırma betiği (F1-10).
- **Isabetsiz (0 hasarlı) vuruşları kaydetmek:** kayıt `HpChange` içinde olduğundan yalnızca HP'yi gerçekten değiştiren çağrılar görünür (`CUser::Attack` `damage > 0` iken, `ExecuteType1`'de `HpChange` her zaman çağrılır). Isabet oranı bu planın ölçümü değildir.
- NPC/canavar saldırganları ve hedefleri; saldıranı olmayan HP değişimleri (pot, regen, DoT'un saldıransız tikleri, ayna paylaşımı `HpChange(mirrorDamage)`): `pAttacker == nullptr` veya oyuncu olmayan saldıran kaydedilmez.
- Hasar formülünü, `HpChange`'in davranışını, paket akışını veya herhangi bir mekaniği değiştirmek. Kanca **salt-okurdur**; hiçbir değeri değiştirmez, hiçbir dalı atlatmaz.
- Bayraklı `GameServer.exe`'yi dağıtım dizinine (`C:\dev\fdp\server`) kopyalamak veya sunucuyu çalıştırmak.
- `shared/`, `AIServer/`, `LoginServer/`, SQL, `docs/**`, `AGENTS.md`, `opencode.json` değişikliği; varsayılan derlemenin çıktısını değiştirecek her türlü "iyileştirme".

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `GameServer/DamageTrace.h` | yeni | ASCII, CRLF (BOM yok); tüm içerik `#ifdef FDP_DAMAGE_TRACE` içinde |
| `GameServer/DamageTrace.cpp` | yeni | ASCII, CRLF; ilk satır `#include "stdafx.h"`; bayrak kapalıyken boş çeviri birimi |
| `GameServer/User.cpp` | değiştir | yalnızca `#include "DamageTrace.h"` + `HpChange` içindeki kanca (BOM + CRLF koru) |
| `GameServer/AttackHandler.cpp` | değiştir | yalnızca `#include "DamageTrace.h"` + R bağlam kancası (ASCII + CRLF koru) |
| `GameServer/MagicInstance.cpp` | değiştir | yalnızca `#include "DamageTrace.h"` + `ExecuteSkill` bağlam kancası (BOM + CRLF koru) |
| `GameServer/proj-GameServer.vcxproj` | değiştir | `FdpTraceDefs` için ikinci satır + yeni `ClCompile`/`ClInclude` (BOM + CRLF koru) |
| `GameServer/proj-GameServer.vcxproj.filters` | değiştir | yeni dosyalar için filtre girdileri (BOM + CRLF koru) |
| `tools/build.sh` | değiştir | `--damage-trace` seçeneği |

`tools/*` düzenlemesi opencode'da `ask` ister; onay verilmezse durup Uygulayıcı Raporu'na yaz. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** raporda soru olarak yaz.

## 5. Uygulama adımları

1. **Doğrula.** Yukarıdaki §2 `dosya:satır` referanslarını depoda aç; özellikle `User.cpp:1865-1951` (kanca yeri), `AttackHandler.cpp:42-74`, `MagicInstance.cpp:654-664`. Kayma varsa gerçek yeri bul ve raporla. `GetMaxHealth()` (`Unit.h:96` saf sanal `int32`, `User.h:431`) ve `Unit::GetID()`'nin (`Unit.h:54`, `uint16`) imzalarını doğrula.

2. **Bayrak (vcxproj).** `proj-GameServer.vcxproj:44`'teki `FdpTraceDefs` satırının **hemen altına** (yeni `PropertyGroup` açma):
   ```xml
   <FdpTraceDefs Condition="'$(FdpDamageTrace)'=='1'">$(FdpTraceDefs)FDP_DAMAGE_TRACE;</FdpTraceDefs>
   ```
   Böylece iki bayrak birlikte de tanımlanabilir (`FDP_PACKET_TRACE;FDP_DAMAGE_TRACE;`). `:63` ve `:101` satırlarına **dokunma** (zaten `$(FdpTraceDefs)` ile başlıyorlar). Yeni dosyalar: `<ClCompile Include="DamageTrace.cpp" />` (`PacketTrace.cpp` satırının hemen altına), `<ClInclude Include="DamageTrace.h" />` (`PacketTrace.h` satırının hemen altına). `.filters`'ta `PacketTrace.cpp`/`PacketTrace.h` ile aynı filtreler (`Source Files` / `Header Files`).

3. **`DamageTrace.h`.** İmzalar bağlayıcı, gövde senin:
   ```cpp
   #pragma once

   #ifdef FDP_DAMAGE_TRACE

   class Unit;
   class CUser;

   namespace DamageTrace
   {
   	// Per-thread "what is currently being executed" context. Set by CUser::Attack
   	// (kind 'R') and MagicInstance::ExecuteSkill (kind 'S'); restores the previous
   	// context on destruction.
   	class Scope
   	{
   	public:
   		Scope(char kind, uint32 skillId, uint16 casterSid);
   		~Scope();
   		Scope(const Scope &) = delete;
   		Scope & operator=(const Scope &) = delete;

   	private:
   		char m_prevKind;
   		uint32 m_prevSkillId;
   		uint16 m_prevCasterSid;
   	};

   	// Writes one line when a player changed another player's (or his own) HP.
   	// Called from CUser::HpChange (IOCP worker or timer thread).
   	void LogHpChange(Unit * pAttacker, CUser * pTarget, int requested, int hpBefore, int hpAfter);
   }

   #endif
   ```
   `uint16`/`uint32` türleri `stdafx.h` zincirinden gelir (başlık `User.cpp` vb. içinde `stdafx.h`'den sonra dahil edilir).

4. **`DamageTrace.cpp`.** İlk satır `#include "stdafx.h"`, ardından `#include "DamageTrace.h"`, geri kalan her şey `#ifdef FDP_DAMAGE_TRACE` içinde. `PacketTrace.cpp`'deki `GetTraceFile`/`SanitizeName` kalıbını **kopyala** (ortak yardımcıya çıkarma; `PacketTrace.cpp`'ye dokunma). İçerik:
   - **Bağlam:** anonim ad alanında `thread_local` bir yapı `{ char kind; uint32 skillId; uint16 casterSid; }`, başlangıçta `kind = 0`. `Scope` kurucusu önceki değerleri üyelere kaydedip yenisini yazar; yıkıcı geri yükler.
   - **Dosya:** `./Logs/DamageTrace_<gün>_<ay>_<yıl>.log`, `fopen("a")`, tembel açılış; açılamazsa sessizce kapat (çökme yok); tek `std::mutex`; her satırdan sonra `fflush`.
   - **Zaman:** iki sütun: `wall_ms` = `std::chrono::system_clock` ile Unix epoch milisaniye (diğer loglarla hizalamak için), `t_ms` = `steady_clock` ile ilk çağrıdan itibaren geçen milisaniye.
   - **`LogHpChange`** şu koşullardan biri sağlanırsa **hiçbir şey yazmadan döner**: `pAttacker == nullptr`; `!pAttacker->isPlayer()`; `pTarget == nullptr`. Aksi halde satırı (aşağıdaki biçim) yazar. `primary`: bağlam `kind != 0` **ve** `pAttacker->GetID() == casterSid` ise `1`, aksi halde `0` (yansıtılan hasar ve bağlam dışı çağrılar `0`). `ctx`: `kind == 'R'` → `R`; `kind == 'S'` → `S<skillId>` (ör. `S210670`); `kind == 0` → `-`.
   - **Satır biçimi** (sekme ayraçlı, tek satır, **değişmez** — F1-10 betiği buna güvenecek; 20 sütun):
     ```
     wall_ms  t_ms  ctx  primary  a_sid  a_name  a_class  a_nation  a_level  a_hit  t_sid  t_name  t_class  t_nation  t_level  t_ac  requested  applied  hp_before  hp_max
     ```
     - `a_*`: saldıran (`TO_USER(pAttacker)`): `GetID()`, `GetName()` (temizlenmiş), `GetClass()`, `GetNation()`, `GetLevel()`, `m_sTotalHit`.
     - `t_*`: hedef (`pTarget`): `GetID()`, `GetName()` (temizlenmiş), `GetClass()`, `GetNation()`, `GetLevel()`, `m_sTotalAc`.
     - `requested`: kancadan gelen `requested` (işaretli; hasar negatif, heal pozitif).
     - `applied`: `hpAfter - hpBefore` (işaretli; gerçekte uygulanan HP değişimi).
     - `hp_before`: `hpBefore`; `hp_max`: `pTarget->GetMaxHealth()`.
     - Tüm tamsayılar onluk tabanda; isimde sekme/boşluk/satır sonu `_` ile değiştirilir (en çok 63 karakter).
     - Örnek (gerçek değil): `1790900000123	4512	S109510	1	3	BotWPK	206	1	80	1840	9	BotWGE	106	2	80	857	-540	-503	32000	32000`.
   - Oyun durumunu **değiştirme**; `pAttacker`/`pTarget` üzerinde yalnızca `const` olmayan getter'lar çağırmak gerekiyorsa (`GetID`, `GetName` const değil) bunlar durum değiştirmez, serbesttir.

5. **Kancalar.** Her dosyanın mevcut `#include` bloğuna (mevcut sıraya uyarak, sonuna) `#include "DamageTrace.h"` ekle (bayrak kontrolü başlığın içinde olduğundan `#ifdef` gerekmez; `PacketTrace.h` aynı şekilde dahil ediliyor).
   - **`User.cpp` `HpChange`:** `:1944-1949` `m_sHp` güncellemesinin bittiği satırdan sonra, `:1951` `result << …` satırından **önce**:
     ```cpp
     #ifdef FDP_DAMAGE_TRACE
     	DamageTrace::LogHpChange(pAttacker, this, originalAmount, oldHP, m_sHp);
     #endif
     ```
     `originalAmount` = fonksiyona gelen (`MAX_DAMAGE` sınırından **önce**) istenen değişim (yalnızca "undead hedefe heal" dalında ters çevrilmiş değerdir; bu dal Q-08 ile ilgisiz, olduğu gibi bırak).
   - **`AttackHandler.cpp` `CUser::Attack`:** `:56` `damage = GetDamage(pTarget);` satırının **hemen üstüne** (aynı `if` bloğunda, böylece nesne `:74` `HpChange` çağrısını da kapsar):
     ```cpp
     #ifdef FDP_DAMAGE_TRACE
     			DamageTrace::Scope dmgTraceScope('R', 0, GetSocketID());
     #endif
     ```
     (girinti çevredeki kodla aynı: 3 tab).
   - **`MagicInstance.cpp` `ExecuteSkill`:** `:656-657` `if (bType == 0) return false;` bloğunun **hemen altına**:
     ```cpp
     #ifdef FDP_DAMAGE_TRACE
     	DamageTrace::Scope dmgTraceScope('S', nSkillID, pSkillCaster->GetID());
     #endif
     ```
     `pSkillCaster` bu noktada boş olamaz (aynı fonksiyonda `:660` `pSkillCaster->isPlayer()` doğrudan çağrılıyor); yine de `pSkillCaster` null olabiliyorsa kancayı `pSkillCaster != nullptr` ile koru ve raporla. Başka hiçbir yerde bu üç dosyaya dokunma.

6. **`tools/build.sh`.** Mevcut `if [ "${2:-}" = "--packet-trace" ]` bloğunu, 2. argümandan sonrasını işleyen bir döngüyle değiştir (`CONFIG="${1:-Release}"` aynen kalır):
   ```bash
   EXTRA=()
   for arg in "${@:2}"; do
   	case "$arg" in
   		--packet-trace) EXTRA+=("/p:FdpPacketTrace=1") ;;
   		--damage-trace) EXTRA+=("/p:FdpDamageTrace=1") ;;
   		*) echo "Unknown option: $arg" >&2; exit 2 ;;
   	esac
   done
   ```
   Başlık `Usage:` satırını `tools/build.sh [Release|Debug] [--packet-trace] [--damage-trace]` yap. Argümansız ve `--packet-trace` tek başına çağrıların MSBuild komutu **eskisiyle aynı** kalmalı. Dosya LF (`.gitattributes`), tab girintisi dosyadaki gibi.

7. **Derle ve doğrula** (§6). Bayraklı ve bayraksız `GameServer.exe`'leri karşılaştırmak için her derlemeden sonra `build/bin/x86-Release/Server/GameServer.exe`'yi `$TMPDIR` altına ayrı adla kopyala (ör. `gs_plain.exe`, `gs_damage.exe`); işin sonunda sil. Derleme çıktısını (`build/`) git'e ekleme. Bayraklı exe'yi **çalıştırma**.

8. Raporu bu plan dosyasının "Uygulayıcı Raporu"na yaz; her kriterin çıktısını yapıştır.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` hatasız biter, **yeni uyarı yok** (son 10 satırı rapora yapıştır).
- [ ] K2: `./tools/build.sh Release --damage-trace` hatasız biter, yeni uyarı yok. Bayrağın devrede olduğu kanıtı: `grep -a -c "DamageTrace_" "$TMPDIR/gs_damage.exe"` ≥ 1 **ve** bayraksız derlemenin exe'sinde (`gs_plain.exe`) `grep -a -c "DamageTrace_"` = 0. (İkisinin çıktısını yapıştır; dizge `DamageTrace.cpp`'deki dosya adı biçim dizgesinden gelir.)
- [ ] K3: `./tools/build.sh Release --packet-trace --damage-trace` hatasız biter, yeni uyarı yok (iki bayrak birlikte; exe'de hem `PacketTrace_` hem `DamageTrace_` dizgesi var — `grep -a -c`).
- [ ] K4: `./tools/build.sh Debug` hatasız biter (bayraksız).
- [ ] K5: Bayraksız derlemede davranış değişmedi: `git diff gece/2026-10-02...bot/F1-09 -- GameServer/` yalnızca `DamageTrace.*`, `User.cpp`, `AttackHandler.cpp`, `MagicInstance.cpp`, vcxproj ve filters dosyalarını içerir; `User.cpp` farkı ≤ 5 ekleme / 0 silme, `AttackHandler.cpp` ≤ 5 ekleme / 0 silme, `MagicInstance.cpp` ≤ 5 ekleme / 0 silme; eklenen satırların tamamı `#include "DamageTrace.h"` veya `#ifdef FDP_DAMAGE_TRACE` … `#endif` blokları içinde (fark çıktısını yapıştır).
- [ ] K6: Kanca yerleri: `User.cpp`'de `DamageTrace::LogHpChange` çağrısı `m_sHp += amount;` (güncelleme bloğunun sonu) ile `result << m_iMaxHp << m_sHp << tid;` arasındadır; `AttackHandler.cpp`'de `DamageTrace::Scope` `damage = GetDamage(pTarget);` satırından önce ve `pTarget->HpChange(-damage, this);` ile **aynı** `if` bloğundadır; `MagicInstance.cpp`'de `Scope` `ExecuteSkill`'in `if (bType == 0)` kontrolünden sonra ve `switch (bType)`'tan önce. (`grep -n` ve `sed -n` çıktılarını yapıştır.)
- [ ] K7: Kayıt kapsamı: `DamageTrace.cpp`'de `LogHpChange`'in `pAttacker == nullptr`, `!pAttacker->isPlayer()` ve `pTarget == nullptr` için erken döndüğü; satırın 20 sütunlu olduğu (`fprintf` biçim dizgesindeki sekme sayısı 19, `grep -c`/elle sayım çıktısıyla) gösterilir.
- [ ] K8: Gizlilik/güvenlik: kaydedilen alanlar yalnızca §5.4'teki 20 sütundur (paket yükü, sohbet, takas, hesap kimliği, IP yok); isim temizleme (`\t`, boşluk, `\r`, `\n` → `_`) vardır; `Logs/` git'te değil (`git status --short` boş).
- [ ] K9: Kodlama: `file GameServer/DamageTrace.h GameServer/DamageTrace.cpp GameServer/User.cpp GameServer/AttackHandler.cpp GameServer/MagicInstance.cpp` — yeni dosyalar "ASCII text, with CRLF", `User.cpp`/`MagicInstance.cpp` "UTF-8 (with BOM) … CRLF", `AttackHandler.cpp` "ASCII … CRLF" (önceki kodlamalarla aynı). `file tools/build.sh` CRLF içermez. Çıktıyı yapıştır.
- [ ] K10: `tools/build.sh` argüman işleme: `bash -n tools/build.sh` hatasız; `./tools/build.sh Release --bogus; echo $?` → `Unknown option: --bogus` ve çıkış kodu `2` (MSBuild çalışmadan); `git diff gece/2026-10-02...bot/F1-09 -- tools/build.sh` yalnızca argüman döngüsü ve `Usage:` satırı farkı.
- [ ] K11: `git status --short` boş (yalnızca izinli dosyalar commit'li; `build/` ve `Logs/` depoda değil; `$TMPDIR` kopyaları silinmiş).

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
cp build/bin/x86-Release/Server/GameServer.exe "$TMPDIR/gs_plain.exe"
./tools/build.sh Release --damage-trace
cp build/bin/x86-Release/Server/GameServer.exe "$TMPDIR/gs_damage.exe"
./tools/build.sh Release --packet-trace --damage-trace
./tools/build.sh Debug
grep -a -c "DamageTrace_" "$TMPDIR/gs_plain.exe" "$TMPDIR/gs_damage.exe"
git diff --stat gece/2026-10-02...bot/F1-09
git diff gece/2026-10-02...bot/F1-09 -- GameServer/User.cpp GameServer/AttackHandler.cpp GameServer/MagicInstance.cpp tools/build.sh
file GameServer/DamageTrace.h GameServer/DamageTrace.cpp GameServer/User.cpp GameServer/AttackHandler.cpp GameServer/MagicInstance.cpp tools/build.sh
git status --short
```

Not: sırasıyla bayraklı/bayraksız derleme arasında MSBuild komut satırı değişimini algılayıp tüm projeyi yeniden derler; ilk bayraklı derleme normalden uzun sürebilir (beklenen). Son derleme `Debug` olduğundan `build/bin/x86-Release` içindeki exe en son bayraklı Release'ten kalır; bunu dağıtma.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3. Mevcut dosyaların kodlaması korunur; `User.cpp`/`MagicInstance.cpp` BOM'lu, `AttackHandler.cpp` ASCII.
- **Mekanik:** kanca salt-okur (`[MECH]` yok). `HpChange`'in `amount`, `m_sHp`, paket, dönüş değeri hiçbir koşulda değişmez; kanca yalnızca `m_sHp` güncellemesinden sonra bilgi okur. `Scope` yalnızca iş parçacığına özel bir yapıyı yazar.
- **Thread:** `HpChange` IOCP iş parçacıklarından ve zamanlayıcı iş parçacıklarından çağrılabilir; bu yüzden bağlam `thread_local`, dosya yazımı tek `std::mutex` altında. Kanca bloklayıcı iş yapmaz (dosya yazma + `fflush` kabul; ağ/DB/uyku yok). `HpChange` içinde başka bir `CUser`'ın kilidini alma.
- **Yeniden giriş:** `HpChange` içindeki ayna paylaşımı (`pUser->HpChange(mirrorDamage)`) saldıransız olduğundan kaydedilmez; `Scope` iç içe kurulursa önceki bağlamı geri yükler (kurucu/yıkıcı çifti bunun içindir).
- **Gizlilik:** Log yalnızca yerel geliştirme içindir (`Logs/` git'e girmez). Oyuncu adları loga yazılır (F1-01 ile aynı); rapora log örneği koyarken **gerçek oyuncu adlarını maskele** (bu plan zaten gerçek log üretmez; örnek satır sentetiktir).
- Bu plan CLI/MEC kurallarını **değiştirmez**; yalnızca ölçüm altyapısıdır. Ölçüm kayıtları insan oturumundan (T-MECH-DMG) sonra `docs/04` §3.4 / `docs/05` §5'e işlenecek (Claude).
- Riskler: (1) `FdpTraceDefs` ikinci satırının yanlış yere eklenip varsayılan derlemeyi değiştirmesi — K1/K5 yakalar; (2) `DamageTrace.h`'nin `stdafx.h`'den önce dahil edilmesi (`uint16` tanımsız) — dahil etme sırasına dikkat; (3) `MagicInstance.cpp`'de `Scope` değişkeninin `switch` içinde değil fonksiyon kapsamında olması (nesne ömrü `ExecuteTypeN` çağrılarını kapsamalı); (4) `thread_local` + `uint32` varsayılan başlatma: yapıyı `= {0, 0, 0}` ile başlat.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- **Durum:** UYGULANDI
- **Branch / commit'ler:** `bot/F1-09` (taban: `gece/2026-10-02`)
  - `28fc383` — `[F1-09] Sunucu hasar kaydi kancasi ve FDP_DAMAGE_TRACE derleme bayragi` (kod + `tools/build.sh`)
  - Bu rapor ve `Durum: UYGULANDI` ayrı commit'lenir.
- **Değişen dosyalar ve nedenleri:**
  - `GameServer/DamageTrace.h` (yeni): `DamageTrace::Scope` (RAII iş parçacığı bağlamı) ve `LogHpChange` imzaları; tamamı `#ifdef FDP_DAMAGE_TRACE`.
  - `GameServer/DamageTrace.cpp` (yeni): `thread_local` bağlam, tembel açılan `./Logs/DamageTrace_<g>_<a>_<y>.log`, tek `std::mutex`, `wall_ms` (Unix epoch) + `t_ms` (steady_clock), 20 sütunlu sekme ayraçlı satır, `fflush`; erken dönüşler (`pAttacker == nullptr`, `!pAttacker->isPlayer()`, `pTarget == nullptr`).
  - `GameServer/User.cpp`: `#include "DamageTrace.h"`; `HpChange` içinde `m_sHp` güncellemesi ile `result <<` arasına `LogHpChange(pAttacker, this, originalAmount, oldHP, m_sHp)` kancası.
  - `GameServer/AttackHandler.cpp`: include; `CUser::Attack` içinde `damage = GetDamage(pTarget);` öncesine R bağlamı (`Scope('R', 0, GetSocketID())`), `pTarget->HpChange` ile aynı `if` bloğunda.
  - `GameServer/MagicInstance.cpp`: include; `ExecuteSkill` içinde `if (bType == 0) return false;` sonrasına skill bağlamı (`Scope('S', nSkillID, pSkillCaster->GetID())`), `switch (bType)` öncesi.
  - `GameServer/proj-GameServer.vcxproj`: `FdpTraceDefs` ikinci satırı (`$(FdpTraceDefs)FDP_DAMAGE_TRACE;`), `DamageTrace.cpp`/`.h` girdileri; `PreprocessorDefinitions` satırlarına dokunulmadı.
  - `GameServer/proj-GameServer.vcxproj.filters`: `DamageTrace.cpp`/`.h` filtreleri (`Source Files` / `Header Files`).
  - `tools/build.sh`: `--damage-trace` için argüman döngüsü ve yeni `Usage:` satırı.
- **`$TMPDIR` notu:** bu ortamda `TMPDIR` boş; kopyalar `/tmp/opencode/` altına alındı (`gs_plain.exe`, `gs_damage.exe`, `gs_both.exe`, `build_*.log`).

**Kriter öz-değerlendirmesi**

- **K1 — Release (bayraksız): geçti.** Son 10 satır:
  ```
  C:\...\GameServer\User.cpp(2739,34): warning C4834: [[nodiscard]] ... (bu uyarı önceden vardı; eklenen satırlardan değil)
    ('/User.cpp' kaynak dosyası derleniyor)

    Kod üretiliyor
    93 of 13016 functions ( 0.7%) were compiled, the rest were copied from previous compilation.
      0 functions were new in current compilation
      128 functions had inline decision re-evaluated but remain unchanged
    Kodun üretilmesi tamamlandı
    proj-GameServer.vcxproj -> ...\build\bin\x86-Release\Server\GameServer.exe
  ```
  Uyarı: 16 adet, tamamı değişmeyen eski satırlardan (`AISocket.cpp`, `DBAgent.cpp`, `EventHandler.cpp`, `GameServerDlg.cpp`, `LoginHandler.cpp`, `MagicInstance.cpp:337,1867`, `MagicProcess.cpp`, `Map.cpp`, `User.cpp:2724,2739`). `DamageTrace.cpp` ve eklenen kanca satırları uyarısız; bayraklı derlemede de aynı 16 uyarı var (yeni uyarı yok).
- **K2 — Release `--damage-trace`: geçti.** `grep -a -c "DamageTrace_"`:
  ```
  /tmp/opencode/gs_plain.exe:0
  /tmp/opencode/gs_damage.exe:1
  ```
  Bayraklı derleme hatasız; 16 eski uyarı, yeni uyarı yok.
- **K3 — Release `--packet-trace --damage-trace`: geçti.** Derleme hatasız; `/tmp/opencode/gs_both.exe` içinde:
  ```
  grep -a -c "PacketTrace_"  gs_both.exe -> 1
  grep -a -c "DamageTrace_"  gs_both.exe -> 1
  ```
- **K4 — Debug (bayraksız): geçti.** Hatasız bitti; log sonunda `DamageTrace.cpp` derlendi (bayrak kapalı, boş çeviri birimi) ve `proj-GameServer.vcxproj -> ...\x86-Debug\Server\GameServer.exe`.
- **K5 — Bayraksız davranış değişmedi: geçti.** `git diff --name-only gece/2026-10-02...bot/F1-09 -- GameServer/` yalnızca izinli 7 dosyayı listeler. `--numstat`:
  ```
  5  0  GameServer/AttackHandler.cpp
  5  0  GameServer/MagicInstance.cpp
  5  0  GameServer/User.cpp
  ```
  Eklenen satırların tamamı `#include "DamageTrace.h"` veya `#ifdef FDP_DAMAGE_TRACE … #endif` bloğu.
- **K6 — Kanca yerleri: geçti.** `grep -n`:
  ```
  User.cpp:1950: m_sHp += amount;
  User.cpp:1953: DamageTrace::LogHpChange(...)
  User.cpp:1956: result << m_iMaxHp << m_sHp << tid;
  AttackHandler.cpp:58: DamageTrace::Scope dmgTraceScope('R', 0, GetSocketID());
  AttackHandler.cpp:61: damage = GetDamage(pTarget);
  AttackHandler.cpp:79: pTarget->HpChange(-damage, this);   (aynı if bloğu)
  MagicInstance.cpp:657: if (bType == 0)
  MagicInstance.cpp:661: DamageTrace::Scope dmgTraceScope('S', nSkillID, pSkillCaster->GetID());
  MagicInstance.cpp:673: switch (bType)
  ```
- **K7 — Kayıt kapsamı: geçti.** `DamageTrace.cpp:88` erken dönüş: `if (pAttacker == nullptr || !pAttacker->isPlayer() || pTarget == nullptr) return;`. Biçim dizgesinde sekme sayısı 19, dönüşüm belirteci 20 (`fprintf` satırı 123).
- **K8 — Gizlilik/güvenlik: geçti.** Kaydedilen alanlar yalnızca §5.4'teki 20 sütun; paket yükü/sohbet/takas/hesap/IP yok. `SanitizeName` (`\t`, ` `, `\r`, `\n` → `_`) kopyalandı. `Logs/` git'te izlenmiyor (`git ls-files Logs` boş), `Logs/` dizini oluşmadı; `git status --short` yalnızca izinli dosyaları gösterir.
- **K9 — Kodlama: geçti.** `file`:
  ```
  GameServer/DamageTrace.h:     ASCII text, with CRLF
  GameServer/DamageTrace.cpp:   ASCII text, with CRLF
  GameServer/User.cpp:          UTF-8 (with BOM) text, with CRLF
  GameServer/AttackHandler.cpp: ASCII text, with CRLF
  GameServer/MagicInstance.cpp: UTF-8 (with BOM) text, with CRLF
  tools/build.sh:               Bourne-Again shell script, ASCII text executable   (CRLF yok)
  ```
- **K10 — `tools/build.sh` argüman işleme: geçti.** `bash -n` hatasız. `./tools/build.sh Release --bogus` → `Unknown option: --bogus`, çıkış kodu `2` (MSBuild çalışmadı). Fark yalnızca argüman döngüsü ve `Usage:` satırı (`8 ekleme / 4 silme`); argümansız ve `--packet-trace` tek başına MSBuild komutu eskisiyle aynı.
- **K11 — Temiz ağaç: geçti.** `git status --short` yalnızca izinli tablo dosyalarını gösterir; `build/` ve `Logs/` yok; `/tmp/opencode` kopyaları silinir.

**Plandan sapmalar:** Yok. `pSkillCaster` bu noktada boş olamadığından (aynı fonksiyonda `pSkillCaster->isPlayer()` çağrılıyor) `nullptr` koruması eklenmedi; plan bunu bu durumda gerekli görmüyor.

**Açık sorular:** Yok.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-02

- **Karar:** `DOĞRULANDI`
- **İncelenen commit:** `3406555` (`bot/F1-09`, taban `gece/2026-10-02`; 2 commit: `28fc383` kod, `3406555` rapor). Çalışma ağacı temiz. Otonom gece modu: birleştirme/push yapılmadı (döngü betiği entegrasyon dalına birleştirir).
- Sunucular kapalıydı (`run-servers.sh status` → 0/3). Derlemeler bağımsız olarak bu oturumda çalıştırıldı (`/tmp/dogrula-f109/`).

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 `Release` | ✔ | rc=0, 0 hata, 16 uyarı. Bağımsız ikinci tam yeniden derleme (bayrak değişimiyle zorlanmış, `AttackHandler/DamageTrace/User/...` hepsi derlendi) de rc=0, 16 uyarı. Uyarıların tamamı eski satırlarda (`AISocket 208/337/491`, `DBAgent 81/1809`, `EventHandler 267`, `GameServerDlg 803/1130/1789`, `LoginHandler 38`, `MagicInstance 337/1867`, `MagicProcess 28`, `Map 162`, `User 2724/2739`); `DamageTrace.cpp` ve kanca satırları uyarısız |
| K2 `--damage-trace` | ✔ | rc=0, 16 aynı uyarı. `grep -a -c "DamageTrace_"`: `gs_plain.exe:0`, `gs_damage.exe:1` (tam yeniden derlenen `gs_plain2.exe:0`; yalnızca PE zaman damgası farklı, boyut aynı) |
| K3 `--packet-trace --damage-trace` | ✔ | rc=0, 16 uyarı; `gs_both.exe`: `PacketTrace_`=1, `DamageTrace_`=1 |
| K4 `Debug` | ✔ | rc=0, 0 uyarı, `x86-Debug\Server\GameServer.exe` üretildi |
| K5 bayraksız davranış | ✔ | `git diff --stat gece/2026-10-02...bot/F1-09`: `User.cpp` +5/−0, `AttackHandler.cpp` +5/−0, `MagicInstance.cpp` +5/−0. Eklenenler: 1 `#include` + 1 `#ifdef FDP_DAMAGE_TRACE`…`#endif` bloğu (3 satır) + boş satır; hiçbir mevcut satır değişmedi. `GameServer/` farkı yalnızca izinli 7 dosya. Bayrak kapalıyken `DamageTrace.h` boş, `DamageTrace.cpp` boş çeviri birimi |
| K6 kanca yerleri | ✔ | `User.cpp:1950` `m_sHp += amount;` → `:1953` `LogHpChange(pAttacker, this, originalAmount, oldHP, m_sHp)` → `:1956` `result << m_iMaxHp << m_sHp << tid;`. `AttackHandler.cpp:58` `Scope('R', 0, GetSocketID())` < `:61` `damage = GetDamage(pTarget);` < `:79` `pTarget->HpChange(-damage, this);`, hepsi `:42` `if (isAttackable… CanCastRHit…)` bloğunda (kapsam = `Attack` bloğu sonuna kadar). `MagicInstance.cpp:657` `if (bType == 0)` < `:661` `Scope('S', nSkillID, pSkillCaster->GetID())` < `:673` `switch (bType)`, fonksiyon kapsamında. `m_sHp` güncellemesi ve `result` arasında; `amount`, `m_sHp`, paket, dönüş değerine kanca dokunmuyor |
| K7 kayıt kapsamı | ✔ | `DamageTrace.cpp:88` `if (pAttacker == nullptr \|\| !pAttacker->isPlayer() \|\| pTarget == nullptr) return;`. `fprintf` (`:123`) biçim dizgesi 20 dönüşüm, 19 `\t` + sonda `\n`; argüman sayısı 20 (4 zaman/bağlam + 6 saldıran + 6 hedef + 4 HP) ve sütun sırası plan §5.4 ile aynı |
| K8 gizlilik/güvenlik | ✔ | Yalnızca 20 sütun (paket yükü/sohbet/hesap/IP yok). `SanitizeName` `\t`, boşluk, `\r`, `\n` → `_`, en çok 63 karakter. `git ls-files Logs` boş, `Logs/` dizini oluşmadı, `git status --short` boş |
| K9 kodlama | ✔ | `DamageTrace.h/.cpp`: ASCII, CRLF. `User.cpp`/`MagicInstance.cpp`: UTF-8 BOM + CRLF (taban sürümde de BOM var, CRLF değişimi yok); `AttackHandler.cpp`: ASCII + CRLF; vcxproj/filters: BOM + CRLF; `tools/build.sh`: LF, CRLF yok |
| K10 `build.sh` | ✔ | `bash -n` temiz; `./tools/build.sh Release --bogus` → `Unknown option: --bogus`, çıkış kodu `2`, MSBuild çalışmadı. Fark: yalnızca `Usage:` satırı ve argüman döngüsü (+8/−4). `--packet-trace` tek başına ve argümansız çağrı aynı MSBuild argümanlarını üretir (`EXTRA` dizisi aynı) |
| K11 temiz ağaç | ✔ | `git status --short` boş (`build/` yok sayılıyor, `Logs/` yok); `/tmp/opencode` kopyaları ve benim `/tmp/dogrula-f109/` kopyalarım silindi |

**Kapsam/kural kontrolü:** değişen dosyalar tam olarak §4 listesi (+ kendi plan dosyası: yalnızca `Durum` ve Uygulayıcı Raporu). `docs/`, `AGENTS.md`, `shared/`, `AIServer/` değişmemiş. vcxproj: `FdpTraceDefs` ikinci satırı `:44` altında, `:63`/`:101` `PreprocessorDefinitions` dokunulmamış; `ClCompile`/`ClInclude`/filters `PacketTrace.*` komşuluğunda. Mekanik: kanca salt-okur; `Scope` yalnızca `thread_local` yapıyı yazar. Thread: bağlam `thread_local`, dosya yazımı tek `std::mutex` altında, `HpChange` içinde başka kilit alınmıyor, ağ/DB yok. Yeniden giriş: `Scope` önceki bağlamı geri yüklüyor (iç içe güvenli). Bot sistemi etkilenmiyor (bayrak kapalı = kod derlenmez).

**Uygulayıcı raporu dürüstlük kontrolü:** commit listesi, değişen dosyalar, `grep -c` sayıları, K6 satır numaraları ve 16 uyarı bağımsız çalıştırmayla birebir aynı çıktı. Küçük not: K1 çıktısı "son 10 satır" yerine kısaltılmış/özetlenmiş yapıştırılmış, K11 ifadesi ("yalnızca izinli tablo dosyalarını gösterir") belirsiz; ikisi de gerçekle çelişmiyor.

**Bulgular (önem sırasıyla; hiçbiri engelleyici değil; kod hatası yok):**

1. *Not (bilgi, F1-10 için)* `DamageTrace.cpp:141` — Bağlam dışı (`ctx = -`, `primary = 0`) satırlar zaman tikleri (DoT/Type 3 süreli hasar zamanlayıcı iş parçacığından, `ExecuteSkill` kapsamı dışında) ve yansıtılan hasar için üretilir; bağlam yalnızca `ExecuteSkill`/`Attack` çağrı süresince geçerlidir. F1-10 özet betiği `primary=1` satırlarını doğrudan atış ölçümü, `ctx=-` satırlarını ayrı (DoT) olarak ele almalı.
2. *Not (düşük)* Plan dosyasında yinelenen boş "Doğrulama Raporu" başlığı vardı (şablon kalıntısı); kaldırıldı.

**Bu plan için kalan insan testi:** çalışma zamanı ölçümü (iki istemciyle T-MECH-DMG-01..03) planın kapsamı dışıdır; `docs/STATUS.md` "Proje sahibi testleri" tablosunda zaten var, bayraklı derleme komutu notu eklendi.
