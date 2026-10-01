# F1-06: Fiziksel hasar/istatistik hesaplayıcısı (`tools/stat-model.py`) ve bot başlangıç HP/MP düzeltmesi

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F1 — Veri ve mekanik doğrulama (`docs/17` §2) |
| Branch | `bot/F1-06` (taban: `main`) |
| Bağımlı olduğu planlar | F1-04, F1-05 (KAPANDI) |
| İlgili gereksinim / kabul | T-MECH-DMG-01 (model kısmı), T-DATA-03 (veri girdisi), T-DATA-01; MEC-DMG-01/02/05/06, MEC-CHR-* |
| Tahmini büyüklük | M (1 yeni araç betiği; `db/002` içinde iki sayı; README notu) |
| Hazırlayan / tarih | Claude / 2026-10-02 |

---

## 1. Amaç

İki iş:

1. **Bot başlangıç HP/MP düzeltmesi.** Sunucu girişte `SetMaxHp()`/`SetMaxMp()` ile yalnızca **aşağı kırpar**, hiçbir zaman yukarı çıkarmaz (`GameServer/User.cpp:1041-1078`, `:1081-1112`). F1-04 betiği bota ekipmansız maks değeri (ör. W-P 4458) yazıyor; ekipman bonusu sonradan maksı artırdığı için bot **tam canla başlamaz**. Çözüm: `db/002_bot_characters.sql` içinde `Hp` ve `Mp` değerlerini `32000` yap; giriş kırpması maksa indirir (tam can/mana).
2. **Hesaplayıcı.** Sunucu formüllerini birebir uygulayıp 12 botun türetilmiş istatistiklerini (maks HP/MP, toplam saldırı, toplam AC, isabet/kaçınma) ve **fiziksel hasar** beklentisini (R vuruşu ve warrior Type1 skill'leri) hesaplayan bir araç. Amaç, `docs/04` §4'teki elle hesaplanmış değerleri doğrulamak ve T-MECH-DMG-01'in "model ± %15" karşılaştırması için modeli hazırlamaktır. Çalışma zamanı ölçümü bu planda yok.

Büyü hasarı (mage/priest, `MagicInstance.cpp:2558-2717`) bu planın **kapsamı dışıdır** (F1-07).

## 2. Bağlam (okunması zorunlu)

- `docs/03` §7 (MEC-DMG-01..07), `docs/04` §3.4, §4 (türetilmiş değerler tablosu), `docs/03` §13.
- `tools/bot-gear-report.py` (F1-05): DB'den bot ve `ITEM` okuma, `strItem` çözme, `sqlcmd` çağırma kalıbı **aynen yeniden kullanılır** (kopyala-uyarla; ortak modüle taşıma yok).
- `db/002_bot_characters.sql`, `db/README.md`.
- Sunucu kodu (Claude'un 2026-10-02'de okuduğu yerler; satırlar kaymışsa gerçek satırı raporla):
  - `GameServer/User.cpp:2087-2330` `CUser::SetUserAbility`: silah katsayısı `COEFFICIENT` tablosundan (`ShortSword, Sword, Axe, Club, Spear, Pole, Staff, Bow`, silah türü `m_bKind/10` ile `:2102-2128`); `sItemDamage` = sağ el `Damage` (+`m_bAddWeaponDamage`; dayanıklılık 0 ise /2), sol el (kalkan vb.) `Damage/2` (`:2130-2160`); `sItemDamage < 3 → 3` (`:2175`). `temp_str = GetStat(STR)` (**temel stat**), `baseAP = temp_str − 150` (temp_str > 150 ise), `temp_str == 160` ise `baseAP−−`; sonra `temp_str += GetStatBonusTotal(STR)` (`:2179-2182`); warrior/priest: `ap_stat = temp_str`, `additionalAP = 3 + baseAP`. `m_sTotalHit` (warrior/priest) = `(uint16)(0.010 × sItemDamage × (ap_stat+40) + hitcoefficient × sItemDamage × Level × ap_stat)`, sonra `(m_sTotalHit + additionalAP) × (100 + m_byAPBonusAmount) / 100` (`:2194-2198`); mage: `0.005 × sItemDamage × (ap_stat+40) + hitcoefficient × sItemDamage × Level` (`:2205-2208`; mage'in `additionalAP = 3`, `ap_stat` kullanımı koddan doğrulanmalı).
  - `m_sTotalAc = (short)(COEFFICIENT.Ac × (Level + m_sItemAc))`, `× m_sACPercent/100` (`:2210-2214`); warrior pasif savunma bonusu `PRO_SKILL2` puanına göre (20/30/40/50/60, kalkan yoksa yarısı, `:2227-2262`); `STA > 100` ise `+= STA − 100` (**temel** STA, `:2282-2285`); `m_bAddWeaponDamage > 0` ise `++m_sTotalHit`.
  - `m_fTotalHitrate = (1 + COEFFICIENT.Hitrate × Level × temp_dex) × m_sItemHitrate/100 × (m_bHitRateAmount/100)`, `m_fTotalEvasionrate` benzeri (`:2216-2219`); `temp_dex = getStatTotal(DEX)` (**item bonuslu**).
  - `GameServer/User.cpp:1041-1112` `SetMaxHp`/`SetMaxMp`: `HP = (short)(COEFFICIENT.Hp × L² × STA + 0,1 × L × STA + STA/5 + m_sMaxHPAmount + m_sItemMaxHp + 20)`, burada `STA = getStatTotal(STA)` (**item bonuslu**); üst sınır `MAX_PLAYER_HP = 14000` (`GameServer/Define.h:22`); `MP`: `COEFFICIENT.Mp != 0` ise `Mp × L² × (INT+30) + 0,2 × L × (INT+30)` (kodda `0.1 × L × 2 × INT'`) `+ INT'/5 + m_sMaxMPAmount + m_sItemMaxMp + 20`, `INT' = getStatTotal(INT) + 30`; değilse `Sp` katsayısıyla STA'dan. Maks, geçerli değerden küçükse geçerli değer maksa indirilir (`:1073-1076`, `:1108-1111`).
  - `GameServer/Unit.cpp:208-394` `CUser::GetDamage` (R vuruşu ve skill): `temp_ap = m_sTotalHit × m_bAttackAmount` (varsayılan 100); `temp_hit_B = (temp_ap × 200 / 100) / (temp_ac + 240)` (tamsayı); R: `damage = (short)(0.85 × temp_hit_B + 0.3 × rand(0..temp_hit_B))`; Type1 skill: `temp_hit = (int32)(temp_hit_B × sHit/100)`, `damage = (short)(temp_hit + 0.3 × rand(0..temp_hit) + 0.99)`; isabet: R için `GetHitRate(m_fTotalHitrate / hedef.m_fTotalEvasionrate)`, Type1 skill için `bHitType` 0 ise göreceli isabet `GetHitRate(oran × sHitRate/100)`, 1 ise sabit `sHitRate`%; ardından `GetMagicDamage` (item elemental bonusları), oyuncu hedefe `GetACDamage` (`Unit.cpp:676-715`: silah türüne göre hedefin `m_sSwordR/m_sAxeR/m_sMaceR/m_sSpearR/... / 200` kadar azaltma, sol+sağ el için ayrı ayrı) ve **`damage /= 2`** (`:373-387`, MEC-DMG-02), üst sınır `MAX_DAMAGE 32000`.
  - `GameServer/Unit.cpp:718-804` `Unit::GetHitRate`: oran ≥ 5 → %98 (başarı), ≥ 3 → %96, ≥ 2 → %94, ≥ 1,25 → %92, ≥ 0,8 → %90, ≥ 0,5 → %80, ≥ 0,33 → %70, ≥ 0,2 → %60, altı → %50 (MEC-DMG-05); sonuçların tam aralıklarını kodda doğrula.
  - `GameServer/User.cpp:1279-1420` `SetSlotItemValue`: ekipman (yuva 0–13) toplamları: `m_sItemAc` (dayanıklılık 0 ise /10), `m_sItemMaxHp`, `m_sItemMaxMp`, `m_sStatItemBonuses[STR..CHA]`, `m_sSwordR/m_sAxeR/m_sMaceR/m_sSpearR/m_sDaggerR/m_sBowR` (item `SwordAc, AxeAc, ...` sütunları, `:1359-1364` civarı), `m_sItemHitrate/Evasionrate` (item `Hitrate`/`Evasionrate`), elemental dirençler.
- DB sütunları (Claude'un doğruladığı): `COEFFICIENT(sClass, ShortSword, Sword, Axe, Club, Spear, Pole, Staff, Bow, Hp, Mp, Sp, Ac, Hitrate, Evasionrate)` (master sınıflar: 106, 110, 112; El Morad 206/210/212 satırlarını da kontrol et); `MAGIC_TYPE1(iNum, Name, Description, Type, HitRate, Hit, AddDamage, Delay, ComboType, ComboCount, ComboDamage, Range)` (kodda `bHitType` = tablodaki `Type` sütunu olabilir, **doğrula**); `ITEM`: `Damage, Delay, Range, Kind, Ac, FireDamage, IceDamage, LightningDamage, PoisonDamage, HPDrain, MPDamage, MPDrain, MirrorDamage, Hitrate, Evasionrate, DaggerAc, SwordAc, MaceAc, AxeAc, SpearAc, BowAc, StrB..ChaB, MaxHpB, MaxMpB, FireR, ColdR, LightningR, MagicR, PoisonR, CurseR`.
- **Veritabanı** (`AGENTS.md` §2.7): yerel DB'ye bağlanmak serbest. Bu plan yalnızca (a) `COEFFICIENT`, `ITEM`, `MAGIC`, `MAGIC_TYPE1`, bot `USERDATA` (`strUserID LIKE 'Bot%'`) satırlarını **okur**, (b) `db/002_bot_characters.sql` ile yalnızca 12 bot satırını yeniden yazar. Başka oyuncu satırlarını okuma. sqlcmd'de `-W` ile `-y` birlikte kullanılamaz.

## 3. Kapsam

**Yapılacaklar**

- `db/002_bot_characters.sql`: 12 profil satırındaki `hp` ve `mp` sütun değerlerini `32000` yap (sütun türü `int`, `USERDATA.Hp/Mp` `smallint`, 32000 ≤ 32767). Başlık yorumuna bir cümle ekle: `Hp/Mp are set high on purpose; the server clamps them to the real maximum at login (User.cpp SetMaxHp/SetMaxMp).` Betik başka hiçbir şeyi değişmez.
- `db/README.md`: 002 bölümüne tek cümlelik not ("Hp/Mp bilerek yüksek yazılır, giriş maks'a kırpar").
- `tools/stat-model.py` (yeni): türetilmiş istatistikler ve fiziksel hasar beklentisi; `--selftest`.
- Betiği yeniden uygula ve doğrula (`Upgrade=7`, botlar uygulanmış kalır).

**Kapsam dışı (yapılmayacak)**

- Büyü/heal hasarı (mage, priest), Type3/Type4 skill'ler, buff/debuff, kritik/özel pasifler (Boldness vb.) → F1-07.
- `GameServer/`, `docs/**`, `AGENTS.md`, `.gitattributes`, diğer `db/*.sql` dosyaları, kod değişikliği.
- Çalışma zamanı ölçümü (giriş testi, GM ile hasar ölçümü): proje sahibiyle ayrı oturum.
- Bot olmayan DB satırlarını okumak/değiştirmek.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `db/002_bot_characters.sql` | değiştir | yalnızca 12 satırda `hp`/`mp` değerleri + başlık yorumu (ASCII, LF) |
| `db/README.md` | değiştir | 002 bölümüne bir cümle |
| `tools/stat-model.py` | yeni | ASCII, LF, yalnızca standart kütüphane |

`tools/*` düzenlemesi opencode'da `ask` ister. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** raporda soru olarak yaz.

## 5. Uygulama adımları

1. **Önce oku:** §2'deki kod satırlarını depoda aç ve doğrula; kaymışsa gerçek satırı raporla. `GameDefine.h:176-182` `SkillPointCategory` (PRO_SKILL2'nin hangi ağaca denk geldiğini doğrula; `strSkill` bayt 6 = ağaç 6 olmalı) ve `GetMagicDamage` (`Unit.cpp:591-675`) mantığını da oku. Nesnelerin ekipman toplamları için `SetSlotItemValue`'nun `:1345-1420` bölümünü oku.
2. **`db/002` düzeltmesi:** 12 satırda `hp`, `mp` sütunlarına `32000`, `32000`. Betiği uygula (`-v Upgrade=7`): 12 satır sonuç; ardından DB'den okuyup rapora yapıştır: `SELECT strUserID, Hp, Mp FROM USERDATA WHERE strUserID LIKE 'Bot%' ORDER BY strUserID` (hepsi 32000). Bot olmayan satır sayıları önce/sonra aynı olmalı (`USERDATA` 6, `ACCOUNT_CHAR` 4, `WAREHOUSE` 4). Botlar uygulanmış kalır.
3. **`tools/stat-model.py`** — komut satırı `python3 tools/stat-model.py [--sqlcmd PATH] [--server ".\SQLEXPRESS"] [--db FDP_kn_online]` ve `--selftest`. Veri erişimi `tools/bot-gear-report.py`'deki kalıpla (alt süreç `SQLCMD.EXE`, `-W -s "|" -h -1 -b`). Yalnızca ASCII çıktı. Sorgular: bot satırları (F1-05'teki `BOT_QUERY`, `Bot%`), bu botların `ITEM` satırları, `COEFFICIENT` (botların sınıfları), warrior skill'leri için `MAGIC` + `MAGIC_TYPE1` (aşağıda).
   - **Bölüm S (türetilmiş istatistikler):** her bot için iki değer kümesi: **item'sız** (docs/04 §4 ile karşılaştırma) ve **item'lı** (gerçek giriş değeri). Satır: `S <bot> items=<yes|no> max_hp=.. max_mp=.. total_hit=.. total_ac=.. hitrate=%.3f evasion=%.3f`. Item'sız kümede item bonusu (`StrB..`, `MaxHpB/MaxMpB`, `Ac`, `Hitrate/Evasionrate`) yok ve `sItemDamage` yalnızca silahın `Damage`'ı (ekipman olarak kalır). **Pasif skill bonusları dahil:** warrior pasif savunma (`PRO_SKILL2` = `strSkill` bayt 6 puanına göre, kalkan yoksa yarısı) uygulanır; master warrior/priest "Boldness" (HP < %30) uygulanmaz; buff yok.
   - **Bölüm R (R vuruşu):** her saldırgan bot × her savunmacı **profil** (WP, WG, PHD, PHB, MF, MI; Karus ve El Morad stat ve ekipmanı aynı olduğundan her biri yalnızca bir kez, saldırgan olarak da yalnızca Karus botları, 6 × 6 = 36 çift): `R <atk>-><def> hit_prob=%.3f base=<temp_hit_B> dmg_avg=%.1f dmg_min=<m> dmg_max=<M>`. Hesap: `temp_ac` = savunmacının item'lı `total_ac` (`m_sACAmount = 0`, `m_sACPercent = 100` varsayılan), `temp_ap = total_hit × 100`, `temp_hit_B = ((temp_ap × 200 / 100) // (temp_ac + 240))` (tamsayı); hasar = `int(0.85 × B + 0.3 × r)`, `r` ∈ [0, B] tamsayı **tüm değerler üzerinde** ortalaması, en düşük ve en yüksek (r = 0 ve r = B); `GetMagicDamage` ve `GetACDamage` adımlarını **uygula**: (a) `GetMagicDamage`'ın item elemental/drain etkileri bu referans setlerde 0 ise (`FireDamage` vb. 0) etkisiz olduğunu rapora yaz, değilse hesaba kat; (b) `GetACDamage`: saldırganın her silah yuvası (sağ ve sol el) için silah türüne göre `damage -= damage × hedef.R / 200` (tamsayı bölme, sıra: sol sonra sağ el); (c) `/2` (oyuncu hedef). `hit_prob`: `rate = atk.hitrate / def.evasion` için `GetHitRate` başarı olasılığı (kodun gerçek aralıklarına göre). Not: R satırlarında gösterilen `dmg_avg` isabet **olduğu** durumdaki ortalamadır; beklenen vuruş başına hasar `hit_prob × dmg_avg` ayrıca `exp=` alanı olarak yazılır.
   - **Bölüm K (warrior Type1 skill'leri):** WP ve WG için: yalnızca master warrior ağaç 5'teki (saldırı) Type1 skill'leri (`MAGIC.MagicNum / 1000 = 106`, `MagicNum` basamak 4 = 5, `Type1 = 1`, bayt 5 puanı ≥ `SkillLevel` olanlar; WP için 70, WG için 60); her biri için `K <atk>-><def> skill=<MagicNum> <ad> hit_pct=.. sHit=.. base=<temp_hit_B> dmg_avg=..` (skill hasarı: `temp_hit = int(B × sHit/100)`, `damage = int(temp_hit + 0.3 × r + 0.99)`, `r` ∈ [0, temp_hit] ortalaması; isabet: `Type` (bHitType) 0 ise `GetHitRate(rate × sHitRate/100)` başarı olasılığı, 1 ise `sHitRate/100`; ardından aynı `GetACDamage` ve `/2`). Savunmacı: 6 profil.
   - **Bölüm P (karşılaştırma):** item'sız W-P için docs/04 §4 değerleri (Maks HP ~4458, Maks MP ~4438, Raptor +7 saldırı ~1766) ile hesaplananı yan yana: `P WP max_hp calc=<x> doc=4458 diff=<d>`, `max_mp ... doc=4438`, `total_hit ... doc=1766`; aynısı WG (Graham +7: saldırı doc=1036), PHD/PHB (Maks HP 2636, Maks MP 5696), MF (896 / 5286), MI (1582 / 5286). Fark varsa nedenini (ör. docs/04 yuvarlaması, silah dayanıklılığı, sol el) raporda açıkla; farkın büyük olması başarısızlık değil, **bulgudur**.
   - Çıktı sabit sırada (`== S ==`, `== R ==`, `== K ==`, `== P ==`); aynı DB ile her çalıştırmada aynı (rastgele sayı yok; `r` üzerindeki toplam kesin ortalamadır).
   - **`--selftest`** (DB'siz, sentetik girdilerle): (a) W-P item'lı: `Damage=175`, `COEFFICIENT.Spear/Pole = 0.00032`, `Level=80`, temel STR 255, item STR bonusu 29, `m_byAPBonusAmount=0` → `total_hit = 1947` (hesap: `ap_stat = 284`, `(uint16)(0.010×175×324 + 0.00032×175×80×284) = 1839`, `additionalAP = 3 + 105 = 108`, toplam 1947); (b) aynı bot, item bonusuz (STR 255) → `total_hit = 1766`; (c) W-P item'sız `max_hp`: `STA=162`, `COEFFICIENT.Hp = 0.003`, `Level = 80`, `m_sItemMaxHp = 0` → `4458`; (d) `temp_hit_B`: `total_hit = 1947`, savunmacı `temp_ac = 1488` → `((1947×100)×200//100)//(1488+240) = 225`; (e) `GetHitRate` eşik değerleri: oran 5.0 → 0.98, oran 0.1 → 0.50 vb. (kodun gerçek olasılıklarını okuyarak); (f) R hasarı: `B = 225` için min `191` (`int(0.85×225)`), max `int(0.85×225 + 0.3×225) = 258`; ortalama `r` üzerinden hesaplanan değer. Beklenen sayılar Claude'un elle hesabıdır; **seninkiler farklı çıkarsa durup raporla** (kodu yeniden oku, bulgun doğru olabilir).
4. **Çalıştır:** `python3 tools/stat-model.py` çıktısını **kırpmadan** rapora yapıştır (S: 24 satır, R: 36 satır, K: skill sayısı × 12, P: 14 satır civarı).
5. Raporu yaz; commit mesajı `[F1-06] ...`; yalnızca üç dosya eklenir.

## 6. Kabul kriterleri

- [ ] K1: `python3 tools/stat-model.py --selftest` → `selftest OK`, çıkış 0 (yukarıdaki a–f assert'leri).
- [ ] K2: `db/002_bot_characters.sql` farkı yalnızca 12 satırda `hp/mp = 32000` ve başlık yorumu (`git diff` rapora); betik tekrar uygulandı (`Upgrade=7`), 12 satır sonuç; `SELECT ... Hp, Mp FROM USERDATA WHERE strUserID LIKE 'Bot%'` hepsi 32000; bot olmayan satır sayıları önce/sonra 6/4/4; `db/002_bot_characters_rollback.sql` değişmedi.
- [ ] K3: Araç çıktısı S, R, K, P bölümleriyle eksiksiz (S 24 satır; R 36 satır; K en az WP ve WG skill'leri; P karşılaştırma satırları) ve çıkış 0.
- [ ] K4: Her formül adımının hangi kod satırından alındığı raporda tablo olarak (adım → `dosya:satır`), özellikle `PRO_SKILL2` eşlemesi, `GetHitRate` aralıkları, `GetACDamage` ve `GetMagicDamage` (item etkisi 0 mı?).
- [ ] K5: `P` bölümündeki farklar (calc vs doc) açıklanmış; açıklama yoksa kriter karşılanmış sayılmaz.
- [ ] K6: Betik yalnızca bot satırlarını okuyor: `grep -n "USERDATA" tools/stat-model.py` tek sorgu, `LIKE 'Bot%'`.
- [ ] K7: Kapsam: `git diff --stat main...bot/F1-06` yalnızca `db/002_bot_characters.sql`, `db/README.md`, `tools/stat-model.py` ve plan dosyası; `git status --short` boş; `file tools/stat-model.py db/002_bot_characters.sql` ASCII.

## 7. Doğrulama komutları

```bash
python3 tools/stat-model.py --selftest
python3 tools/stat-model.py | head -80
git diff main...bot/F1-06 -- db/002_bot_characters.sql
grep -n "USERDATA" tools/stat-model.py
file tools/stat-model.py db/002_bot_characters.sql
git diff --stat main...bot/F1-06
git status --short
```

Claude doğrulaması: araç yeniden çalıştırılır; W-P ve M-F için elle hesap; formül satırları açılır; `db/002` yeniden uygulanıp `Hp/Mp` kontrolü ve rollback testi.

## 8. Kısıtlar ve uyarılar

- **Hesabı tam sayı semantiğiyle yap.** C++'ta `(uint16)` dönüşümleri, `int` bölmeleri ve `short` kesmeleri sonucu değiştirir; Python'da `//`, `int()` (sıfıra doğru) ve açık `& 0xFFFF` kullan. `float` ara değerleri C++ `float` (32 bit) iken Python `float` 64 bittir; fark ±1 hasar oluşturabilir, bunu rapora yaz (önemli bir fark çıkarsa `struct` ile 32 bit float yuvarlaması dene).
- Modelde **buff yok**, `m_bAttackAmount = 100`, `m_bPlayerAttackAmount = 100`, `m_sACAmount = 0`, `m_sACPercent = 100`, `m_bHitRateAmount = 100`, `m_sAvoidRateAmount = 100` varsayılan değerlerdir (`Unit.cpp:56-61`, `User.cpp:75`).
- Bu araç ölçmez, **modeller**; sonuçlar çalışma zamanında doğrulanana kadar `[I]`/`[D]` etiketlidir.
- `db/002` değişikliği yalnızca iki sayıdır; başka bir davranış değişikliği yapma. Bot dışı satırlara yazma.
- Yeni karar gerekirse **durup** raporda sor.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1 — 2026-10-02

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F1-06` (taban: `main` @ `6869bf0`)
  - `9edfb02` — `[F1-06] Stat/hasar modeli araci ve bot baslangic HP/MP` (`tools/stat-model.py`, `db/002_bot_characters.sql`, `db/README.md`)
  - Bu rapor ve `Durum: UYGULANDI` ayrı commit'lenir.
- Değişen dosyalar ve nedenleri:
  - `db/002_bot_characters.sql`: 12 satırda `hp`/`mp` = `32000`; başlık yorumuna kırpma notu. Başka değişiklik yok.
  - `db/README.md`: 002 bölümüne tek cümle (`Hp`/`Mp` bilerek yüksek).
  - `tools/stat-model.py` (yeni, ASCII + LF): S/R/K/P bölümleri + `--selftest`.
  - `plans/F1-06-hasar-modeli-ve-baslangic-hp.md`: yalnızca `Durum` satırı ve bu rapor.
- Veritabanına yalnızca `db/002` ile 12 bot satırı yazıldı; botlar **uygulanmış bırakıldı**.

**Adım 1 — kod satırı doğrulaması (plan §2 ile uyumlu, kayma yok; ek okumalar)**
```
ItemEquipAvailable/SetUserAbility/...: bkz. K4 tablosu
GameDefine.h:172  #define PRO_SKILL2 0x06        (strSkill bayt 6 = ağaç 6)
GameDefine.h:176-183 SkillPointCategory       (Free=0, Cat1=5, Cat2=6, Cat3=7, Master=8)
GameDefine.h:363  GetItemGroup() = m_bKind/10 (silah türü eşlemesi)
GameDefine.h:365-376 isDagger/isSword/isAxe/isMace/isSpear/isShield/isStaff/isBow
User.cpp:4607     CheckSkillPoint             m_bstrSkill[skillnum] >= min && <= max
MagicType1Set.h:18-19  MAGIC_TYPE1 column 2 = Type -> bHitType; column 4 = Hit -> sHit
COEFFICIENT (DB) 106=206, 110=210, 112=212 birebir aynı; Spear 0.00032, Club 0.00025,
                 Staff 0.00015, Hitrate 0.05 (warrior) / 0.015, Evasionrate 0.018 / 0.015
```
Nesnelerin ekipman toplamları (`SetSlotItemValue` `:1302-1364`) ve `GetMagicDamage` (`Unit.cpp:591-675`) okundu; referans setlerde **tüm elemental/drain/mirror sütunları 0** (F1-06 çalıştırması kontrol eder) → `GetMagicDamage` etkisizdir. `GetHitRate` tam aralıkları (`Unit.cpp:718-801`) ve `myrand` kapsayıcı aralığı (`shared/globals.cpp:16-22`) okundu.

**Adım 2 — `db/002` düzeltmesi ve uygulama (K2)**
```
$ git diff main...bot/F1-06 -- db/002_bot_characters.sql
@@ başlık yorumu @@
+-- Hp/Mp are set high on purpose; the server clamps them to the real maximum
+-- at login (User.cpp SetMaxHp/SetMaxMp), so the bots start at full HP/MP.
@@ 12 satır @@
-    ('WP', 'BotWP_K',  ..., 4458, 4438, 0x...),
+    ('WP', 'BotWP_K',  ..., 32000, 32000, 0x...),
     ... (12 satırın tamamı; yalnızca hp/mp değişti)

$ (once) SELECT ud_nonbot, ac_nonbot, wh_nonbot
6 4 4
$ "$SQLCMD" ... -b -W -s '|' -v Upgrade=7 -i db/002_bot_characters.sql   (12 satırlık sonuç)
BotMF_E|BotAcc_MF_E|2|12|210|80|577|142|71|1000|12|7
BotMF_K|BotAcc_MF_K|1|3|110|80|577|142|71|1000|12|7
BotMI_E|BotAcc_MI_E|2|12|210|80|577|142|71|1000|12|7
BotMI_K|BotAcc_MI_K|1|3|110|80|577|142|71|1000|12|7
BotPHB_E|BotAcc_PHB_E|2|12|212|80|577|142|71|1000|13|6
BotPHB_K|BotAcc_PHB_K|1|4|112|80|577|142|71|1000|13|6
BotPHD_E|BotAcc_PHD_E|2|12|212|80|577|142|71|1000|13|6
BotPHD_K|BotAcc_PHD_K|1|4|112|80|577|142|71|1000|13|6
BotWG_E|BotAcc_WG_E|2|11|206|80|577|142|71|1000|13|6
BotWG_K|BotAcc_WG_K|1|1|106|80|577|142|71|1000|13|6
BotWP_E|BotAcc_WP_E|2|11|206|80|577|142|71|1000|12|6
BotWP_K|BotAcc_WP_K|1|1|106|80|577|142|71|1000|12|6
apply_exit=0

$ SELECT RTRIM(strUserID), Hp, Mp FROM USERDATA WHERE strUserID LIKE 'Bot%' ORDER BY strUserID
BotMF_E|32000|32000
BotMF_K|32000|32000
BotMI_E|32000|32000
BotMI_K|32000|32000
BotPHB_E|32000|32000
BotPHB_K|32000|32000
BotPHD_E|32000|32000
BotPHD_K|32000|32000
BotWG_E|32000|32000
BotWG_K|32000|32000
BotWP_E|32000|32000
BotWP_K|32000|32000

$ (sonra) SELECT ud_nonbot, ac_nonbot, wh_nonbot
6 4 4
```
`db/002_bot_characters_rollback.sql` değişmedi (`git diff` boş).

**K1 — `--selftest` (a–f iddiaları)**
```
$ python3 tools/stat-model.py --selftest
selftest OK
selftest_exit=0
```
(a) W-P item'lı `total_hit=1947` (core `1839` + `additionalAP 108`), (b) item'sız `1766`, (c) item'sız `max_hp=4458`, (d) `B=225` (`1947×2×100//(1488+240)`), (e) `GetHitRate` eşikleri (5.0→0.98 … 0.2→0.60, <0.2→0.50), (f) R hasarı `min=191`, `max=258`, `r=0..225` ortalaması `224.5`.

**Adım 3/4 — model çıktısı (tam, kırpılmadı; `python3 tools/stat-model.py`, çıkış 0; 252 satır: S 24, R 36, K 174, P 14 + 4 başlık)**
```
== S ==
S BotMF_E items=no max_hp=896 max_mp=5286 total_hit=54 total_ac=80 hitrate=73.000 evasion=73.000
S BotMF_E items=yes max_hp=1541 max_mp=6021 total_hit=57 total_ac=605 hitrate=73.000 evasion=73.000
S BotMF_K items=no max_hp=896 max_mp=5286 total_hit=54 total_ac=80 hitrate=73.000 evasion=73.000
S BotMF_K items=yes max_hp=1541 max_mp=6021 total_hit=57 total_ac=605 hitrate=73.000 evasion=73.000
S BotMI_E items=no max_hp=1581 max_mp=5286 total_hit=54 total_ac=87 hitrate=73.000 evasion=73.000
S BotMI_E items=yes max_hp=2228 max_mp=6021 total_hit=57 total_ac=612 hitrate=73.000 evasion=73.000
S BotMI_K items=no max_hp=1581 max_mp=5286 total_hit=54 total_ac=87 hitrate=73.000 evasion=73.000
S BotMI_K items=yes max_hp=2228 max_mp=6021 total_hit=57 total_ac=612 hitrate=73.000 evasion=73.000
S BotPHB_E items=no max_hp=2636 max_mp=5696 total_hit=403 total_ac=127 hitrate=85.000 evasion=85.000
S BotPHB_E items=yes max_hp=3491 max_mp=6392 total_hit=418 total_ac=923 hitrate=101.800 evasion=101.800
S BotPHB_K items=no max_hp=2636 max_mp=5696 total_hit=403 total_ac=127 hitrate=85.000 evasion=85.000
S BotPHB_K items=yes max_hp=3491 max_mp=6392 total_hit=418 total_ac=923 hitrate=101.800 evasion=101.800
S BotPHD_E items=no max_hp=2636 max_mp=5696 total_hit=403 total_ac=127 hitrate=85.000 evasion=85.000
S BotPHD_E items=yes max_hp=3491 max_mp=6392 total_hit=418 total_ac=923 hitrate=101.800 evasion=101.800
S BotPHD_K items=no max_hp=2636 max_mp=5696 total_hit=403 total_ac=127 hitrate=85.000 evasion=85.000
S BotPHD_K items=yes max_hp=3491 max_mp=6392 total_hit=418 total_ac=923 hitrate=101.800 evasion=101.800
S BotWG_E items=no max_hp=4458 max_mp=4438 total_hit=1036 total_ac=182 hitrate=241.000 evasion=87.400
S BotWG_E items=yes max_hp=5650 max_mp=5370 total_hit=1138 total_ac=1488 hitrate=241.000 evasion=87.400
S BotWG_K items=no max_hp=4458 max_mp=4438 total_hit=1036 total_ac=182 hitrate=241.000 evasion=87.400
S BotWG_K items=yes max_hp=5650 max_mp=5370 total_hit=1138 total_ac=1488 hitrate=241.000 evasion=87.400
S BotWP_E items=no max_hp=4458 max_mp=4438 total_hit=1766 total_ac=142 hitrate=241.000 evasion=87.400
S BotWP_E items=yes max_hp=5650 max_mp=5370 total_hit=1947 total_ac=857 hitrate=241.000 evasion=87.400
S BotWP_K items=no max_hp=4458 max_mp=4438 total_hit=1766 total_ac=142 hitrate=241.000 evasion=87.400
S BotWP_K items=yes max_hp=5650 max_mp=5370 total_hit=1947 total_ac=857 hitrate=241.000 evasion=87.400
== R ==
R BotMF_K->BotMF_K hit_prob=0.900 base=13 dmg_avg=6.0 dmg_min=5 dmg_max=7 exp=5.4
R BotMF_K->BotMI_K hit_prob=0.900 base=13 dmg_avg=6.0 dmg_min=5 dmg_max=7 exp=5.4
R BotMF_K->BotPHB_K hit_prob=0.800 base=9 dmg_avg=4.0 dmg_min=3 dmg_max=5 exp=3.2
R BotMF_K->BotPHD_K hit_prob=0.800 base=9 dmg_avg=4.0 dmg_min=3 dmg_max=5 exp=3.2
R BotMF_K->BotWG_K hit_prob=0.900 base=6 dmg_avg=2.6 dmg_min=2 dmg_max=3 exp=2.3
R BotMF_K->BotWP_K hit_prob=0.900 base=10 dmg_avg=4.5 dmg_min=4 dmg_max=5 exp=4.1
R BotMI_K->BotMF_K hit_prob=0.900 base=13 dmg_avg=6.0 dmg_min=5 dmg_max=7 exp=5.4
R BotMI_K->BotMI_K hit_prob=0.900 base=13 dmg_avg=6.0 dmg_min=5 dmg_max=7 exp=5.4
R BotMI_K->BotPHB_K hit_prob=0.800 base=9 dmg_avg=4.0 dmg_min=3 dmg_max=5 exp=3.2
R BotMI_K->BotPHD_K hit_prob=0.800 base=9 dmg_avg=4.0 dmg_min=3 dmg_max=5 exp=3.2
R BotMI_K->BotWG_K hit_prob=0.900 base=6 dmg_avg=2.6 dmg_min=2 dmg_max=3 exp=2.3
R BotMI_K->BotWP_K hit_prob=0.900 base=10 dmg_avg=4.5 dmg_min=4 dmg_max=5 exp=4.1
R BotPHB_K->BotMF_K hit_prob=0.920 base=98 dmg_avg=43.8 dmg_min=37 dmg_max=50 exp=40.3
R BotPHB_K->BotMI_K hit_prob=0.920 base=98 dmg_avg=43.8 dmg_min=37 dmg_max=50 exp=40.3
R BotPHB_K->BotPHB_K hit_prob=0.900 base=71 dmg_avg=22.8 dmg_min=19 dmg_max=26 exp=20.5
R BotPHB_K->BotPHD_K hit_prob=0.900 base=71 dmg_avg=22.8 dmg_min=19 dmg_max=26 exp=20.5
R BotPHB_K->BotWG_K hit_prob=0.900 base=48 dmg_avg=15.6 dmg_min=13 dmg_max=18 exp=14.0
R BotPHB_K->BotWP_K hit_prob=0.900 base=76 dmg_avg=34.0 dmg_min=29 dmg_max=39 exp=30.6
R BotPHD_K->BotMF_K hit_prob=0.920 base=98 dmg_avg=43.8 dmg_min=37 dmg_max=50 exp=40.3
R BotPHD_K->BotMI_K hit_prob=0.920 base=98 dmg_avg=43.8 dmg_min=37 dmg_max=50 exp=40.3
R BotPHD_K->BotPHB_K hit_prob=0.900 base=71 dmg_avg=22.8 dmg_min=19 dmg_max=26 exp=20.5
R BotPHD_K->BotPHD_K hit_prob=0.900 base=71 dmg_avg=22.8 dmg_min=19 dmg_max=26 exp=20.5
R BotPHD_K->BotWG_K hit_prob=0.900 base=48 dmg_avg=15.6 dmg_min=13 dmg_max=18 exp=14.0
R BotPHD_K->BotWP_K hit_prob=0.900 base=76 dmg_avg=34.0 dmg_min=29 dmg_max=39 exp=30.6
R BotWG_K->BotMF_K hit_prob=0.960 base=269 dmg_avg=120.8 dmg_min=103 dmg_max=139 exp=116.0
R BotWG_K->BotMI_K hit_prob=0.960 base=267 dmg_avg=119.9 dmg_min=102 dmg_max=138 exp=115.1
R BotWG_K->BotPHB_K hit_prob=0.940 base=195 dmg_avg=63.2 dmg_min=54 dmg_max=73 exp=59.4
R BotWG_K->BotPHD_K hit_prob=0.940 base=195 dmg_avg=63.2 dmg_min=54 dmg_max=73 exp=59.4
R BotWG_K->BotWG_K hit_prob=0.940 base=131 dmg_avg=42.4 dmg_min=36 dmg_max=49 exp=39.9
R BotWG_K->BotWP_K hit_prob=0.940 base=207 dmg_avg=92.9 dmg_min=79 dmg_max=107 exp=87.3
R BotWP_K->BotMF_K hit_prob=0.960 base=460 dmg_avg=206.8 dmg_min=176 dmg_max=238 exp=198.5
R BotWP_K->BotMI_K hit_prob=0.960 base=457 dmg_avg=205.4 dmg_min=175 dmg_max=236 exp=197.2
R BotWP_K->BotPHB_K hit_prob=0.940 base=334 dmg_avg=108.4 dmg_min=92 dmg_max=125 exp=101.9
R BotWP_K->BotPHD_K hit_prob=0.940 base=334 dmg_avg=108.4 dmg_min=92 dmg_max=125 exp=101.9
R BotWP_K->BotWG_K hit_prob=0.940 base=225 dmg_avg=73.0 dmg_min=62 dmg_max=84 exp=68.6
R BotWP_K->BotWP_K hit_prob=0.940 base=354 dmg_avg=159.1 dmg_min=135 dmg_max=183 exp=149.5
== K ==
K BotWG_K->BotMF_K skill=106500 Hash hit_pct=0.9600 sHit=100 base=269 dmg_avg=139.4
K BotWG_K->BotMI_K skill=106500 Hash hit_pct=0.9600 sHit=100 base=267 dmg_avg=138.4
K BotWG_K->BotPHB_K skill=106500 Hash hit_pct=0.9400 sHit=100 base=195 dmg_avg=73.0
K BotWG_K->BotPHD_K skill=106500 Hash hit_pct=0.9400 sHit=100 base=195 dmg_avg=73.0
K BotWG_K->BotWG_K skill=106500 Hash hit_pct=0.9400 sHit=100 base=131 dmg_avg=49.1
K BotWG_K->BotWP_K skill=106500 Hash hit_pct=0.9400 sHit=100 base=207 dmg_avg=107.3
K BotWG_K->BotMF_K skill=106505 hoodwink hit_pct=0.9600 sHit=150 base=269 dmg_avg=208.7
K BotWG_K->BotMI_K skill=106505 hoodwink hit_pct=0.9600 sHit=150 base=267 dmg_avg=207.2
K BotWG_K->BotPHB_K skill=106505 hoodwink hit_pct=0.9400 sHit=150 base=195 dmg_avg=109.3
K BotWG_K->BotPHD_K skill=106505 hoodwink hit_pct=0.9400 sHit=150 base=195 dmg_avg=109.3
K BotWG_K->BotWG_K skill=106505 hoodwink hit_pct=0.9400 sHit=150 base=131 dmg_avg=73.4
K BotWG_K->BotWP_K skill=106505 hoodwink hit_pct=0.9400 sHit=150 base=207 dmg_avg=160.6
K BotWG_K->BotMF_K skill=106510 Shear hit_pct=0.9600 sHit=100 base=269 dmg_avg=139.4
K BotWG_K->BotMI_K skill=106510 Shear hit_pct=0.9600 sHit=100 base=267 dmg_avg=138.4
K BotWG_K->BotPHB_K skill=106510 Shear hit_pct=0.9400 sHit=100 base=195 dmg_avg=73.0
K BotWG_K->BotPHD_K skill=106510 Shear hit_pct=0.9400 sHit=100 base=195 dmg_avg=73.0
K BotWG_K->BotWG_K skill=106510 Shear hit_pct=0.9400 sHit=100 base=131 dmg_avg=49.1
K BotWG_K->BotWP_K skill=106510 Shear hit_pct=0.9400 sHit=100 base=207 dmg_avg=107.3
K BotWG_K->BotMF_K skill=106515 pierce hit_pct=0.9901 sHit=100 base=269 dmg_avg=139.4
K BotWG_K->BotMI_K skill=106515 pierce hit_pct=0.9901 sHit=100 base=267 dmg_avg=138.4
K BotWG_K->BotPHB_K skill=106515 pierce hit_pct=0.9901 sHit=100 base=195 dmg_avg=73.0
K BotWG_K->BotPHD_K skill=106515 pierce hit_pct=0.9901 sHit=100 base=195 dmg_avg=73.0
K BotWG_K->BotWG_K skill=106515 pierce hit_pct=0.9901 sHit=100 base=131 dmg_avg=49.1
K BotWG_K->BotWP_K skill=106515 pierce hit_pct=0.9901 sHit=100 base=207 dmg_avg=107.3
K BotWG_K->BotMF_K skill=106520 leg cutting hit_pct=0.9600 sHit=100 base=269 dmg_avg=139.4
K BotWG_K->BotMI_K skill=106520 leg cutting hit_pct=0.9600 sHit=100 base=267 dmg_avg=138.4
K BotWG_K->BotPHB_K skill=106520 leg cutting hit_pct=0.9400 sHit=100 base=195 dmg_avg=73.0
K BotWG_K->BotPHD_K skill=106520 leg cutting hit_pct=0.9400 sHit=100 base=195 dmg_avg=73.0
K BotWG_K->BotWG_K skill=106520 leg cutting hit_pct=0.9400 sHit=100 base=131 dmg_avg=49.1
K BotWG_K->BotWP_K skill=106520 leg cutting hit_pct=0.9400 sHit=100 base=207 dmg_avg=107.3
K BotWG_K->BotMF_K skill=106525 Carving hit_pct=0.9600 sHit=200 base=269 dmg_avg=278.6
K BotWG_K->BotMI_K skill=106525 Carving hit_pct=0.9600 sHit=200 base=267 dmg_avg=276.5
K BotWG_K->BotPHB_K skill=106525 Carving hit_pct=0.9400 sHit=200 base=195 dmg_avg=145.9
K BotWG_K->BotPHD_K skill=106525 Carving hit_pct=0.9400 sHit=200 base=195 dmg_avg=145.9
K BotWG_K->BotWG_K skill=106525 Carving hit_pct=0.9400 sHit=200 base=131 dmg_avg=98.1
K BotWG_K->BotWP_K skill=106525 Carving hit_pct=0.9400 sHit=200 base=207 dmg_avg=214.4
K BotWG_K->BotMF_K skill=106530 Sever hit_pct=0.9600 sHit=100 base=269 dmg_avg=139.4
K BotWG_K->BotMI_K skill=106530 Sever hit_pct=0.9600 sHit=100 base=267 dmg_avg=138.4
K BotWG_K->BotPHB_K skill=106530 Sever hit_pct=0.9400 sHit=100 base=195 dmg_avg=73.0
K BotWG_K->BotPHD_K skill=106530 Sever hit_pct=0.9400 sHit=100 base=195 dmg_avg=73.0
K BotWG_K->BotWG_K skill=106530 Sever hit_pct=0.9400 sHit=100 base=131 dmg_avg=49.1
K BotWG_K->BotWP_K skill=106530 Sever hit_pct=0.9400 sHit=100 base=207 dmg_avg=107.3
K BotWG_K->BotMF_K skill=106535 prick hit_pct=0.9901 sHit=150 base=269 dmg_avg=208.7
K BotWG_K->BotMI_K skill=106535 prick hit_pct=0.9901 sHit=150 base=267 dmg_avg=207.2
K BotWG_K->BotPHB_K skill=106535 prick hit_pct=0.9901 sHit=150 base=195 dmg_avg=109.3
K BotWG_K->BotPHD_K skill=106535 prick hit_pct=0.9901 sHit=150 base=195 dmg_avg=109.3
K BotWG_K->BotWG_K skill=106535 prick hit_pct=0.9901 sHit=150 base=131 dmg_avg=73.4
K BotWG_K->BotWP_K skill=106535 prick hit_pct=0.9901 sHit=150 base=207 dmg_avg=160.6
K BotWG_K->BotMF_K skill=106540 multiple shock hit_pct=0.9600 sHit=150 base=269 dmg_avg=208.7
K BotWG_K->BotMI_K skill=106540 multiple shock hit_pct=0.9600 sHit=150 base=267 dmg_avg=207.2
K BotWG_K->BotPHB_K skill=106540 multiple shock hit_pct=0.9400 sHit=150 base=195 dmg_avg=109.3
K BotWG_K->BotPHD_K skill=106540 multiple shock hit_pct=0.9400 sHit=150 base=195 dmg_avg=109.3
K BotWG_K->BotWG_K skill=106540 multiple shock hit_pct=0.9400 sHit=150 base=131 dmg_avg=73.4
K BotWG_K->BotWP_K skill=106540 multiple shock hit_pct=0.9400 sHit=150 base=207 dmg_avg=160.6
K BotWG_K->BotMF_K skill=106545 Cleave hit_pct=0.9600 sHit=150 base=269 dmg_avg=208.7
K BotWG_K->BotMI_K skill=106545 Cleave hit_pct=0.9600 sHit=150 base=267 dmg_avg=207.2
K BotWG_K->BotPHB_K skill=106545 Cleave hit_pct=0.9400 sHit=150 base=195 dmg_avg=109.3
K BotWG_K->BotPHD_K skill=106545 Cleave hit_pct=0.9400 sHit=150 base=195 dmg_avg=109.3
K BotWG_K->BotWG_K skill=106545 Cleave hit_pct=0.9400 sHit=150 base=131 dmg_avg=73.4
K BotWG_K->BotWP_K skill=106545 Cleave hit_pct=0.9400 sHit=150 base=207 dmg_avg=160.6
K BotWG_K->BotMF_K skill=106550 mangling hit_pct=0.9600 sHit=100 base=269 dmg_avg=139.4
K BotWG_K->BotMI_K skill=106550 mangling hit_pct=0.9600 sHit=100 base=267 dmg_avg=138.4
K BotWG_K->BotPHB_K skill=106550 mangling hit_pct=0.9400 sHit=100 base=195 dmg_avg=73.0
K BotWG_K->BotPHD_K skill=106550 mangling hit_pct=0.9400 sHit=100 base=195 dmg_avg=73.0
K BotWG_K->BotWG_K skill=106550 mangling hit_pct=0.9400 sHit=100 base=131 dmg_avg=49.1
K BotWG_K->BotWP_K skill=106550 mangling hit_pct=0.9400 sHit=100 base=207 dmg_avg=107.3
K BotWG_K->BotMF_K skill=106555 thrust hit_pct=0.9901 sHit=100 base=269 dmg_avg=139.4
K BotWG_K->BotMI_K skill=106555 thrust hit_pct=0.9901 sHit=100 base=267 dmg_avg=138.4
K BotWG_K->BotPHB_K skill=106555 thrust hit_pct=0.9901 sHit=100 base=195 dmg_avg=73.0
K BotWG_K->BotPHD_K skill=106555 thrust hit_pct=0.9901 sHit=100 base=195 dmg_avg=73.0
K BotWG_K->BotWG_K skill=106555 thrust hit_pct=0.9901 sHit=100 base=131 dmg_avg=49.1
K BotWG_K->BotWP_K skill=106555 thrust hit_pct=0.9901 sHit=100 base=207 dmg_avg=107.3
K BotWG_K->BotMF_K skill=106557 sword aura hit_pct=0.9901 sHit=100 base=269 dmg_avg=139.4
K BotWG_K->BotMI_K skill=106557 sword aura hit_pct=0.9901 sHit=100 base=267 dmg_avg=138.4
K BotWG_K->BotPHB_K skill=106557 sword aura hit_pct=0.9901 sHit=100 base=195 dmg_avg=73.0
K BotWG_K->BotPHD_K skill=106557 sword aura hit_pct=0.9901 sHit=100 base=195 dmg_avg=73.0
K BotWG_K->BotWG_K skill=106557 sword aura hit_pct=0.9901 sHit=100 base=131 dmg_avg=49.1
K BotWG_K->BotWP_K skill=106557 sword aura hit_pct=0.9901 sHit=100 base=207 dmg_avg=107.3
K BotWG_K->BotMF_K skill=106560 sword dancing hit_pct=0.9901 sHit=150 base=269 dmg_avg=208.7
K BotWG_K->BotMI_K skill=106560 sword dancing hit_pct=0.9901 sHit=150 base=267 dmg_avg=207.2
K BotWG_K->BotPHB_K skill=106560 sword dancing hit_pct=0.9901 sHit=150 base=195 dmg_avg=109.3
K BotWG_K->BotPHD_K skill=106560 sword dancing hit_pct=0.9901 sHit=150 base=195 dmg_avg=109.3
K BotWG_K->BotWG_K skill=106560 sword dancing hit_pct=0.9901 sHit=150 base=131 dmg_avg=73.4
K BotWG_K->BotWP_K skill=106560 sword dancing hit_pct=0.9901 sHit=150 base=207 dmg_avg=160.6
K BotWP_K->BotMF_K skill=106500 Hash hit_pct=0.9600 sHit=100 base=460 dmg_avg=238.2
K BotWP_K->BotMI_K skill=106500 Hash hit_pct=0.9600 sHit=100 base=457 dmg_avg=236.7
K BotWP_K->BotPHB_K skill=106500 Hash hit_pct=0.9400 sHit=100 base=334 dmg_avg=124.9
K BotWP_K->BotPHD_K skill=106500 Hash hit_pct=0.9400 sHit=100 base=334 dmg_avg=124.9
K BotWP_K->BotWG_K skill=106500 Hash hit_pct=0.9400 sHit=100 base=225 dmg_avg=84.2
K BotWP_K->BotWP_K skill=106500 Hash hit_pct=0.9400 sHit=100 base=354 dmg_avg=183.4
K BotWP_K->BotMF_K skill=106505 hoodwink hit_pct=0.9600 sHit=150 base=460 dmg_avg=357.3
K BotWP_K->BotMI_K skill=106505 hoodwink hit_pct=0.9600 sHit=150 base=457 dmg_avg=354.7
K BotWP_K->BotPHB_K skill=106505 hoodwink hit_pct=0.9400 sHit=150 base=334 dmg_avg=187.4
K BotWP_K->BotPHD_K skill=106505 hoodwink hit_pct=0.9400 sHit=150 base=334 dmg_avg=187.4
K BotWP_K->BotWG_K skill=106505 hoodwink hit_pct=0.9400 sHit=150 base=225 dmg_avg=126.1
K BotWP_K->BotWP_K skill=106505 hoodwink hit_pct=0.9400 sHit=150 base=354 dmg_avg=275.0
K BotWP_K->BotMF_K skill=106510 Shear hit_pct=0.9600 sHit=100 base=460 dmg_avg=238.2
K BotWP_K->BotMI_K skill=106510 Shear hit_pct=0.9600 sHit=100 base=457 dmg_avg=236.7
K BotWP_K->BotPHB_K skill=106510 Shear hit_pct=0.9400 sHit=100 base=334 dmg_avg=124.9
K BotWP_K->BotPHD_K skill=106510 Shear hit_pct=0.9400 sHit=100 base=334 dmg_avg=124.9
K BotWP_K->BotWG_K skill=106510 Shear hit_pct=0.9400 sHit=100 base=225 dmg_avg=84.2
K BotWP_K->BotWP_K skill=106510 Shear hit_pct=0.9400 sHit=100 base=354 dmg_avg=183.4
K BotWP_K->BotMF_K skill=106515 pierce hit_pct=0.9901 sHit=100 base=460 dmg_avg=238.2
K BotWP_K->BotMI_K skill=106515 pierce hit_pct=0.9901 sHit=100 base=457 dmg_avg=236.7
K BotWP_K->BotPHB_K skill=106515 pierce hit_pct=0.9901 sHit=100 base=334 dmg_avg=124.9
K BotWP_K->BotPHD_K skill=106515 pierce hit_pct=0.9901 sHit=100 base=334 dmg_avg=124.9
K BotWP_K->BotWG_K skill=106515 pierce hit_pct=0.9901 sHit=100 base=225 dmg_avg=84.2
K BotWP_K->BotWP_K skill=106515 pierce hit_pct=0.9901 sHit=100 base=354 dmg_avg=183.4
K BotWP_K->BotMF_K skill=106520 leg cutting hit_pct=0.9600 sHit=100 base=460 dmg_avg=238.2
K BotWP_K->BotMI_K skill=106520 leg cutting hit_pct=0.9600 sHit=100 base=457 dmg_avg=236.7
K BotWP_K->BotPHB_K skill=106520 leg cutting hit_pct=0.9400 sHit=100 base=334 dmg_avg=124.9
K BotWP_K->BotPHD_K skill=106520 leg cutting hit_pct=0.9400 sHit=100 base=334 dmg_avg=124.9
K BotWP_K->BotWG_K skill=106520 leg cutting hit_pct=0.9400 sHit=100 base=225 dmg_avg=84.2
K BotWP_K->BotWP_K skill=106520 leg cutting hit_pct=0.9400 sHit=100 base=354 dmg_avg=183.4
K BotWP_K->BotMF_K skill=106525 Carving hit_pct=0.9600 sHit=200 base=460 dmg_avg=476.3
K BotWP_K->BotMI_K skill=106525 Carving hit_pct=0.9600 sHit=200 base=457 dmg_avg=473.2
K BotWP_K->BotPHB_K skill=106525 Carving hit_pct=0.9400 sHit=200 base=334 dmg_avg=249.8
K BotWP_K->BotPHD_K skill=106525 Carving hit_pct=0.9400 sHit=200 base=334 dmg_avg=249.8
K BotWP_K->BotWG_K skill=106525 Carving hit_pct=0.9400 sHit=200 base=225 dmg_avg=168.3
K BotWP_K->BotWP_K skill=106525 Carving hit_pct=0.9400 sHit=200 base=354 dmg_avg=366.6
K BotWP_K->BotMF_K skill=106530 Sever hit_pct=0.9600 sHit=100 base=460 dmg_avg=238.2
K BotWP_K->BotMI_K skill=106530 Sever hit_pct=0.9600 sHit=100 base=457 dmg_avg=236.7
K BotWP_K->BotPHB_K skill=106530 Sever hit_pct=0.9400 sHit=100 base=334 dmg_avg=124.9
K BotWP_K->BotPHD_K skill=106530 Sever hit_pct=0.9400 sHit=100 base=334 dmg_avg=124.9
K BotWP_K->BotWG_K skill=106530 Sever hit_pct=0.9400 sHit=100 base=225 dmg_avg=84.2
K BotWP_K->BotWP_K skill=106530 Sever hit_pct=0.9400 sHit=100 base=354 dmg_avg=183.4
K BotWP_K->BotMF_K skill=106535 prick hit_pct=0.9901 sHit=150 base=460 dmg_avg=357.3
K BotWP_K->BotMI_K skill=106535 prick hit_pct=0.9901 sHit=150 base=457 dmg_avg=354.7
K BotWP_K->BotPHB_K skill=106535 prick hit_pct=0.9901 sHit=150 base=334 dmg_avg=187.4
K BotWP_K->BotPHD_K skill=106535 prick hit_pct=0.9901 sHit=150 base=334 dmg_avg=187.4
K BotWP_K->BotWG_K skill=106535 prick hit_pct=0.9901 sHit=150 base=225 dmg_avg=126.1
K BotWP_K->BotWP_K skill=106535 prick hit_pct=0.9901 sHit=150 base=354 dmg_avg=275.0
K BotWP_K->BotMF_K skill=106540 multiple shock hit_pct=0.9600 sHit=150 base=460 dmg_avg=357.3
K BotWP_K->BotMI_K skill=106540 multiple shock hit_pct=0.9600 sHit=150 base=457 dmg_avg=354.7
K BotWP_K->BotPHB_K skill=106540 multiple shock hit_pct=0.9400 sHit=150 base=334 dmg_avg=187.4
K BotWP_K->BotPHD_K skill=106540 multiple shock hit_pct=0.9400 sHit=150 base=334 dmg_avg=187.4
K BotWP_K->BotWG_K skill=106540 multiple shock hit_pct=0.9400 sHit=150 base=225 dmg_avg=126.1
K BotWP_K->BotWP_K skill=106540 multiple shock hit_pct=0.9400 sHit=150 base=354 dmg_avg=275.0
K BotWP_K->BotMF_K skill=106545 Cleave hit_pct=0.9600 sHit=150 base=460 dmg_avg=357.3
K BotWP_K->BotMI_K skill=106545 Cleave hit_pct=0.9600 sHit=150 base=457 dmg_avg=354.7
K BotWP_K->BotPHB_K skill=106545 Cleave hit_pct=0.9400 sHit=150 base=334 dmg_avg=187.4
K BotWP_K->BotPHD_K skill=106545 Cleave hit_pct=0.9400 sHit=150 base=334 dmg_avg=187.4
K BotWP_K->BotWG_K skill=106545 Cleave hit_pct=0.9400 sHit=150 base=225 dmg_avg=126.1
K BotWP_K->BotWP_K skill=106545 Cleave hit_pct=0.9400 sHit=150 base=354 dmg_avg=275.0
K BotWP_K->BotMF_K skill=106550 mangling hit_pct=0.9600 sHit=100 base=460 dmg_avg=238.2
K BotWP_K->BotMI_K skill=106550 mangling hit_pct=0.9600 sHit=100 base=457 dmg_avg=236.7
K BotWP_K->BotPHB_K skill=106550 mangling hit_pct=0.9400 sHit=100 base=334 dmg_avg=124.9
K BotWP_K->BotPHD_K skill=106550 mangling hit_pct=0.9400 sHit=100 base=334 dmg_avg=124.9
K BotWP_K->BotWG_K skill=106550 mangling hit_pct=0.9400 sHit=100 base=225 dmg_avg=84.2
K BotWP_K->BotWP_K skill=106550 mangling hit_pct=0.9400 sHit=100 base=354 dmg_avg=183.4
K BotWP_K->BotMF_K skill=106555 thrust hit_pct=0.9901 sHit=100 base=460 dmg_avg=238.2
K BotWP_K->BotMI_K skill=106555 thrust hit_pct=0.9901 sHit=100 base=457 dmg_avg=236.7
K BotWP_K->BotPHB_K skill=106555 thrust hit_pct=0.9901 sHit=100 base=334 dmg_avg=124.9
K BotWP_K->BotPHD_K skill=106555 thrust hit_pct=0.9901 sHit=100 base=334 dmg_avg=124.9
K BotWP_K->BotWG_K skill=106555 thrust hit_pct=0.9901 sHit=100 base=225 dmg_avg=84.2
K BotWP_K->BotWP_K skill=106555 thrust hit_pct=0.9901 sHit=100 base=354 dmg_avg=183.4
K BotWP_K->BotMF_K skill=106557 sword aura hit_pct=0.9901 sHit=100 base=460 dmg_avg=238.2
K BotWP_K->BotMI_K skill=106557 sword aura hit_pct=0.9901 sHit=100 base=457 dmg_avg=236.7
K BotWP_K->BotPHB_K skill=106557 sword aura hit_pct=0.9901 sHit=100 base=334 dmg_avg=124.9
K BotWP_K->BotPHD_K skill=106557 sword aura hit_pct=0.9901 sHit=100 base=334 dmg_avg=124.9
K BotWP_K->BotWG_K skill=106557 sword aura hit_pct=0.9901 sHit=100 base=225 dmg_avg=84.2
K BotWP_K->BotWP_K skill=106557 sword aura hit_pct=0.9901 sHit=100 base=354 dmg_avg=183.4
K BotWP_K->BotMF_K skill=106560 sword dancing hit_pct=0.9901 sHit=150 base=460 dmg_avg=357.3
K BotWP_K->BotMI_K skill=106560 sword dancing hit_pct=0.9901 sHit=150 base=457 dmg_avg=354.7
K BotWP_K->BotPHB_K skill=106560 sword dancing hit_pct=0.9400 sHit=150 base=334 dmg_avg=187.4
K BotWP_K->BotPHD_K skill=106560 sword dancing hit_pct=0.9400 sHit=150 base=334 dmg_avg=187.4
K BotWP_K->BotWG_K skill=106560 sword dancing hit_pct=0.9400 sHit=150 base=225 dmg_avg=126.1
K BotWP_K->BotWP_K skill=106560 sword dancing hit_pct=0.9901 sHit=150 base=354 dmg_avg=275.0
K BotWP_K->BotMF_K skill=106570 Howling Sword hit_pct=0.9901 sHit=200 base=460 dmg_avg=476.3
K BotWP_K->BotMI_K skill=106570 Howling Sword hit_pct=0.9901 sHit=200 base=457 dmg_avg=473.2
K BotWP_K->BotPHB_K skill=106570 Howling Sword hit_pct=0.9901 sHit=200 base=334 dmg_avg=249.8
K BotWP_K->BotPHD_K skill=106570 Howling Sword hit_pct=0.9901 sHit=200 base=334 dmg_avg=249.8
K BotWP_K->BotWG_K skill=106570 Howling Sword hit_pct=0.9901 sHit=200 base=225 dmg_avg=168.3
K BotWP_K->BotWP_K skill=106570 Howling Sword hit_pct=0.9901 sHit=200 base=354 dmg_avg=366.6
== P ==
P WP max_hp calc=4458 doc=4458 diff=0
P WP max_mp calc=4438 doc=4438 diff=0
P WP total_hit calc=1766 doc=1766 diff=0
P WG max_hp calc=4458 doc=4458 diff=0
P WG max_mp calc=4438 doc=4438 diff=0
P WG total_hit calc=1036 doc=1036 diff=0
P PHD max_hp calc=2636 doc=2636 diff=0
P PHD max_mp calc=5696 doc=5696 diff=0
P PHB max_hp calc=2636 doc=2636 diff=0
P PHB max_mp calc=5696 doc=5696 diff=0
P MF max_hp calc=896 doc=896 diff=0
P MF max_mp calc=5286 doc=5286 diff=0
P MI max_hp calc=1581 doc=1582 diff=-1
P MI max_mp calc=5286 doc=5286 diff=0
```

**K4 — formül adımı → kod satırı eşlemesi**

| Adım | Kod |
|---|---|
| Silah katsayısı (silah türü `m_bKind/10`) | `GameServer/User.cpp:2102-2128`; tür sabitleri `GameDefine.h:363-376` |
| `sItemDamage` (sağ el; sol el yay/kalkan; dayanıklılık 0 → /2) | `User.cpp:2130-2160` |
| `sItemDamage < 3 → 3` | `User.cpp:2175` |
| `baseAP = STR−150`; `STR==160 → baseAP--` | `User.cpp:2179-2182` |
| `ap_stat` (temel+item STR), `additionalAP = 3+baseAP` | `User.cpp:2185-2193` |
| Warrior/priest `m_sTotalHit` | `User.cpp:2194-2198` |
| Mage `m_sTotalHit` (ap_stat yalnızca `+40` teriminde; `+additionalAP` sonra) | `User.cpp:2205-2208` |
| `m_sTotalAc = AC×(L+m_sItemAc)`; `×AC%/100` | `User.cpp:2212-2216` |
| `m_fTotalHitrate` / `m_fTotalEvasionrate` (tem_dex item bonuslu) | `User.cpp:2218-2220` |
| Warrior pasif savunma `PRO_SKILL2` (bayt 6; kalkan yoksa yarı) | `User.cpp:2236-2268`; `GameDefine.h:172,176-184`; `CheckSkillPoint` `User.cpp:4607` |
| Boldness (**uygulanmadı**: HP tam; kod koşulu `m_sHp < %30`) | `User.cpp:2266-2273` (atlanan) |
| `STA>100 → AC += STA−100` (**temel** STA) | `User.cpp:2282-2285` |
| Ekipman toplamları (AC/Hp/Mp/stat/hit/evasion/direnç) | `User.cpp:1302-1364` |
| `getStatTotal` = temel+item+buff; `GetStatBonusTotal` = item+buff | `User.h:536`, `User.h:511` |
| `SetMaxHp` (kırpma `short`, cap 14000) | `User.cpp:1041-1076`; `Define.h:22` |
| `SetMaxMp` (MP yolu; warrior SP yolu; +20 yalnızca MP yolunda) | `User.cpp:1081-1111` |
| `temp_ap`, `temp_hit_B` (PvP çarpanları 100) | `Unit.cpp:229-267` |
| R hasarı `(short)(0.85f·B + 0.3f·r)` | `Unit.cpp:351-353` |
| Type1 `temp_hit = int(B·sHit/100)` ve hasar | `Unit.cpp:284-294`, `:333` |
| `GetHitRate` aralıkları (0.98 … 0.50) | `Unit.cpp:718-801` |
| `GetMagicDamage` (**etkisiz**: tüm item elemental/drain sütunları 0) | `Unit.cpp:591-675` |
| `GetACDamage` (sol→sağ; tür bazlı R/200; kalkan/staff kapsam dışı) | `Unit.cpp:676-715`; tür eşlemesi `GameDefine.h:365-376` |
| Oyuncu hedefte `/2` | `Unit.cpp:381-387` |
| `MAX_DAMAGE` tavanı | `Unit.cpp:389-391`; `Define.h:23` |
| `myrand(0,100)` kapsayıcı → `sHitRate/101` (bHitType≠0) | `globals.cpp:16-22`; `Unit.cpp:288-291` |
| `MAGIC_TYPE1.Type → bHitType`, `Hit → sHit` | `MagicType1Set.h:18-19` |
| Varsayılanlar (buff yok, `m_bAttackAmount=100`, …) | `Unit.cpp:56-61`, `User.cpp:70-78` |

**K5 — P bölümü farkları**
- 14 satırın 13'ünde fark **0**.
- `P MI max_hp calc=1581 doc=1582 diff=-1`: docs/04 §4 değeri **yuvarlanmış** (~1582); kod `(short)` ile **aşağı kırpar**: `0.001×80²×107 + 0.1×80×107 + 107/5 + 20 = 684,8 + 856 + 21 + 20 = 1581,8 → 1581`. Fark, modelden değil docs yuvarlamasından gelir. Başka açıklama gerekmez.

**Kabul kriterleri öz-değerlendirmesi**
- K1 ✔ `--selftest` → `selftest OK`, çıkış 0 (a–f iddiaları).
- K2 ✔ `db/002` farkı yalnızca 12 satırda `hp/mp=32000` + başlık yorumu; betik yeniden uygulandı (12 satır sonuç, exit 0); DB'de tüm botlar `Hp=Mp=32000`; bot olmayan sayılar önce/sonra `6/4/4`; rollback betiği değişmedi.
- K3 ✔ Çıktı S/R/K/P ile eksiksiz (S 24, R 36, K 174 = WP 15×6 + WG 14×6, P 14), çıkış 0.
- K4 ✔ Formül→satır tablosu yukarıda; `PRO_SKILL2` eşlemesi, `GetHitRate` aralıkları, `GetACDamage`, `GetMagicDamage` (etkisiz) açıklandı.
- K5 ✔ MI `max_hp` −1 farkı açıklandı (docs yuvarlaması vs `(short)` kırpması).
- K6 ✔ `grep -n "USERDATA" tools/stat-model.py` tek satır: `LIKE 'Bot%'`; başka `USERDATA` koşulu yok.
- K7 ✔ `git diff --stat main...bot/F1-06` yalnızca `db/002_bot_characters.sql`, `db/README.md`, `tools/stat-model.py` (+ bu plan dosyası rapor commit'iyle); `file` = ASCII; rapor commit'inden sonra `git status --short` boş.

**Plandan sapmalar ve gerekçeleri**
1. **`-0.1` farkı (P MI max_hp)** bir bulgudur, sapma değil; yukarıda açıklandı.
2. **K satırları 168 değil 174**: WG'nin ağaç-5 puanı 60 olduğu için `SkillLevel ≤ 60` olan 14 skill (106500…106560, 106557 dahil), WP için 15 skill (…106570) seçildi; 29×6 = 174. Plan "skill sayısı × 12" derken skill sayısını yaklaşık vermişti.
3. **K sırası bot adına göre alfabetik** (`BotWG_K` satırları `BotWP_K`'dan önce); çıktı sabit ve deterministik. İstenirse ayrı bir düzeltmede WP önce alınabilir.
4. **Boldness bilinçli olarak uygulanmadı:** kod koşulu HP < %30'a bağlı; girişte HP tam (plan da "uygulanmaz" diyor). `CheckExistEvent(51,2)` (seviye 70 quest'i) botlarda yok; 70+ puan dalında pasif savunma 50 alındı (`:2244-2249`) — WP zaten 0 puan, WG 62 → 55-69 dalı, fark etmez.
5. **`local DB` bağlantısı:** yalnızca plan izinli sorgular (`BOT_QUERY`, `ITEM`, `COEFFICIENT`, `MAGIC`+`MAGIC_TYPE1`) çalıştırıldı; başka oyuncu satırı okunmadı; yazma yalnızca `db/002` bot satırları.

**Açık sorular / bulgular**
1. **Item'lı gerçek giriş değerleri:** ör. WP `max_hp=5650`, `total_hit=1947`, `total_ac=857`; bunlar giriş kırpması ve ekipman etkisiyle oluşacak beklenen değerlerdir; çalışma zamanı doğrulaması (GM ile hasar/HP ölçümü, T-MECH-DMG-01 ± %15) ayrı oturumdadır.
2. **K (skill) ölçümü modelde yalnızca isabet olasılığı + deterministik ortalama** verir; gerçek oyunda sHit/addDamage/animasyon etkileşimi ve Type1 tip kapısı (saniyede 1) ayrıca F1-07/F4 konusudur.
3. Mage rakip profillerinin (MF/MI) fiziksel R hasarı ~6'dır (silahsız/asasız fiziksel formül); mage'ler için asıl beklenti büyü hasarıdır → F1-07.
4. Modeller `[D]`/`[I]` etiketli kalır; çalışma zamanı karşılaştırması yapılmadan `[V]`'ye yükseltilmez.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(henüz yok)
