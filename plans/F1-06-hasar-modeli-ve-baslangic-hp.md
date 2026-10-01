# F1-06: Fiziksel hasar/istatistik hesaplayıcısı (`tools/stat-model.py`) ve bot başlangıç HP/MP düzeltmesi

| Alan | Değer |
|---|---|
| Durum | HAZIR |
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

### Tur 1

- Durum: —
- Branch / commit'ler: —
- Değişen dosyalar ve neden: —
- Kabul kriterleri öz-değerlendirme: —
- Plandan sapmalar ve gerekçeleri: —
- Açık sorular: —

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(henüz yok)
