# F4-42: T-MECH-SKILL botla koşusu, dilim 2a — betik üreteci `tools/skill-script-gen.py` ve Karus priest dost hedefli skill betiği

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2; ADR-0018 m.9 "T-MECH-SKILL'in botla yeniden koşusu", Ek 18) |
| Branch | `bot/F4-42` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-41 (`tools/skill-check.py`, cast `mp`/`mp_after`/`skill` telemetrisi) — `KAPANDI` (merge `0901ae0`); F4-40 (envanter doldurma, MP pot stoku) — `KAPANDI`; F4-19/F4-22 (betik ayrıştırıcı ve `Scripts/` yükleyici) — `KAPANDI` |
| İlgili gereksinim / kabul | `docs/05` §9 (T-MECH-SKILL-P-*: MP düşümü, recast, etki, fail sebebi), `docs/03` MEC-MAG-02/-08, CLI-04; `docs/17` F4 "Teslimatlar: T-MECH-SKILL-* bot kanıtları"; ADR-0018 m.9, Ek 17, Ek 18 |
| Tahmini büyüklük | S–M (1 yeni Python aracı + 1 spec + 1 üretilmiş betik; C++ yok) |
| Hazırlayan / tarih | Claude / 2026-10-03 (otonom gece döngüsü) |

---

## 1. Amaç

F4-41 ölçüm aracını (`tools/skill-check.py`) verdi; şimdi ölçüm **koşusunun girdisi** gerekiyor: botların her skill'i yeterli sayıda ve doğru aralıkla atmasını sağlayan bir test betiği (`Scripts/<ad>.txt`). Elle yazılmış betikte zaman kayması, MP bitmesi veya üst üste binen cast'ler ölçümü bozar ve betik sınırları (`kScriptMaxSteps = 100`, 8192 bayt, 128 satır, 600000 ms) kolayca aşılır. Bu plan (a) `MAGIC` verisinden **cast süresi ve recast'e göre ofset hesaplayan** `tools/skill-script-gen.py` aracını, (b) Karus priest'in (`BotPHD_K`, `BotPHB_K`) **dost hedefli / kendine / party** skill'lerini listeleyen `bots/config/skill_priest_k.spec` dosyasını ve (c) bundan üretilmiş `bots/config/skill_priest_k.txt` betiğini ekler. Gerçek koşu ve sonuçların `docs/05`'e işlenmesi Claude'un çalışma zamanı doğrulamasıdır (K7). Düşman hedefli priest skill'leri (curse), warrior ve mage dilimleri sonraki planlardır (§3 Kapsam dışı).

## 2. Bağlam (okunması zorunlu)

- `docs/05` §6 (priest çekirdek skill tablosu), §9 (T-MECH-SKILL), `docs/03` MEC-MAG-02 (`ReCastTime` 0,1 sn birimi), MEC-BUF-02 (aynı `BuffType` etkinken buff reddi).
- `tools/skill-check.py` (kullanım, `--min-n`, `load_magic_sql`/`parse_magic_lines` kalıbı: **kopyala, `import` etme**, araçlar bağımsız betiklerdir) ve `tools/bot-refill.sh` (MP pot stoku: varsayılan `--mp-pots 0`).
- İlgili kod (hepsini açıp doğrula; satırlar `0901ae0` itibarıyladır):
  - `BotCore/ScriptPlan.h:12-16` betik sınırları (`kScriptMaxBytes 8192`, `kScriptMaxLines 128`, `kScriptMaxSteps 100`, `kScriptMaxLineLen 255`, `kScriptMaxOffsetMs 600000`); `:62-67` izinli 20 fiil (`move stop attack cast pot sit stand target regene pinvite paccept pdecline pleave ppromote pkick pchat see npcs snap list`); `ParseScript` ofsetin azalmasına izin vermez (`SCRIPT_ERR_OFFSET_ORDER`), yorum (`#`) ve boş satırlar da 128 satır sınırına dahildir.
  - `GameServer/Bot/ScriptRunner.cpp:75` betik dosyası `./Scripts/<ad>.txt` (sunucu klasörü), `:88` `ParseScript`.
  - `BotCore/BotCombat.h:125-139`: `kCastExtraMs = 80`, `kCastGapMs = 140`, `kTypeGateMs = 1000`, `CastDurationMs(castTime) = castTime == 0 ? 0 : castTime*100 + 80`, `CastRecastMs(reCastTime) = reCastTime*100`; `:588` `kPotCooldownMs = 2500` (HP/MP pot ortak süresi).
  - `GameServer/Bot/BotManager.cpp:1381-1382` ve `:1566` `cast <bot> <skill id> <target bot|self> [cycles]` (cycles 1..20; `cycles` aynı skill'in recast beklenerek tekrar atılmasıdır: `ActionExecutor.cpp:963` `CastWaitMs > 0` iken `ARMED` bekler), `:1588-1589` `pot <bot> <item id> [count]` (count 1..20; `:1708`).
  - Bot kimlikleri ve beceri (`db/002_bot_characters.sql:126-137`): `BotPHD_K` (priest, `skillHex 0x00000000003C003E1400`: Heal 60, [7] = 62, master 20), `BotPHB_K` (`0x00000000003C3E001400`: Heal 60, [6] = 62, master 20), `BotWP_K`, `BotWG_K` (warrior, dost hedef olarak).
- `MAGIC` doğrulaması (sqlcmd, yalnızca `MAGIC`; `Msp, CastTime, ReCastTime, Range, Type1, Moral`): `112527` Great healing `80/15/20/56/3/2`; `112536` `160/15/1/56/3/2`; `112545` `320/15/1/56/3/2`; `112548` `625/15/1/56/3/2`; `112554` `960/15/54/56/3/2`; `112557` `960/15/54/56/3/6`; `112560` `1920/15/64/56/3/6`; `112525` Cure curse `60/15/15/56/5/2`; `112535` `120/15/15/56/5/2`; `112660` `150/15/1/56/4/2`; `112645` `60/15/1/56/4/2`; `112654` `240/15/1/56/4/4`; `112657` `360/15/1/56/4/4`; `112656` `570/15/1/101/4/6`; `112820` `320/15/1/45/4/1`. Uygulayıcı önce aynı sorguyu çalıştırıp kimliklerin ve değerlerin hâlâ böyle olduğunu doğrulasın (farklıysa Uygulayıcı Raporu'na yaz, spec'i gerçek değere göre yazma kararı Claude'dadır).

## 3. Kapsam

**Yapılacaklar**

1. `tools/skill-script-gen.py` (yeni): spec dosyası + `MAGIC` → ofsetleri hesaplanmış, `ParseScript` sınırlarına uyan `Scripts/<ad>.txt` metni. `--check` ile var olan bir betiği aynı sınırlara karşı denetler. `--selftest` ile.
2. `bots/config/skill_priest_k.spec` (yeni): §5.2'deki Karus priest listesi.
3. `bots/config/skill_priest_k.txt` (yeni): 2'den aracın ürettiği betik (elle düzenlenmez; K3 yeniden üretip karşılaştırır).

**Kapsam dışı (yapılmayacak)**

- C++ kodu (`GameServer/`, `BotCore/`, `Tests/`): **yok**; bu plan davranışı değiştirmez. `.vcxproj` değişmez.
- Düşman hedefli skill'ler (priest curse `112703/112724/112736/112745/112757/112760`, Judgment/Helis), warrior ve mage betikleri, **uçan skill'lerde MP'nin iki kez düşmesi** (MEC-MAG-12) için `skill-check.py` beklenti düzeltmesi, bot konumlandırma/`move`: **F4-43+** (mage dilimi uçan skill'e girdiğinde karar verilir).
- Diriltme (`112733..`), `112825` Elysian Web (`Moral` 11, bot açmıyor), `112802` Judgment (düşman hedefli): koşulmaz.
- `tools/skill-check.py`, `tools/bot-refill.sh`, `db/*`, `docs/`, ADR, senaryo YAML'ı: **değişmez** (docs/ADR'yi Claude yazar). Gerçek koşu (sunucu açma, `GameServer.ini`, bot spawn) ve sonuçları `docs/05`'e işlemek Claude'un işidir; DeepSeek sunucu açmaz.
- Yeni fiil, yeni betik sözdizimi, `ScriptPlan.h` değişikliği: yok.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `tools/skill-script-gen.py` | yeni | araç + `--check` + `--selftest`; yalnızca standart kütüphane |
| `bots/config/skill_priest_k.spec` | yeni | §5.2 listesi |
| `bots/config/skill_priest_k.txt` | yeni | araç çıktısı (üretilmiş) |

Plan dosyası dahil 4 dosya. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

### 5.1 Branch

`git switch -c bot/F4-42 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop` (`AGENTS.md` §4; bu plan sunucu açmaz ama derleme exe kilidine takılmasın).

### 5.2 Spec biçimi ve priest spec'i

Spec, satır tabanlı metindir (`#` yorum, boş satır serbest, UTF-8/ASCII). Üç satır türü:

```
raw  <offset_ms> <komut ve argümanlar>      # sabit ofsette aynen geçirilen adım (yalnızca ön hazırlık: party kurma)
cast <bot> <skill_id> <hedef bot|self> <cycles 1..20>
pot  <bot> <item_id> <count 1..20>
```

- `raw` satırlarının fiili `ScriptPlan.h` listesinde olmalıdır; değilse hata (çıkış 2). `raw` ofsetleri hazırlık aşamasını belirler: `t0 = (en büyük raw ofseti) + 1500` ms (raw yoksa `t0 = 0`; `--start-gap-ms` ile değişir).
- Her **bot** için ayrı bir imleç `c` tutulur (`c = t0` ile başlar); aynı botun `cast`/`pot` satırları spec sırasıyla, **art arda**; farklı botlar **paralel** çalışır (imleçler bağımsız).
- `cast` adımı ofset `c`'de yazılır, ardından `c += cycles * period + margin_ms`; `period = max(recast_ms, cast_ms + 140, 1000)` (`cast_ms = CastTime == 0 ? 0 : CastTime*100 + 80`, `recast_ms = ReCastTime*100`; 140 `kCastGapMs`, 1000 `kTypeGateMs` ve en küçük aralık). `cycles * period` son atıştan sonraki recast'i de kapsar, böylece aynı skill'in sonraki satırı recast'e takılmaz.
- `pot` adımı ofset `c`'de yazılır, ardından `c += count * 2500 + margin_ms` (`kPotCooldownMs`).
- `margin_ms` varsayılan 1500 (`--margin-ms N`). Skill kimliği `MAGIC`'te yoksa hata (çıkış 2).
- Çıktı: ilk satırlar `# generated by tools/skill-script-gen.py from <spec adı>; do not edit by hand.` ve `# bots: <sıralı bot adları>`; ardından adımlar **ofsete göre sıralı** (eşit ofsette spec sırası korunur), sonda en son bitiş + 500 ms ofsetinde bir `list` adımı. Biçim `<offset> <komut>`, LF, ASCII.
- Sınırlar (`ScriptPlan.h`) aşılırsa (adım > 100, bayt > 8192, satır > 128, satır > 255, ofset > 600000) çıkış 1, stderr'e hangi sınır ve değer; dosya yazılmaz.

`bots/config/skill_priest_k.spec` içeriği (bu sırayla; yorum satırlarını İngilizce yaz):

```
# party: BotPHD_K leads, the other three join (Moral 4 / 6 skills need a party)
raw 0    pinvite BotPHD_K BotPHB_K
raw 1000 paccept BotPHB_K
raw 2000 pinvite BotPHD_K BotWP_K
raw 3000 paccept BotWP_K
raw 4000 pinvite BotPHD_K BotWG_K
raw 5000 paccept BotWG_K

# BotPHD_K: heals and cures on BotWP_K (Heal 60); MP pots between the expensive ones
cast BotPHD_K 112527 BotWP_K 3
cast BotPHD_K 112536 BotWP_K 3
cast BotPHD_K 112545 BotWP_K 3
cast BotPHD_K 112525 BotWP_K 3
cast BotPHD_K 112535 BotWP_K 3
cast BotPHD_K 112548 BotWP_K 3
pot  BotPHD_K 389220000 3
cast BotPHD_K 112554 BotWP_K 3
cast BotPHD_K 112557 BotWP_K 3
pot  BotPHD_K 389220000 3
cast BotPHD_K 112560 BotWP_K 2

# BotPHB_K: buffs; one target per sample because an active same-BuffType buff is rejected (MEC-BUF-02)
cast BotPHB_K 112660 BotWP_K 1
cast BotPHB_K 112660 BotPHD_K 1
cast BotPHB_K 112660 BotWG_K 1
cast BotPHB_K 112645 BotWP_K 1
cast BotPHB_K 112645 BotPHD_K 1
cast BotPHB_K 112645 BotWG_K 1
cast BotPHB_K 112654 BotWP_K 1
cast BotPHB_K 112654 BotWG_K 1
cast BotPHB_K 112654 BotPHD_K 1
cast BotPHB_K 112657 self 1
cast BotPHB_K 112656 self 1
cast BotPHB_K 112820 self 1
```

Not: `112557/112560` `Moral` 6 (party-all), `112654/112657` `Moral` 4 (party üyesi), `112656` `Moral` 6, `112820` `Moral` 1: hepsi `CastMoralSupported`'tadır (`BotCombat.h:373-381`). Bir skill bot kuralıyla reddedilirse (`unsupported_skill`, `quest_locked`, `no_item`...) bu bir **bulgudur**, spec'ten çıkarma.

### 5.3 Araç (`tools/skill-script-gen.py`)

Komut satırı:

```
python3 tools/skill-script-gen.py SPEC [--out FILE] [--magic FILE | --sqlcmd P --server S --db D]
                                       [--margin-ms N] [--start-gap-ms N]
python3 tools/skill-script-gen.py --check SCRIPT
python3 tools/skill-script-gen.py --selftest
```

- `--out` yoksa betik stdout'a; varsa dosyaya (UTF-8, LF). `--magic FILE`: `skill-check.py`'deki `MagicNum|EnName|Msp|CastTime|ReCastTime|Range|Type1|Type2` satır biçimi (aynı ayrıştırıcıyı kopyala); verilmezse `sqlcmd` ile **yalnızca** `SELECT MagicNum, RTRIM(EnName), Msp, CastTime, ReCastTime, Range, Type1, Type2 FROM MAGIC` (`skill-check.py` `load_magic_sql`'deki **bayt olarak çöz** düzeltmesini koru: `capture_output=True`, `decode("utf-8", errors="replace")`; gerçek `MAGIC`'te UTF-8 olmayan bayt vardır).
- Çıkış kodları: 0 başarı; 1 betik sınırı aşıldı (`--check`'te sınır ihlali); 2 kullanım/girdi/spec hatası (bilinmeyen satır türü, bozuk sayı, `cycles`/`count` aralık dışı, bilinmeyen skill, izinsiz fiil, okunamayan dosya). Hata mesajı `satır numarası + sebep` içerir.
- `--check SCRIPT`: dosyayı `ParseScript` kurallarıyla denetler (bayt, satır, satır uzunluğu, kontrol karakteri, ofset yalnızca rakam ve azalmayan ve ≤ 600000, fiil izinli listede, adım ≤ 100, boş betik yok), ihlalde `satır: sebep` basar, çıkış 1; temizse `ok: N steps, M bytes, K lines, last offset T ms`, çıkış 0.
- Çıktı **belirlenimli**: aynı spec + aynı `MAGIC` her seferinde bayt bayt aynı betiği verir.

### 5.4 Birim sınaması (`--selftest`)

Geçici klasörde, bellek içi `MAGIC` ile (DB/sunucu yok); hepsi geçerse son satır `selftest: N checks, 0 failed`, çıkış 0. En az şu adlandırılmış kontroller (N ≥ 14):

1. `cursor_sequential`: aynı botun iki `cast` satırı art arda, ikincinin ofseti `t0 + cycles*period + margin`.
2. `bots_parallel`: iki bot aynı `t0`'da başlar.
3. `period_recast_dominates`: `ReCastTime 54` (5400), `CastTime 15` → `period 5400`.
4. `period_cast_dominates`: `ReCastTime 1`, `CastTime 15` → `period 1720` (`1580 + 140`).
5. `period_floor_1000`: `CastTime 0`, `ReCastTime 1` → `period 1000`.
6. `pot_advance`: `pot ... 3` → imleç `3*2500 + margin` ilerler.
7. `raw_sets_t0`: son `raw` ofseti 5000 → `t0 = 6500`; raw yoksa `t0 = 0`.
8. `sorted_stable`: çıktı ofsete göre sıralı, eşit ofsette spec sırası korunur.
9. `trailing_list`: son satır `list`, ofseti son bitişten 500 ms sonra.
10. `unknown_skill`: `MAGIC`'te olmayan kimlik → çıkış 2, mesajda satır numarası.
11. `bad_verb_raw`: `raw 0 teleport X` → çıkış 2.
12. `bad_cycles`: `cycles 0` ve `21` → çıkış 2; `pot` için `count 0` ve `21` → çıkış 2.
13. `step_limit`: 101 adımlık spec → çıkış 1, dosya yazılmaz.
14. `offset_limit`: ofset 600001'e çıkan spec → çıkış 1.
15. `deterministic`: aynı girdiyle iki üretim aynı bayt dizisi.
16. `check_ok_and_bad`: `--check` temiz betikte 0; azalan ofsetli, 129 satırlı, 256 karakterlik satırlı ve izinsiz fiilli betiklerde 1 (her biri ayrı kontrol).
17. `sqlcmd_non_utf8`: sahte `sqlcmd` betiği (POSIX; `printf '112527|great\250|80|15|20|56|3|0\n'`, bayt `0xA8`) → çökmeden `{112527: ...}` (Windows'ta `sqlcmd_non_utf8_skipped` adıyla sayılır).

## 6. Kabul kriterleri

- [ ] K1: `python3 tools/skill-script-gen.py --selftest` çıkış 0, son satır `selftest: N checks, 0 failed`, N ≥ 14 (§5.4'teki 17 durumun hepsi adlandırılmış kontrol).
- [ ] K2: `python3 tools/skill-script-gen.py bots/config/skill_priest_k.spec --out /tmp/skill_priest_k.txt` (gerçek `MAGIC`, `--magic` verilmeden, sqlcmd) çıkış 0 ve `diff /tmp/skill_priest_k.txt bots/config/skill_priest_k.txt` boş (yani commit'lenen betik aracın çıktısıdır).
- [ ] K3: `python3 tools/skill-script-gen.py --check bots/config/skill_priest_k.txt` çıkış 0; basılan satır `ok: N steps, ...` biçiminde, `N ≤ 100`, son ofset ≤ 600000. Betikte 6 `raw` hazırlık adımı (`pinvite`/`paccept`), 21 `cast` satırı (`BotPHD_K` 9 + `BotPHB_K` 12), 2 `pot` satırı ve 1 `list` vardır (`grep -c`'lerle gösterilir, toplam 30 adım).
- [ ] K4: `grep -c '^[0-9]* cast BotPHD_K' bots/config/skill_priest_k.txt` = 9, `... BotPHB_K` = 12; `grep -c 112703 bots/config/skill_priest_k.txt` = 0 (düşman hedefli skill yok).
- [ ] K5: `./tools/build.sh Release` hatasız biter (C++ değişmedi; yeni uyarı yok) ve `./tools/run-tests.sh` mevcut sayıyla (`251 tests, 0 failed` ya da fazlası) geçer.
- [ ] K6: `git diff --stat gece/2026-10-02...bot/F4-42` yalnızca `tools/skill-script-gen.py`, `bots/config/skill_priest_k.spec`, `bots/config/skill_priest_k.txt` ve plan dosyasını gösterir (`GameServer/`, `BotCore/`, `Tests/`, `tools/skill-check.py`, `docs/` farkı boş).
- [ ] K7 (Claude, çalışma zamanı): `./tools/bot-refill.sh apply --mp-pots 20` (sunucular kapalıyken), sunucular açık, `[BOT] ENABLED=1 MAX_BOTS=16 TELEMETRY=decisions`, `BotPHD_K,BotPHB_K,BotWP_K,BotWG_K` spawn, betik `Scripts/skill_priest_k.txt` olarak kopyalanıp `/bot script run skill_priest_k` ile koşulur: betik hatasız yüklenir (`Bot_*.log`'da `bad verb`/`offset` hatası yok), tüm adımlar zamanında çalışır (`max_late` makul), `tools/skill-check.py <jsonl> --min-n 2` raporu üretilir; her skill için `started`/sonuç dağılımı, `mp_delta` ve recast hükmü kaydedilir; reddedilen/başarısız her skill bir bulgu olarak listelenir (düzeltme planı Claude'dadır). Sonuçlar `docs/05` §9'a işlenir. Sunucular `stop` ile kapatılır, `GameServer.ini` yedekten geri döner.
- Derleme sonucu (`tools/build.sh Release` son satırları) Uygulayıcı Raporu'na yapıştırılır.

## 7. Doğrulama komutları

```bash
python3 tools/skill-script-gen.py --selftest
python3 tools/skill-script-gen.py bots/config/skill_priest_k.spec --out /tmp/skill_priest_k.txt
diff /tmp/skill_priest_k.txt bots/config/skill_priest_k.txt
python3 tools/skill-script-gen.py --check bots/config/skill_priest_k.txt
./tools/build.sh Release
./tools/run-tests.sh
git diff --stat gece/2026-10-02...bot/F4-42
```

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §Kod kuralları. `tools/skill-script-gen.py` ASCII/UTF-8, LF, yürütülebilir bit gerekmez, yalnızca standart kütüphane. Spec ve üretilmiş betik ASCII, LF (`ParseScript` `\r`'yi de kabul eder, ama araç LF üretir). Kod yorumları ve değişken adları İngilizce.
- Kişisel veri: yalnızca `MAGIC` okunur; başka tablo sorgulanmaz. Bot adları (`BotPHD_K`...) sistem karakterleridir, kişisel veri değildir.
- Araç yalnızca dosya üretir; sunucuya bağlanmaz. `Scripts/` klasörüne (depo dışı) **yazma**; kopyalama Claude'un koşusunda yapılır.
- Zamanlama modeli `[A]`'dır (uçuş aşaması, sunucu gecikmesi, MP yenilenmesi hesaba girmez; `margin_ms` payı bunun içindir). Koşuda bir adım ofsetinde bot hâlâ önceki cast'teyse `BeginCast` reddeder: bu bir **betik zamanlama bulgusudur**, araç `--margin-ms` ile ayarlanır (Claude karar verir); DeepSeek kendi başına `margin_ms` varsayılanını değiştirmez.
- MP bütçesi `[A]`: priest'in azami MP'si ölçülmedi; `pot` adımları `389220000` (Potion of Ancient Spirit, +2160 MP; `db/004` yuva 21) stokunun `--mp-pots` ile doldurulmasını gerektirir. Stok yoksa `pot` `no_stock` ile reddedilir; bu da bulgudur.
 - Uçan skill ve düşman hedefli skill bu planda yok (Kapsam dışı); spec'e ekleme.

---

## Uygulayıcı Raporu

### Tur 1

- **Durum:** UYGULANDI. Yalnızca §4'teki dört dosyaya dokunuldu; C++/derleme girdisi değişmedi.
- **Branch / commit'ler:** `bot/F4-42` (taban `gece/2026-10-02`).
  - `f4e4528` `[F4-42] skill-script-gen.py üreteci, priest dost hedefli spec ve üretilmiş betik`
  - (bu rapor ve `Durum` satırı: sonraki commit)
- **Değişen dosyalar:**
  - `tools/skill-script-gen.py` (yeni, 647 satır): spec + `MAGIC` → `ParseScript` sınırlarına uyan betik; `--check` (ParseScript kurallarının birebir yansıması) ve `--selftest` (23 adlandırılmış kontrol); `skill-check.py`'deki `--magic` ayrıştırıcısı ve `load_magic_sql` (bayt olarak çöz) kopyalandı.
  - `bots/config/skill_priest_k.spec` (yeni): §5.2 listesi (6 `raw`, 9+12 `cast`, 2 `pot`).
  - `bots/config/skill_priest_k.txt` (yeni, üretilmiş): 32 satır, 30 adım, son ofset 115500 ms.
  - `plans/F4-42-...md`: `Durum` + bu rapor.
- **Kabul kriterleri öz-değerlendirmesi:**
  - **K1 ✔** `python3 tools/skill-script-gen.py --selftest` → `selftest: 23 checks, 0 failed`, çıkış 0. §5.4'teki 17 durumun tamamı adlandırılmış kontrol olarak kapsanır (1 `cursor_sequential`; 2 `bots_parallel`; 3–5 `period_*`; 6 `pot_advance`; 7 `raw_sets_t0` (+`raw_absent_t0`); 8 `sorted_stable`; 9 `trailing_list`; 10 `unknown_skill`; 11 `bad_verb_raw`; 12 `bad_cycles`; 13 `step_limit`+`step_limit_no_file`; 14 `offset_limit`; 15 `deterministic`; 16 `check_ok`+`check_bad_offset_order`+`check_too_many_lines`+`check_line_too_long`+`check_bad_verb`; 17 `sqlcmd_non_utf8`).
  - **K2 ✔** Gerçek `MAGIC` ile (`--magic` verilmeden, sqlcmd) üretim `/tmp`'ye yazıldı; `diff /tmp/skill_priest_k.txt bots/config/skill_priest_k.txt` boş.
  - **K3 ✔** `--check` → `ok: 30 steps, 1109 bytes, 32 lines, last offset 115500 ms`; 6 `raw`, 21 `cast`, 2 `pot`, 1 `list` = 30 adım, `N=30 ≤ 100`, son ofset ≤ 600000.
  - **K4 ✔** `grep -c '^[0-9]* cast BotPHD_K'` = 9, `... BotPHB_K` = 12, `grep -c 112703` = 0.
  - **K5 ✔** `./tools/build.sh Release` hatasız bitti; yeni uyarı yok (yalnızca önceden var olan `UpgradeHandler.cpp(634/862)` C4789). `./tools/run-tests.sh` → `251 tests, 0 failed`.
  - **K6 ✔** `git diff --name-only gece/2026-10-02...bot/F4-42` yalnızca dört dosyayı listeler (`GameServer/`, `BotCore/`, `Tests/`, `tools/skill-check.py`, `docs/` farkı yok).
  - **K7 → Claude'da** (çalışma zamanı; bu planda DeepSeek sunucu açmaz).
- **Derleme çıktısının son satırları:**
  ```
    BotCore.vcxproj -> ...\build\bin\x86-Release\libs\BotCore.lib
    proj-AIServer.vcxproj -> ...\build\bin\x86-Release\Server\AIServer.exe
    proj-GameServer.vcxproj -> ...\build\bin\x86-Release\Server\GameServer.exe
  ```
  (Tek uyarı: `UpgradeHandler.cpp(634): warning C4789`, `(862)` — bu plan öncesinden var.)
- **Plandan sapmalar / notlar:**
  - `# bots:` başlığı, `cast`/`pot` satırlarındaki **yürütücü** botları ilk görünme sırasıyla yazar (`BotPHD_K BotPHB_K`); hedef botlar (`BotWP_K`, `BotWG_K`) başlığa girmez. Plan bu satırın içeriğini tanımlamıyor ve hiçbir kabul kriteri denetlemiyor; işlevsel etkisi yok (yorum satırı, sunucu yok sayar).
  - `--check` `kScriptMaxBytes` karşılaştırması dosya baytı üzerinden, satır uzunluğu denetimi ise çözülen metin üzerinden yapılır (betikler ASCII olduğu için C++ `std::string` bayt sayımıyla aynı sonucu verir).
  - Repo `core.autocrlf=true` olduğundan Git, `bots/config/skill_priest_k.{spec,txt}` için "LF will be replaced by CRLF the next time Git touches it" uyarısı verir. Bu çalışma ağacında dosyalar LF'tir ve K2 `diff`'i boş kalır; taze bir checkout'ta Git `.txt`'i CRLF'e çevirebilir (`.txt` için `.gitattributes` kuralı yok ve `.gitattributes` bu planın dokunulabilir dosyaları arasında değil). Doğrulama bu çalışma ağacında yapıldığı sürece K2/K3 geçerlidir.
- **Açık sorular:** Yok. K7 (gerçek koşu, `docs/05` §9 işlemesi) planda Claude'a bırakılmıştır.

