# 03 — Sürüm Uyumluluğu ve Doğrulanmış Mekanikler

> Durum: Taslak v1.0 · Araştırma tarihi: 2026-10-01 · Referans commit: `0f52027`
> **Bu doküman sunucu mekaniklerinin tek kaynağıdır.** Davranış dokümanları (06–12) kuralları buradaki `MEC-*`, `CLI-*` ve `MB-*` kimlikleriyle referans verir. Skill bazlı sayısal veriler (ID, MP, süre, menzil) [05](05_SKILL_CATALOG_AND_COMBAT_RULES.md)'tedir; karakter bütçeleri [04](04_CHARACTER_BUILDS_STATS_AND_EQUIPMENT.md)'tedir.
> Etiketler: `[D]` depo kodu · `[V]` yerel sürüm verisi (DB/harita) · `[S]` bu sürüm/dönem için dış kaynak · `[B]` başka sürüm (ör. 1.298 resmi kaynak, modern KO) · `[Ö]` öneri · `[A]` açık/çalışma zamanı testi gerekli.

---

## 0. Bu dokümanı nasıl okumalı

- Her kural bir kimlik taşır. `MEC-*` sunucunun uyguladığı kurallardır. `CLI-*` sunucunun uygulamadığı ama gerçek istemcinin uyguladığı ve botun **adalet için** kendisinin uygulaması gereken sınırlardır. `MB-*` mekanik hatalardır.
- Bir kural `[D]` ise kod satırıyla doğrulanmıştır ama **çalışma zamanında gözlenmemiştir**. Çalışma zamanı doğrulaması F1 fazında yapılır ([17](17_IMPLEMENTATION_ROADMAP_AND_PHASE_GATES.md)); doğrulanan kurallar `[D+R: T-…]` olarak güncellenir ([21](21_PROJECT_TRACKING_TEMPLATES_AND_DOC_RULES.md) §5).
- Güncel Knight Online ile bu sürüm arasında ciddi farklar vardır (§1.3). Modern rehberlerdeki değerler **bu projede kullanılmaz**.

## 1. Sürüm kimliği

### 1.1 Bu depo hangi oyun dönemine karşılık geliyor?

| Bulgu | Kaynak | Etiket |
|---|---|---|
| Sunucu sürüm sabiti 1453 | [`shared/version.h:3`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/version.h#L3) | `[D]` |
| Maksimum seviye 80 | [`GameServer/Define.h:21`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/Define.h#L21) (`MAX_LEVEL 80`) | `[D]` |
| Master sınıflar (x06/x08/x10/x12) ve master ağacı (skill dizisi indeks 8) var | `GameServer/GameDefine.h:4-28,162-183` | `[D]` |
| Ronark Land = zone 71, Ardream = 72, Ronark Land Base = 73 (73 için ZONE_INFO satırı yok, yüklenmiyor) | [`GameServer/Define.h:113-150`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/Define.h#L113-L150); yerel ZONE_INFO | `[D]` `[V]` |
| USKO "Reign of the Fire Drake" genişlemesi Ağustos 2006'da çıktı: seviye sınırı 80, Bifrost, Ardream, Colony Zone'un adı Ronark Land oldu | Kalais haber arşivi; Wikipedia revizyonları; DonanımHaber 2007 ([19](19_SOURCES_AND_EVIDENCE.md) W-01..W-04) | `[S]` |
| İstemci 1453'ün yaklaşık Ekim 2006'ya ait olduğu iddiası (tek bir arama özeti; sayfa doğrulanamadı) | Softonic özeti | `[B]`/`[A]` |
| Topluluk bu dosyaları "1.453 Reign of the Fire Drake server files" olarak adlandırıyor | kocuce, kofans.cn (2010), pvpkenti | `[S]` |
| Forgotten Frontiers (Ekim 2008) seviye sınırını 83'e çıkardı; bu depo ondan önceki döneme aittir | Kalais arşivi | `[S]` |

**Sonuç:** Bu proje 2006 dönemi USKO RoFD mekaniklerine yakın bir sunucudur. Ancak sunucu kodu topluluk tarafından değiştirilmiş bir snoxd türevidir (bkz. §1.2). **Davranışın tek hakemi bu depodaki kod ve yerel veri tabanıdır.** Dış kaynaklar yalnızca kod/veri ile çelişmediğinde ve dönem uyumluysa kullanılır.

### 1.2 Kod soyu

- Depo, topluluğun paylaştığı "v1453 kaynak dosyaları" üzerine kuruludur (README). Kod yapısı snoxd/twostars "koserver" türevidir (`KOSocket`, `MagicInstance`, `Unit` sınıfları) `[D]`/`[S]`.
- Open-KO projesi resmi **1.298** sunucu/istemci kaynağını ve veri tabanını yayınlamıştır `[S]`. 1.298 değerleri bu projede `[B]` olarak işaretlenir; yerel değerlerle karşılaştırmalı kullanılır.
- `ko4life-net/ko` (MIT, 2022+) farklı bir kod tabanıdır ve 1.298 dönemini hedefler; karıştırılmamalıdır `[S]`.

### 1.3 Sık karıştırılan farklar

| Konu | Bu proje | Resmî 1.298 `[B]` | Modern KO `[B]` |
|---|---|---|---|
| Seviye sınırı | 80 `[D]` | 70/80 dönemine göre | 83+ |
| Stat puanı/seviye | 3 (≤60), 5 (>60) `[D]` | 3 | farklı |
| Respawn sonrası blink | Ronark'ta **yok** (rakip ulusa saldırılabilen zone) `[D]` | 10 sn | farklı |
| Sunucu R hızı kontrolü | Saniyede 1 başarılı R `[D]` | Yalnızca istemcinin bildirdiği gecikme | farklı |
| Party boyutu | 8 `[D]` | 8 | bazı modern kaynaklarda 6 |
| Ronark minimum seviye | 35 `[D]` | — | 55–70 (kaynağa göre) |
| Heal değerleri | DB'deki değerler ([05](05_SKILL_CATALOG_AND_COMBAT_RULES.md)) `[V]` | 120/240/480… | farklı |

## 2. Karakter mekaniği

### 2.1 Sınıf ve ırk kodları `[D]`

| Kod | Karus | El Morad |
|---|---|---|
| Warrior başlangıç / novice / master | 101 / 105 Berserker / **106 Guardian** | 201 / 205 Blade / **206 Protector** |
| Mage başlangıç / novice / master | 103 / 109 Sorcerer / **110 Necromancer** | 203 / 209 Mage / **210 Enchanter** |
| Priest başlangıç / novice / master | 104 / 111 Shaman / **112 Dark Priest** | 204 / 211 Cleric / **212 Druid** |

Kaynak: [`GameServer/GameDefine.h:4-28`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/GameDefine.h#L4-L28), `GameServer/User.h:44-58,393-417`.

- **MEC-CHR-01** Skill'ler sınıf koduna **birebir** bağlıdır: `MAGIC.Skill/10` karakterin `m_sClass` değerine eşit olmalıdır; örneğin 106 için ayrı satırlar vardır (`GameServer/MagicInstance.cpp:184-188,951-956`) `[D]`. Yerel DB'de her sınıf için ayrı skill satırları vardır `[V]`.
- **MEC-CHR-02** Başlangıç sınıfları (x01–x04) skill puanı dağıtamaz ([`GameServer/User.cpp:2971-3000`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L2971-L3000)) `[D]`.
- **MEC-CHR-03** Bu kurulumda oyun içi sınıf yükseltme akışı yok: quest betiklerinin hiçbiri `PromoteUser*` çağırmıyor; `LOAD_USER_DATA` prosedürü yalnızca novice → master yükseltmesini seviye > 59 ise girişte yapıyor `[V]`. Botlar master sınıf koduyla oluşturulmalıdır ([04](04_CHARACTER_BUILDS_STATS_AND_EQUIPMENT.md)).
- **MEC-CHR-04** Irk ve sınıf kombinasyonunu sunucu doğrulamaz; ekipman kuşanırken ırk ve sınıf kontrolü de yoktur, yalnızca seviye aralığı, rütbe, unvan ve **temel** stat gereksinimleri kontrol edilir ([`GameServer/ItemHandler.cpp:527-539`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/ItemHandler.cpp#L527-L539)) `[D]`. Botlar geçerli kombinasyonları kendileri seçmelidir ([04](04_CHARACTER_BUILDS_STATS_AND_EQUIPMENT.md) §2).
- **MEC-CHR-05** GM yetkisi (Authority 0) hasar almaz, HP sınırı yoktur, quest kapısını atlar (`GameServer/User.cpp:1058,1881-1882`, [`GameServer/MagicInstance.cpp:271`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L271)) `[D]`. **Botlar ve test rakipleri Authority 1 olmalıdır.**

### 2.2 Stat ve skill puanı `[D]`

- **MEC-CHR-06** Toplam stat: `T(L) = 300 + 3(L−1) + 2·max(0, L−60)`; level 80'de **577** ([`GameServer/User.cpp:1782-1792`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L1782-L1792)).
- **MEC-CHR-07** Tek stat üst sınırı 255 (`STAT_MAX`, [`shared/globals.h:352`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/globals.h#L352); [`GameServer/User.cpp:1838-1841`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L1838-L1841)).
- **MEC-CHR-08** Toplam skill puanı `2(L−9)`; level 80'de **142**. Ağaç başına sınır = seviye (80). Master ağacı sınırı `min(20, L−60)` = **20** (`GameServer/User.cpp:1790,2971-3000`).
- **MEC-CHR-09** Seviye atlarken stat puanı yalnızca hedef toplamın altında ise eklenir; eksik birikmiş puanı doldurmaz. Seviye 51'de oluşturulup doğal yoldan 80'e çıkan karakter **427** toplam stat ve **58** skill puanında kalabilir `[D]` + `[A]` (USERDATA varsayılanları doğrulanmalı). Stat sıfırlama tüm 14 kuşanma yuvası boşken çalışır ve toplamı düzeltir ([`GameServer/User.cpp:3908-4158`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L3908-L4158)).
- **MEC-CHR-10** Seviye 80 üstü karakter girişte koparılır ([`GameServer/CharacterSelectionHandler.cpp:197-201`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/CharacterSelectionHandler.cpp#L197-L201)) `[D]`.

### 2.3 Türetilmiş değerler `[D]`

Formüllerin tamamı ve kaynak satırları [04](04_CHARACTER_BUILDS_STATS_AND_EQUIPMENT.md) §3'tedir. Davranış tasarımını doğrudan etkileyenler:

- **MEC-CHR-11** Büyü hasarı yalnızca **temel MP (CHA)** statıyla ölçeklenir; item ile gelen CHA sayılmaz ([`GameServer/MagicInstance.cpp:2589-2595`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L2589-L2595)).
- **MEC-CHR-12** Heal skill'leri **sabit** değerlidir, stat ile ölçeklenmez ([`GameServer/MagicInstance.cpp:1341-1342`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L1341-L1342)).
- **MEC-CHR-13** INT > 100 ise her 2 INT puanı tüm dirençlere +1 verir ([`GameServer/User.cpp:2301-2303`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L2301-L2303)).
- **MEC-CHR-14** Temel STA (HP) > 100 ise AC'ye `(STA−100)` eklenir.
- **MEC-CHR-15** Maksimum HP 14000 ile sınırlıdır (`MAX_PLAYER_HP`, [`GameServer/Define.h:22`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/Define.h#L22)).
- **MEC-CHR-16** Master ağacında ≥ 10 puanı olan master sınıf karakter tüm hasarı ×0,85, 5–9 puanla ×0,90 alır ([`GameServer/User.cpp:1922-1930`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L1922-L1930)).

## 3. Normal saldırı (R)

### 3.1 Paket

`WIZ_ATTACK (0x08)`: `u8 bType, u8 bResult, i16 tid, i16 delaytime, i16 distance` ([`GameServer/AttackHandler.cpp:10`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/AttackHandler.cpp#L10)) `[D]`. Sonuç bölgeye `WIZ_ATTACK, bType, bResult, attackerID, tid` olarak yayınlanır.

### 3.2 Doğrulama sırası `[D]` ([`GameServer/AttackHandler.cpp:4-93`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/AttackHandler.cpp#L4-L93))

| Kimlik | Kontrol |
|---|---|
| MEC-R-01 | Saldıran ölü/kör/blink/Kaul değil (`isIncapacitated`) |
| MEC-R-02 | Saldıran güvenli alanda değil (Ronark'ta güvenli alan yok, §12) |
| MEC-R-03 | Saldıranın gizliliği (stealth) koşulsuz kaldırılır |
| MEC-R-04 | Sağ elde silah varsa ve saldıran mage değilse: **istemcinin bildirdiği** `delaytime ≥ silah.Delay + 10` ve `distance ≤ silah.Range`; aksi halde (boş el veya mage) `delaytime ≥ 100`. Değerler istemciden geldiği için gerçek bir zaman kontrolü değildir. |
| MEC-R-05 | Hedef var, 2D mesafe ≤ 15 + silah menzili ([`GameServer/User.cpp:5013-5076`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L5013-L5076)) |
| MEC-R-06 | Aynı zone, hedef canlı, hedef blink değil, düşman ([`GameServer/Unit.cpp:855-875`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/Unit.cpp#L855-L875)) |
| MEC-R-07 | **Saniyede en fazla bir başarılı R**: `UNIXTIME − son < 1.0` ise ret ([`GameServer/Unit.cpp:926-947`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/Unit.cpp#L926-L947)). `UNIXTIME` 1 sn çözünürlüklü olduğundan pratikte "farklı saniye" şartıdır. |
| MEC-R-08 | Hedefte `FREEZE` varsa sessiz ret |
| MEC-R-09 | Hasar > 0 ise uygulanır; silah ve 5 zırh parçası aşınır |

**Kontrol edilmeyenler `[D]`:** yön/açı, görüş hattı, saldırı hızı buff'ları (`m_sAttackSpeedAmount` sunucuda okunmuyor), skill ile R arasındaki zamanlama.

## 4. Skill (magic) hattı

### 4.1 Opcode'lar `[D]` ([`shared/packets.h:380-403`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/packets.h#L380-L403))

`MAGIC_CASTING 1`, `MAGIC_FLYING 2`, `MAGIC_EFFECTING 3`, `MAGIC_FAIL 4`, `MAGIC_DURATION_EXPIRED 5`, `MAGIC_CANCEL 6`, `MAGIC_TYPE4_EXTEND 8`. Fail kodları `sData[3]`: −100 başarısız, −103 etki yok, −104 ıska.

Paket: `u8 opcode, u32 skillID, i16 casterID, i16 targetID, i16 sData[7]` ([`GameServer/MagicProcess.cpp:16-53`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicProcess.cpp#L16-L53)). `casterID` gönderenin kendi kimliği olmalıdır. Alan skill'lerinde hedef noktası `sData[0]` (X) ve `sData[2]` (Z).

### 4.2 Kritik zamanlama kuralları

| Kimlik | Kural | Kaynak | Etiket |
|---|---|---|---|
| MEC-MAG-01 | **Cast süresi sunucuda uygulanmaz.** `bCastTime` GameServer'da okunmuyor; CASTING → EFFECTING sırası tutulmuyor. EFFECTING doğrudan gönderilebilir. | [`GameServer/MagicInstance.cpp:39-42`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L39-L42); grep | `[D]` |
| MEC-MAG-02 | Skill başına yeniden kullanım: `0 < (UNIXTIME−son)·1000 < ReCastTime·100` ise ret. `ReCastTime` 0,1 sn birimindedir. **Aynı saniye içindeki tekrar (fark 0) bu kontrolden geçer.** | [`GameServer/MagicInstance.cpp:360-370`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L360-L370) | `[D]` |
| MEC-MAG-03 | Tip kapısı: tip 1–7 skill'lerde, ID < 400000 ise aynı tipten saniyede bir kullanım (`0.7 sn` sabiti, 1 sn saat ile pratikte "farklı saniye"). | [`GameServer/MagicInstance.cpp:386-422`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L386-L422); [`GameServer/User.h:23`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.h#L23) | `[D]` |
| MEC-MAG-04 | Element niteliği 0 olan Type3 skill'ler (heal'ler, potlar) tip kapısını kendileri için siler; yalnızca kendi recast'lerine tabidir. | [`GameServer/MagicInstance.cpp:331-345`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L331-L345) | `[D]` |
| MEC-MAG-05 | ID ≥ 400000 skill'ler (tüm potlar 490xxx/500xxx) tip kapısına hiç girmez. MEC-MAG-02'deki aynı saniye açığıyla birlikte, **sunucu aynı saniyede aynı potun tekrarını reddetmez.** | `GameServer/MagicInstance.cpp:395,413`; yerel MAGIC verisi | `[D]` `[V]` |
| MEC-MAG-06 | Skill ile R arasında sunucuda hiçbir zamanlama bağı yoktur; aynı saniyede bir R ve bir Type1 skill kabul edilir. | grep `RHitRepeat` | `[D]` |
| MEC-MAG-07 | `UseStanding = 1` olan skill'ler son hareket hızı 0 değilse reddedilir; bot önce durma hareketi göndermelidir. | [`GameServer/MagicInstance.cpp:327-329`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L327-L329) | `[D]` |
| MEC-MAG-08 | MP, skill çalıştırılmadan **önce** düşülür; çalıştırma başarısız olsa bile MP harcanmış olur. İstisna: tek hedefli Type4 yalnızca başarılı uygulamada MP düşer. | `GameServer/MagicInstance.cpp:994-1045,1795-1797` | `[D]` |
| MEC-MAG-09 | Genel bir başarı zarı yoktur; `bSuccessRate` yalnızca yıldırım stun görseli için okunur. | [`GameServer/MagicInstance.cpp:1361`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L1361) | `[D]` |
| MEC-MAG-10 | Sunucu saati `UNIXTIME` 1 sn çözünürlüklüdür; tüm sunucu cooldown'ları tam saniye karşılaştırır. Gerçek minimum aralık 0–2 sn arasında değişebilir. | [`shared/TimeThread.cpp:26-43`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/TimeThread.cpp#L26-L43) | `[D]` |
| MEC-MAG-11 | **Skill menzili.** EFFECTING/FLYING'de `sRange > 0` ise hedefe mesafe `>= sRange` (metre) olan skill reddedilir; CASTING'te aynı denetime yalnızca `UseStanding = 1` skill'ler girer. Çok hedefli Type3'te `sRange` dışındaki hedef atlanır (skill yine tüketilir). Ayrıca Type 1-3 skill'lerde `isInAttackRange` cömert bir üst sınır uygular (`15 m + sRange`, `sRange = 0` ise `15 m + silah menzili`); asıl dar sınır `sRange`'dir. | [`GameServer/MagicInstance.cpp:352-356`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L352-L356), `:304-308`, `:1337-1339`, `:280`; `GameServer/User.cpp:5047-5105` | `[D]` |
| MEC-MAG-12 | **Uçan Type3 skill'de MP iki kez düşer.** `MAGIC_FLYING` (opcode 2) gelince sunucu, skill Type2 tablosunda yoksa (Type3 skill) `sMsp` yeterliyse `MSpChange(-sMsp)` uygular ve bölgeye yayınlar; ardından `MAGIC_EFFECTING`'te `IsAvailable()` Type3 için `MSpChange(-sMsp)`'i yeniden uygular. Yalnızca `bType[0] == 2 && FlyingEffect != 0` EFFECTING'te MP düşmeden döner. Sonuç: Type3 uçan skill (Fire ball 110515: `Msp 50`) bir cast'te **2 × `Msp`** harcar; FLYING'siz gönderilen EFFECTING bir kez düşer. FLYING sırasında `IsAvailable()` (seviye/silah denetimleri) çağrılmaz; menzil/recast/tip kapısı `CheckSkillPrerequisites`'te hem FLYING hem EFFECTING'te uygulanır. | `GameServer/MagicInstance.cpp:45-94` (`:83-90`), `:284-286`, `:999-1029` (`:1003`, `:1029`), `:296-311` | `[V]` (kod `[D]`; çalışma zamanı ölçümü `plans/F4-25` S1, 2026-10-02: Fire ball `110515` `Msp 50`, MP 6021 → 5971 FLYING sonrası → 5921 EFFECTING sonrası) |
| MEC-MAG-13 | **Çift tipli Type3 + Type4 skill (`bType = {3, 4}`: buz büyüleri, Prismatic).** `Run()` EFFECTING'te `ExecuteSkill(bType[0])` çalıştırır, başarılıysa `bType[0]` ve `bType[1]` için tip damgalarını yazar ve `ExecuteSkill(bType[1])` çalıştırır. Yanıt paketini **yalnızca Type4 bölümü** yayınlar: `ExecuteType3` skill paketini yalnızca `bType[1] == 0 \|\| bType[1] == 3` iken yayınlar; `ExecuteType4` `bType[1] == 0 \|\| bType[1] == 4` iken `{sData[0], bResult, sData[2], süre (sn), sData[4], hız, sData[6]}` ile bölgeye (çağıran dahil) yayınlar. Sonuçlar: yankıda `sData[3]` Type4 süresidir, `SKILLMAGIC_FAIL_ATTACKZERO` (−104) olamaz ("missed" bu yoldan ayırt edilemez); hedef Type3 kısmında ölürse/geçersizleşirse Type4 `false` döner ve **hiç yanıt gelmez**; Type4 `bResult = 0` (debuff engeli) ise yankıdan sonra ek `MAGIC_FAIL` gelir. MP bir kez düşer (`bType[0] == 4` istisnası uygulanmaz, `:1800-1802`); skill uçansa MEC-MAG-12'deki gibi FLYING + EFFECTING'te iki kez. Same-type kapısı (MEC-MAG-03) **iki tip için ayrı** denetlenir, ikisinden biri kapıdaysa skill reddedilir. | `GameServer/MagicInstance.cpp:109-131`, `:332-346`, `:389-423`, `:1598-1599`, `:1682`, `:1800-1802`, `:1862-1874`, `:1892-1893`; `shared/packets.h:402` | `[D]` (çalışma zamanı doğrulandı 2026-10-02, `plans/F4-26` Doğrulama Turu 1: yankı Type4'ten, `code` = süre 10/11/12; MP uçmayanda tek (Ice comet 160, Prismatic 390), uçanda 2 × Msp (Ice arrow 50 + 50, Ice orb 80 + 80); tip kapısı iki tipte ≥ 1000 ms; "hedef Type3'te ölürse yanıt yok" ve `bResult = 0` dalları ölçülmedi) |
| MEC-MAG-14 | **Skill'in quest ile açılması (istemci ↔ sunucu farkı).** Sunucu: `UserCanCast()` (`GameServer/MagicInstance.cpp:269-275`) yalnızca **Release** derlemesinde (`#if !defined(DEBUG)`), GM değilse ve `sEtc != 0` ise `CheckExistEvent(sEtc, 2)` ister; `CheckExistEvent` (`GameServer/QuestHandler.cpp:137`) **tam durum eşitliğidir** (kayıt yoksa yalnızca durum 0 için doğru). Quest listesi `USERDATA.strQuest`'ten gelir (`DBAgent.cpp:388-406`: 3 bayt/quest = `uint16` kimlik + `uint8` durum, en çok `QUEST_LIMIT` = 200); botlar da aynı yolla yüklenir (`WIZ_SEL_CHAR` → veritabanı kuyruğu, `BotManager.cpp:3453`), GM değildir (`Authority = 1`), `db/002` `strQuest`'i **boş** yazar. İstemci: `Client/Data/Skill_Magic_Main_us.tbl` (şifre: yuvarlanan XOR, `tools/client-tbl-quests.py`) 31 kolon × 1779 satır; **kolon 29 quest kimliği**, 104 satırda sıfırdan farklı, 18 kimlik: **51–54** (70. seviye skill'ler) ve **510–523** (72–80). İstemci bu skill'leri quest bitmeden "?" ikonuyla çizip kullandırmaz `[A]` (veri örüntüsü + proje sahibi gözlemi; istemci exe'sinden doğrulanmadı). Sunucu verisi (`MAGIC`): **72 satırda `Etc` = istemci quest kimliği (510–523): iki tarafta kilitli**; **32 satırda (istemci 51–54) `Etc = 0` ve `UseStanding` = 51–54** (KI-017): sunucuda kilitli değil, istemcide kilitli. Yedek tablo (`MAGIC_BAK_etc`) bu 32 satırın düzeltme öncesi `Etc` değerinin **1** olduğunu gösterir (KI-001 yalnızca "quest 1" şartını kaldırdı); 51–54 değerleri hiçbir zaman `Etc`'te değildi, quest kimliği yanlış sütunda (`UseStanding`). Quest ↔ NPC (`QUEST_HELPER.sEventDataIndex`): Skaki (savaşçı: 51, 510, 511), Clarence (rogue: 52, 512–514), Drake (mage: 53, 515–517), Minerva (priest: 54, 518–523); quest seviyeleri 70/72/74/75/76/78/80 (skill puanı eşiğiyle aynı). Yan etki: savaşçıda quest 51 (durum 2) savunma ağacı ≥ 70 iken savunma bonusunu 50 → 60 yapar (`GameServer/User.cpp:2276`). Skill puanı (`m_bstrSkill[ağaç]` ≥ `SkillLevel`, `MagicInstance.cpp:956-960`) ayrı koşuldur; referans profillerde (docs/04: `[70,0,52,20]` vb.) hiçbir ağaç 70'i aşmadığından 72–80 skill'ler quest verilse de erişilemez. Bot sonucu: `BeginCast` `sEtc != 0` skill'i `unsupported_skill` ile reddeder (F4-03); `F4-27` bunu botun quest listesine bakan `quest_locked` kuralına çevirir ve bot satırlarına sınıfının quest'lerini durum 2 yazar | `GameServer/MagicInstance.cpp:269-275`, `:956-960`; `GameServer/QuestHandler.cpp:137`; `GameServer/DBAgent.cpp:388-410`; `GameServer/User.cpp:2276`; `db/002_bot_characters.sql` (`strQuest` boş); `tools/client-tbl-quests.py --server` (`etc_equals_quest=72 usestanding_equals_quest=32 other=0`); `MAGIC_BAK_etc`; `QUEST_HELPER` | `[D]` sunucu, `[V]` veri/ölçüm (2026-10-03), `[A]` istemci "?" davranışı |
| MEC-MAG-15 | **Tek hedefli Type4 skill (`bType = {4, 0}`: buff/debuff; `Moral` 1, 2, 7).** `IsAvailable()` CASTING ve EFFECTING'te `CheckType4Prerequisites()` çağırır: hedef bir oyuncuysa ve skill **buff** ise (`CMagicProcess::IsBuff`: ör. AC için `sAC >= 0` ya da `sACPct >= 100`; `Malice` `ACPct 75` ⇒ debuff) hedefin buff haritasında aynı `BuffType` varsa `false` döner ⇒ `Run()` `SendSkillFailed()` (CASTING'te `−100`, diğer opcode'larda `−103`). Çakışan şey skill id değil **`BuffType`**'tır (Defense `106007` ve Insensibility Skin `112603` ikisi de `BuffType 2`). **Debuff** için bu denetim yoktur: aynı `BuffType` hedefteyse eskisi silinip süre yenilenir ve skill başarılı sayılır; debuff aynı `BuffType`'taki bir buff'ı da siler (`:1756-1760`). **MP:** `IsAvailable()` `bType[0] == 4 && sTargetID != -1` iken MP düşmez; `ExecuteType4` `GrantType4Buff` başarılı olunca bir kez düşer (`:1800-1802`): reddedilen buff MP harcatmaz, başarılı olan bir kez harcar (MEC-MAG-08 istisnası). **Yayın:** `bType[1] == 0` iken `{sData[0], bResult, sData[2], süre (sn), sData[4], hız, sData[6]}` bölgeye (çağıran dahil); `sData[3]` süredir, `−104` olamaz (`missed` yok); `bResult = 0` ise yayından sonra ek `MAGIC_FAIL` (`−103`). Yayın yapılmayan çıkışlar: hedef ölü/`isBlinking` (`:1678-1686`), `sRange > 0` ve mesafe `>= sRange` (`:1695-1696`). Hız/stun debuff'larında (`BuffType` 6/40/47, `Moral` 7) direnç zarı başarısız olsa da yayın `bResult = 1` ile yapılır (MB-09). Bot sonucu: buff uygulandı/debuff yenilendi ⇒ `effected` (`code` = süre), reddedilen buff ⇒ `srv_fail`, sessiz çıkış ⇒ `no_result`. | `GameServer/MagicInstance.cpp:22-29`, `:284-286`, `:540-583`, `:705-718`, `:809-823`, `:857-866`, `:1011-1012`, `:1028-1029`, `:1678-1696`, `:1749-1802`, `:1861-1893`; `GameServer/MagicProcess.cpp:1017`; `shared/packets.h:395-402` | `[D]` (çalışma zamanı sınaması `plans/F4-28` §7 S1–S4 bekliyor) |
| MEC-MAG-16 | **Düşman alan skill'i (`Moral` 10 = AREA_ENEMY; `bType = {3, 0}`, `{3, 4}`, `{4, 0}`; hedef kimliği `-1`).** `UserCanCast()` alan moral'inde (10..13) hedef kimliği `-1` değilse `SkillUseFail` (item proc hariç, `:170-180`); hedef noktası paketin `sData[0]` (x) ve `sData[2]` (z) alanlarından okunur. `IsAvailable()` moral `switch`'inde `MORAL_AREA_ENEMY` yoktur; Type3 alan için `CheckType3Prerequisites()` güvenli bölgede `false` (`:460`; ayrıca her skill için `:355`), Type4 alan için `CheckType4Prerequisites()` `true` (`:549-554`, aynı `BuffType` denetimi alanda yok). **MP:** EFFECTING'te `ExecuteSkill`'den önce bir kez düşer (`:1028-1029`, `bType[0] != 4 || sTargetID == -1`), **kurban yoksa da**; Type4 alanda `ExecuteType4`'ün ek düşümü uygulanmaz (`:1800-1802` yalnızca `sTargetID != -1`). **Kurban seçimi** (Type3 `:1276-1307`, Type4 `:1635-1669`): çağıranın 3×3 bölgesindeki birimlerden GM, çağıranın kendisi, ölü/blink/saldırılamaz olanlar elenir; `UserRegionCheck` (`MagicProcess.cpp:113-208`) `isHostileTo` ve `radius == 0 || kurban.isInRangeSlow(sData[0], sData[2], radius)` (2D) ister; ayrıca **çağırandan** kurbana mesafe `>= sRange` ise kurban atlanır (`:1337-1339`, `:1695`): hedef noktası menzil içinde olsa da kümenin uzak kenarı kaçabilir. **Yayın:** Type3 alan her kurban için hedef kimlikli EFFECTING yayınlar (`bType[1] == 0 || 3`, `:1599`, çağıran dahil) ve sonunda hedef `−1`'li bir alan EFFECTING'i (`:1611-1613`); `{3, 4}` alanda kurban paketlerini Type4 yayınlar (`code` = süre, `:1861-1874`), Type3 yalnızca son `SendSkill()`'i. **Kurban yoksa:** Type3 `SendSkill(); return true` (başarılı: bekleme/tip damgası yazılır, `:1299-1302`); Type4 `SendSkill(); return false` (`:1664-1677`: yankı gelir ama sunucu bekleme/tip damgası yazmaz ve `bType[1]`'i çalıştırmaz). `sData[3]` çağıranın gönderdiği değerdir; `−104` (`missed`) alanda olamaz. Type4 kurbanda `bResult = 0` ise kurban paketinden sonra ek `MAGIC_FAIL` (`−103`, `:1891-1893`). Bot sonucu: yankı ⇒ `effected` (`code` 0 ya da Type4 süresi), `ACTION_RESULT.victims` = çağırana gelen kurban kimlikli EFFECTING sayısı (NPC dahil; isabet değil), hedef noktası `>= sRange` ⇒ bot guard `out_of_range` (CLI-07), güvenli bölge/ağaç puanı ⇒ `srv_fail`. | `GameServer/MagicInstance.cpp:170-180`, `:355`, `:460`, `:549-554`, `:1028-1029`, `:1276-1307`, `:1337-1339`, `:1599`, `:1611-1613`, `:1635-1677`, `:1695`, `:1800-1802`, `:1861-1893`; `GameServer/MagicProcess.cpp:113-208`; `GameServer/Unit.cpp:133-136`, `:805-808` | `[D]`; çalışma zamanı: hedef kimliği `-1`, hedef noktası, boş alanda `effected` + MP düşer, `{3, 4}`/`{4, 0}` kurban başına paket ve `victims`, menzil dışı `out_of_range`, iptal paketi `-1`, ağaç yetersizliği `srv_fail` doğrulandı (`plans/F4-29` Doğrulama Tur 1, S1–S6, 2026-10-03); ölçülmedi: Type4 alanda boş alanın sunucu bekleme damgası, kurban başına `sRange` kenar kaybı, gerçek istemci paketi (`[Ö]`) |
| MEC-MAG-17 | **Uçan düşman alan skill'i (`Moral` 10 + Type3 `FlyingEffect != 0`: Fire burst `110533`, Ice burst `110633` `{3, 4}`, Thunder burst `110733`).** MEC-MAG-12 (uçan MP) ve MEC-MAG-16 (alan) birlikte geçerlidir; ek kural yoktur. `MAGIC_FLYING` dalı `pSkillTarget`'ı kullanmaz, hedef kimliği `-1` bu dalı engellemez (`Run()` hedefi yalnızca `sTargetID != -1` iken çözer; `UserCanCast()` alan moral'inde `-1`'i *ister*); skill `MAGIC_TYPE2` tablosunda olmadığından ok/silah denetimi yoktur, MP yeterliyse `MSpChange(-Msp)` ve `SendSkill(true)` ile FLYING bölgeye yayınlanır. EFFECTING'te `IsAvailable()` MP'yi **bir kez daha** düşer (`bType[0] != 4 \|\| sTargetID == -1`, `bFlyingEffect` yalnızca Type2'de MP'yi atlatır): **cast başına 2 × `Msp`** (burst: 150 + 150). Kurban seçimi, yayın ve sonuç MEC-MAG-16 ile aynıdır (`bFlyingEffect` `MagicInstance.cpp`'de yalnızca `:1003`'te okunur). **Skill ağacı/seviye denetimi** `IsAvailable()` içindedir ve `UserCanCast()` onu **CASTING ve EFFECTING'te** çağırır, FLYING'de çağırmaz (`:284-286`): `CastTime > 0` olan burst'lerde ağacı yetersiz oyuncu CASTING'te `MAGIC_FAIL` alır, MP düşmez; yalnızca `CastTime == 0` olan bir uçan skill'de MP ağaç denetiminden önce (FLYING'de) düşerdi. İstemci ölçümü (CLI-03 satır 425, Fire burst): CASTING → FLYING +1539 ms → EFFECTING +2576 ms, üç pakette `target = -1` ve `sData[0..2]` hedef noktası; istemci EFFECTING ile aynı anda `opcode 4`, `sData[3] = -101` paketi de yollar (sunucu yalnızca iletir). Bot: üç paketin hepsinde hedef kimliği `-1` ve aynı hedef noktası; başlangıç guard'ı `2 × Msp` ister (`CastManaNeed`); FLYING ≥ `CastTime×100 + 80` ms sonra, EFFECTING FLYING'den ≥ 1000 ms sonra (`kFlightMinMs`, tek ölçüm `[A]`); `victims` yalnızca EFFECTING adımındaki kurban kimlikli paketlerden (FLYING yayını sayılmaz). | `GameServer/MagicInstance.cpp:20`, `:45-94`, `:96-134`, `:170-180`, `:284-286`, `:350`, `:956-963`, `:999-1029`, `:1003`; `docs/03` MEC-MAG-12, MEC-MAG-16, CLI-03 | `[V]` (F4-30 doğrulaması, 2026-10-03: Fire burst `110533` üç pakette `target -1`, MP 6021 → 5871 → 5721 (2 × 150), `victims 2`; boş alan `victims 0` ve MP yine −300; Ice burst `110633` `{3, 4}` `code 15` + yavaşlatma debuff; FLYING'de iptalde `Msp` geri gelmedi; ağacı yetersiz Thunder burst CASTING'te `srv_fail` (`code -100`), MP düşmedi); ölçülmedi: uçuş süresinin mesafe bağımlılığı, gerçek istemcinin CASTING'te hedef noktası göndermesi (`[Ö]`) |
| MEC-MAG-18 | **Party hedefli skill (`Moral` 4 = PARTY tek üye/kendisi; `Moral` 6 = PARTY_ALL tüm party; `bType = {3, 0}`/`{4, 0}`).** **`Moral` 4:** `IsAvailable()` `MORAL_PARTY` (CASTING ve EFFECTING): NPC çağıran/hedef ⇒ `false`; çağıran party'de değilse hedef kendisinden başkası olamaz; hedef varsa `GetPartyID()` çağıranınkiyle aynı olmalı (kendisi geçer), aksi `MAGIC_FAIL`. Tek hedefli Type4 kuralları MEC-MAG-15 ile aynıdır (aynı `BuffType` reddi, MP yalnızca başarıda bir kez). Paket: hedef kimliği = üye kimliği, koordinat `self`'te 0. **`Moral` 6:** `IsAvailable()`'ta denetim yoktur; `UserCanCast()` hedef `-1`'i yalnızca `Moral` 10..13'te *ister*, `Moral` 6'da kimlik verilirse skill tek hedefli çalışır; **hedef `-1` grup yoludur** (`ExecuteType3`/`ExecuteType4`): kurban seçimi `UserRegionCheck` `MORAL_PARTY_ALL` (NPC olamaz; hedef party'de değilse yalnızca çağıranın kendisi; aynı `PartyID` ise `Radius` içinde `radius == 0 || isInRangeSlow(sData[0], sData[2], radius)`), ayrıca çağırandan kurbana mesafe `>= sRange` ise atlanır. **Type3 grup heal:** çağıran (`sFirstDamage > 0 || sTimeDamage > 0` ise) her zaman listede, kurban yoksa `SendSkill(); return true`; `CheckType3Prerequisites()` güvenli bölgede `false`, `Moral` 6 + HoT (`sTimeDamage > 0`) çağıranda etkin restoration varken `false` (anlık heal `112557`/`112560` etkilenmez). **Type4 grup buff:** `CheckType4Prerequisites()` hedef `-1`'de `true` (aynı `BuffType` denetimi yok); kurban listesi boşsa `Moral` 6'da çağıran tek kurban olur; hedefte aynı `BuffType` varsa **o üye sessizce atlanır** (`bSkillTypeAlreadyOnTarget && isBuff` ⇒ `continue`, hata ve yayın yok); hiçbir üye buff almazsa yayın yoktur. **MP:** `Moral` 6'da EFFECTING'te bir kez (`bType[0] != 4 || sTargetID == -1`, kurban yoksa da; `ExecuteType4`'ün ek düşümü yalnızca `sTargetID != -1`). Paket: hedef kimliği `-1`, `sData[0..2]` hedef noktası (F4-29 ile aynı). **Kendini öldüren skill:** `MAGIC.HP >= 10000` ise sunucu çağırandan 10 000 HP düşer, can denetimi yoktur, kendine atılamaz (`:1041-1048`); bot sınıflarında yalnızca Sacrifice `106660` (`Moral` 4, `Msp 180`, `HP 10001`): bot bunu `unsupported_skill` ile reddeder. Bot sonucu: `Moral` 4 buff ⇒ `effected` (`code` = süre), reddedilen buff/üye olmayan hedef ⇒ CASTING'te `srv_fail`; `Moral` 6 heal ⇒ `effected`, `code` 0, `victims` = çağırana gelen kurban kimlikli EFFECTING sayısı (çağıran dahil; party'siz çağıranda 1); `Moral` 6 buff ⇒ `effected` (`code` = süre) ya da tüm üyelerde buff varsa `no_result`; ağaç yetersizliği (KI-016) ⇒ CASTING'te `srv_fail`. | `GameServer/MagicInstance.cpp:20`, `:170-180`, `:284-286`, `:446-478`, `:540-583`, `:831-847`, `:956-963`, `:1028-1029`, `:1041-1048`, `:1276-1307`, `:1635-1677`, `:1770-1801`; `GameServer/MagicProcess.cpp:113-141`; `GameServer/MagicInstance.h:23-34` | `[V]` (F4-31 doğrulaması, 2026-10-03, çalışma zamanında ölçüldü: `Moral` 6 grup heal `112557`: paket hedef `-1`, `effected`, `code 0`, `victims` = çağıran + hedef noktasının `Radius 30` m içindeki party üyesi (3 üyeli party'de 3; hedef noktasından 35 m'deki üye sayılmadı; `BotMF_K` hedef alınınca nokta `z=925`, `victims 2`), +960 HP, MP bir kez −960; party'siz çağıran `victims 1`; grup buff Greatness `112656`: `code 600`, `victims 3`, MaxHP +1200, aynı `BuffType` hepsinde varken tekrar ⇒ `no_result` ve MP yine −570; `Moral` 4 Mightness/Brave/Strong: paket hedef kimliği = üye/kendi, `effected` `code 600`, aynı `BuffType` ⇒ CASTING'te `srv_fail` (`op 4`, `code -100`) ve MP düşmez, party dışı/düşman hedef ⇒ `srv_fail`, kendine party'siz `effected`; iptal paketi `Moral` 6'da `-1`, `Moral` 4'te üye kimliği; ağaç yetersizliği (`BotPHD` buff) CASTING'te `srv_fail`; Sacrifice `106660` bota `unsupported_skill`, paket yok, can değişmez; Group heal `112570` (`SkillLevel 70`) bot kabul eder, sunucu `srv_fail`. Ölçülmedi: gerçek istemcinin grup skill'i paketi `[Ö]`) |
| MEC-MAG-19 | **Type5 cure (`Type1 = 5`, `Moral` 2 = FRIEND_WITHME, dost tek hedef: Cure curse `112525`, Cure disease `112535`).** `IsAvailable()` `MORAL_FRIEND_WITHME` (CASTING ve EFFECTING): hedef çağıranın kendisi değilse ve düşmansa `MAGIC_FAIL`; dost için party gerekmez. `ExecuteType5` (oyuncu çağıran; `sTargetID != -1` ise hedef oyuncu olmalı, **ölü hedef** diriltme değilse `false` ve yayın yok) hedef tipine göre çalışır: `MAGIC_TYPE5.Type` 2 = `REMOVE_TYPE4` (hedefin yalnızca `isDebuff()` Type4 kayıtları kalkar, buff'lar kalır), 1 = `REMOVE_TYPE3` (DoT'lar, `m_sHPAmount < 0`, `Reset()` ve hedefe `MAGIC_DURATION_EXPIRED` 200; HoT'lar korunur). **Kaldırılacak bir şey olmasa da** çağırana (bölge yayını) hedef kimlikli EFFECTING gider (`bType[1] == 0` ⇒ koşulsuz `BuildAndSendSkillPacket`, `sData[1] = 1`). **MP:** `IsAvailable()` EFFECTING'te `bType[0] != 4` iken `Msp`'yi bir kez düşer, yani Type4 tek hedefli kuralındaki "yalnızca başarıda" Type5'te yoktur; ölü hedefte MP düşer, yayın yoktur (çalışma zamanında ölçüldü: net −20 = −60 + ~40 kümeli yenileme). Tip kapısı Type 1..7 için ortaktır (0,7 sn), başarıda `bType[0]` damgalanır. Veri (bot sınıfları, `MAGIC`/`MAGIC_TYPE5`): Cure curse `Msp 60`, Cure disease `Msp 120`, ikisi de `SkillLevel` 25/35, `Skill 1125`, `CastTime 15`, `ReCastTime 15`, `Range 56`; Bless of God `112671` (`Moral` 6) ve Resurrection `1127xx` (`Moral` 25, `UseItem`) botta kapalı. Paket: `Moral` 2 tek hedefli (hedef kimliği = dost ya da kendi, koordinat `self`'te 0, dostta konum; F4-28 ile aynı). Bot sonucu: `effected` (`code 0`, `victims` yok), düşman hedef ⇒ CASTING'te `srv_fail`, ölü hedef ⇒ `no_result` (MP düşer) `[V]`; `effected` cure'ün etkili olduğunu **kanıtlamaz**. | `GameServer/MagicInstance.cpp:166`, `:389-405`, `:819-823`, `:1028-1029`, `:1898-2063`; `GameServer/MagicInstance.h:55-59` | `[V]` (F4-32 doğrulaması, 2026-10-03: `plans/F4-32-aksiyon-yurutucu-type5-cure.md` Doğrulama Raporu Tur 1, S1–S6. Ölçüldü: Cure curse kendine/dosta `snap` `buffs 1` → `0` (Chill debuff'ı), hedef kimlikli `CastStart`/`CastEffect`, `effected` `code 0`, `victims` yok; Cure disease Hell fire DoT'unu durdurur (HP düşüşü ≈ −40/1,6 sn → 10 sn sabit; cure'süz kontrolde düşüş sürer); boşa cure (`buffs 0` hedef) `effected` ve MP düşer (−60 / −120 + yenileme); düşman hedef ⇒ `srv_fail`, MP değişmez; ölü dost ⇒ `no_result`, MP düşer; menzil 72 m ⇒ `out_of_range`, paket yok; CASTING'te iptal `cancelled`, iptal paketi hedef kimlikli; Bless of God/Resurrection/Absoluteness `unsupported_skill`, paket yok. Ölçülmedi: HoT'ların korunması ve buff'ların cure sonrası yerinde kalması (`[D]` kod okuması), `MAGIC_DURATION_EXPIRED` paketinin bota yansıması) |
| MEC-MAG-20 | **Diriltme (`Type1 = 5`, `Moral` 25 = CORPSE_FRIEND, `UseItem` 379006000 Stone of Life, `MAGIC_TYPE5.Type` 3 = RESURRECTION: Resurrection of love `112733`/grace `112742`/favors `112754`, El Morad `2127xx`).** `IsAvailable()` `MORAL_CORPSE_FRIEND` (CASTING ve EFFECTING): hedef yoksa, çağıranla düşmansa, çağıranın kendisiyse ya da **hayattaysa** `MAGIC_FAIL`. `UserCanCast()` Type5 için `m_Magictype5Array` kaydını okur ve `CanUseItem(iUseItem, sNeedStone)` kontrolünü **ölü HEDEFE** yapar (çağırana değil); ikinci (çağıran için) eşya denetimi `bType != RESURRECTION` ile atlanır. Hedefte `NeedStone` (4/10/30) adet Stone of Life yoksa CASTING reddedilir. EFFECTING: MP `Msp` (400/600/800) bir kez düşer (`bType[0] != 4`); `ExecuteType5` `RESURRECTION` dalı (hedef ölü olmalı, hayattaysa `false`, yayın yok) hedeften `NeedStone` taşı `RobItem` eder, çağırana `NeedStone / 2 + 1` taş verir ve hedefi `Regene(1, id)` ile **ölen yerinde** diriltir: tam HP, **hedefin MP'si 0**, buff'lar sıfırlanır (`InitType4`), `m_sWhoKilledMe == -1` ise `m_iLostExp * ExpRecover / 100` exp iadesi; konum değişmez, blink yok. Taş EFFECTING'te eksikse dal sessizce atlanır ama sondaki bölge yayını (hedef kimlikli EFFECTING, `bType[1] == 0` ⇒ koşulsuz) yine gider: **yayın diriltmenin gerçekleştiğini kanıtlamaz**. Başarılı EFFECTING sonunda `ConsumeItem()` çağıranın çantasından ek 1 adet `iUseItem` almaya çalışır `[D]`. Veri: `Range 11`, `CastTime 15`, `ReCastTime 250` (**25 sn**), `Etc 0`, `SkillLevel` 33/42/54, `Skill 1127` (priest lanet ağacı); bot satırlarında 30 Stone of Life (`db/002`), `BotPHD_*` `strSkill[7] = 62` (diriltebilir), `BotPHB_*` `[7] = 0` (CASTING'te ağaç yetersizliği, KI-016). Paket: tek hedefli (hedef kimliği = ölü botun kimliği, koordinat hedefin konumu; F4-28/F4-32 ile aynı). Bot sonucu: `effected` (`code 0`, `victims` yok); hedef hayatta/hedefte taş yok/düşman/ağaç yetersiz ⇒ CASTING'te `srv_fail` (MP düşmez); `self` ⇒ bot `bad_target`; `480001` (`RESURRECTION_SELF` öğe skill'i) ve Bless of God kapalı. | `GameServer/MagicInstance.cpp:166`, `:229-262`, `:868-878`, `:1028`, `:1898-2063` (`:2033-2041`), `:2983-3000`; `GameServer/AttackHandler.cpp:100-` (`Regene`); `GameServer/MagicInstance.h:47`, `:55-59`; `shared/database/structs.h:102-108` | `[V]` (F4-33 doğrulaması, 2026-10-03: `plans/F4-33-aksiyon-yurutucu-diriltme.md` Doğrulama Raporu Tur 1, S1–S7. Ölçüldü: Resurrection of love `112733` ve grace `112742` ölü dost botu ölen yerinde diriltir (hedef kimlikli `CastStart`/`CastEffect`, `effected` `code 0`, `victims` yok; hedef `hp` = `MaxHP`, `mp` 0, konum aynı, `snap` `buffs 0`; çağıran MP tam −400 / −600); hedef hayattaysa ⇒ `srv_fail`, MP değişmez; **taşlar ölü hedeften alınır:** hedefte 16 taş varken favors (`NeedStone` 30) CASTING'te `srv_fail`, aynı cesette grace (10) `effected` ve hedef dirildi; `BotPHB_*` (1127 = 0) ⇒ `srv_fail`; düşman ceset ⇒ `srv_fail`; ceset 22 m uzakta ⇒ `FAIRNESS_REJECT` `MEC-MAG-11` `out_of_range`, `limit 11.00`, paket yok; CASTING'te iptal ⇒ `cancelled`, ceset ölü kalır; `self` ⇒ `bad_target`; `480001`, Bless of God `112671` ⇒ `unsupported_skill`, `111733` ⇒ `bad_skill`, paket yok. Ölçülmedi, `[D]` kalır: çağıranın taş değişimi (`+NeedStone/2+1`, `ConsumeItem` ek −1), exp iadesi, "taş EFFECTING'te eksik ama yayın gider" yarışı, favors `112754` başarılı atımı (hedefte 30 taş bırakılmadı), El Morad `2127xx`) |
| MEC-MAG-21 | **Summon (`Type1 = 8`, `Moral` 4 = PARTY, `UseItem 0`, `MAGIC_TYPE8.WarpType` 12 = zone içi summon: summon friend `110004`/`210004`; novice sınıfı `109004`/`209004`).** `IsAvailable()` `MORAL_PARTY` (CASTING ve EFFECTING): NPC çağıran/hedef ⇒ `false`; çağıran party'de değilse hedef kendisinden başkası olamaz; hedef varsa `GetPartyID()` çağıranınkiyle aynı olmalı, aksi `MAGIC_FAIL` ⇒ party dışı hedef (düşman dahil) **CASTING'te** `srv_fail`. Tip kapısı Type8'i kapsamaz (`bType[0]` 1..7); skill başına yeniden-kullanım `ReCastTime * 100` = 100 ms. `sSkill % 10 == 0` olduğundan skill ağacı denetimi yoktur, yalnızca `SkillLevel 4 <= seviye`. EFFECTING: MP `Msp` (5) **bir kez ve `ExecuteType8`'den önce** düşer (`IsAvailable()`, `bType[0] != 4`): summon başarısız olsa da MP gider. `ExecuteType8` tek hedefli dalda hedefi `GetUserPtr(sTargetID)` ile alır (`nullptr` ⇒ `false`, yayın yok); `WarpType != 11` iken **ölü hedef ya da `!canTeleport()`**, `m_bWarp`, **başka zone** ya da **hedef = çağıran** ⇒ `goto packet_send`: ışınlanma yok ama çağırana hedef kimlikli EFFECTING `sData[1] = 0` ile yine yayınlanır. Başarıda hedefe bölge yayını (`sData[1] = 1`), `pTUser->Warp(caster.GetSPosX(), caster.GetSPosZ())` (hedef çağıranın konumuna) ve çağırana EFFECTING `sData[1] = 1`. `CUser::Warp` (aynı zone) `m_bWarp` bayrağını kurmaz; bot istemci onayı göndermek zorunda değildir. Forgotten Temple ve (`ZoneID > ZONE_BIFROST` ile `490042/490050`) `SendSkillFailed` + `false`. **Yayın summon'un gerçekleştiğini kanıtlamaz** (`sData[1]` başarıyı taşır; botun `m_castEcho`'su yalnızca `op`/`sData[3]`/skill saklar); gerçek sonuç hedefin konumudur ya da olay halkasında `SkillEvent.data[1]`. Veri: `CastTime 15`, `ReCastTime 1`, `Range 22500` (menzil fiilen sınırsız; `Radius 10000` tek hedefli yolda kullanılmaz), `Etc 0`, `UseStanding 0`, `FlyingEffect 0`. Bot sonucu: `casting`, sonra `effected` (`code 0`, `victims` yok); party dışı hedef ⇒ CASTING'te `srv_fail` (MP düşmez); `self` ⇒ bot `bad_target`; Gate/Escape/Blink/descent/Wild advent kapalı (`unsupported_skill`). MEC-T8-02'deki "respawn'dan sonra 180 sn" kuralı (`CLAN_SUMMON_TIME`) yalnızca `UserRegionCheck` (hedef `-1`, `Moral` 6) yolundadır; tek hedefli summon friend o yoldan geçmez. | `GameServer/MagicInstance.cpp:284-286`, `:352-358`, `:831-847`, `:1028-1029`, `:2234-2458` (`:2260-2284`, `:2290`, `:2355-2373`, `:2452-2455`); `GameServer/CharacterMovementHandler.cpp:626-654` (`Warp`); `shared/database/structs.h:164-172` | `[V]` (F4-34 doğrulaması, 2026-10-03: summon friend `110004` party üyesini 35 m'den çağıranın konumuna ışınlar (`effected`, `code 0`, `victims` yok, çağıran MP −5, `Warp` sonrası bot `move`'a yanıt verir); ölü hedefte `effected` ama hedef yerinde ve ölü, MP −5 (3/3); party dışı hedef (Karus/El Morad düşman) ve party'siz çağıran CASTING'te `srv_fail` (`code -100`); `self` ⇒ bot `bad_target`; iptal `cancelled`; Gate/Escape/Blink/descent `unsupported_skill`, `109004` `bad_skill`). `[D]` kalanlar: başka zone / `canTeleport()` false / `m_bWarp` ve Forgotten Temple dalları, "180 sn" kuralının tek hedefli yolda olmaması (Ronark `isWarZone()` ölçülmedi), ışınlanan botun eski bölge kayıtları `[A]` |
| MEC-MAG-22 | **Warp: Gate (`Type1 = 8`, `Moral` 1 = SELF, `MAGIC_TYPE8.WarpType` 1: Gate `110015`/`210015`, priest `112700`/`212700`) ve descent (`Moral` 4 = PARTY, `WarpType` 25: `106650`/`206650`).** **Gate:** `IsAvailable()` `MORAL_SELF` hedefin çağıranın kendisi olmasını ister; `ExecuteType8` `case 1` yalnızca Escape kimliklerini (`109035/110035/209035/210035`) `ZoneID > ZONE_BIFROST`'ta reddeder, **Gate `110015`/`210015` Ronark'ta engellenmez**; hedefe (= çağıran) bölge yayını (`sData[1] = 1`), sonra `GetObjectEvent(m_sBind)` bind noktası varsa oraya, yoksa Ronark'ta (`ZF_ATTACK_OTHER_NATION`, `isWarZone()` değil) `m_StartPositionArray` ile ulusun başlangıcına `Warp` (`START_POSITION` 71: Karus (1380, 1090), El Morad (630, 920)); `packet_send` **ikinci** EFFECTING yayınlar (bot yankısı son yazılanı tutar). `Warp` `m_bWarp` ya da `IsValidPosition` başarısızlığında sessizce döner (yayın gider, ışınlanma yok, MP düşmüş olur). **descent:** `IsAvailable()` `MORAL_PARTY` (party dışı hedef CASTING'te `srv_fail`); skill ağacı `m_bstrSkill[6] >= 50` ister (`BotWG_*` 62 ✔, `BotWP_*` 0 ⇒ CASTING'te `srv_fail`); `case 25` hedef başka zone ya da düşmansa **`return false`** (yayın yok, MP düşmüş), `GetDistanceSqrt(hedef) <= Radius (30 m)` ise çağıran hedefin konumuna `Warp` eder, değilse `SendSkillFailed()` (çağırana `MAGIC_FAIL`) **ve ardından** `packet_send` EFFECTING `sData[1] = 1`: yani uzak hedefte bot `effected` görür, çağıran yerinde kalır, MP gider `[D]`. `Range 225` (metre) pratikte sınırsızdır; asıl mesafe `Radius 30`'dır. **MP:** her ikisinde `Msp` (Gate 30, descent 50) `IsAvailable()` içinde `ExecuteType8`'den önce bir kez düşer. Tip kapısı Type8'i kapsamaz; yeniden-kullanım `ReCastTime * 100` (Gate 10 sn, descent 9,1 sn). `CUser::isInSafetyArea()` zone 71 için `false` döner (`case` yok): Gate'in vardığı yer cast'i engellemez. Veri: Gate `Msp 30`, `CastTime 15`, `ReCastTime 100`, `Range 56`, `SkillLevel 15`, `Etc 0`; descent `Msp 50`, `CastTime 0`, `ReCastTime 91`, `Range 225`, `Radius 30`, `SkillLevel 50`, `Etc 0`. Bot sonucu: Gate `casting` → `effected` (`code 0`), konum bind/başlangıç; descent `casting` → `effected`, konum hedefin konumu; `self` descent ⇒ bot `bad_target`; Gate'e hedef adı ⇒ `bad_target`; Escape/Blink/Wild advent kapalı. | `GameServer/MagicInstance.cpp:813-818`, `:831-847`, `:956-963`, `:352-358`, `:1028-1029`, `:2234-2458` (`:2278-2290`, `:2295-2329`, `:2431-2447`, `:2450-2455`); `GameServer/Unit.cpp:1100-1104`, `:1311-1345`; `GameServer/CharacterMovementHandler.cpp:626-654`; `db/002` skill baytları | `[D]` (F4-35 planı, 2026-10-03; çalışma zamanı doğrulaması bekliyor) |

### 4.3 Doğrulama sırası (EFFECTING) `[D]`

Sıra: `CheckSkillPrerequisites` → `UserCanCast` → `IsAvailable` → `ExecuteSkill` (`GameServer/MagicInstance.cpp:10-147,149-293,295-438,794-1054`).

| # | Kontrol | Not |
|---|---|---|
| C1 | Zone ≠ Prison | |
| C2 | `UseStanding` ⇒ hız 0 | MEC-MAG-07 |
| C3 | Element 0 Type3 tip kapısını siler | MEC-MAG-04 |
| C4 | Menzil: `sRange > 0` ve ayakta değilse `mesafe < sRange`; güvenli alanda ID < 400000 ret | Ronark'ta güvenli alan yok |
| C5 | Skill recast | MEC-MAG-02 |
| C7 | Tip kapısı | MEC-MAG-03 |
| C8 | Hedefte FREEZE ise ret (dost skill'ler dahil) | |
| U1 | Skill kullanabilir ve canlı (ölü yalnızca Type5) | Blink ve kör iken **cast edilebilir** |
| U3 | Sınıf birebir ve seviye ≥ `SkillLevel` | MEC-CHR-01 |
| U5 | MP ≥ `sMsp` | |
| U8 | `UseItem ≠ 0` ise item var ve kullanılabilir (sınıf, `ReqLevel ≤ L ≤ ReqLevelMax`) | Diriltmede taşlar **hedeften** istenir |
| U9 | `BeforeAction ∈ 1..4` ise sınıf taşı tüketilir (379058000 + n·1000) | |
| U11 | **Quest kapısı:** GM değil ve `Etc ≠ 0` ise quest `Etc` durum 2 olmalı. **Yalnızca Release derlemesinde** etkin (`#if !defined(DEBUG)`) | KI-001, §2 |
| U12 | Tip < 4 ve hedef varsa saldırı menzili | |
| I2 | Moral kontrolü (§5.6) | |
| I4 | No-Potion debuff'ı HP potlarını engeller | |
| I5 | Skill ağacı puanı: `m_bstrSkill[Skill%10] ≥ SkillLevel` | `Skill%10 = 9` olan satırlar hiç kullanılamaz |
| I7 | MP/HP düşümü | MEC-MAG-08 |
| E1 | Çalıştır; başarıda recast/tip zamanı kaydı, ikinci tip, item tüketimi | |

## 5. Skill tiplerinin sunucu davranışı

### 5.1 Type1 (yakın dövüş skill'i) `[D]`

- İsabet: `bHitType ≠ 0` ise `sHitRate > rand(0,100)`; değilse isabet/kaçınma oranı × `sHitRate`.
- Hasar `temp_hit·sHit/100` + rastgele; PvP'de `sAddDamage` Ronark'ta **/3** eklenir ve **ıska olsa bile** eklenir ([`GameServer/MagicInstance.cpp:1077-1092`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L1077-L1092)).
- Combo alanları (`bComboType`, `bComboCount`, `sComboDamage`) ve Type1 `bDelay` yüklenir ama **kullanılmaz** `[D]`.

### 5.2 Type3 (doğrudan hasar/heal/DoT/HoT) `[D]`

- Alan hedefleri: çağıranın 3×3 bölgesindeki birimler, hedef noktasına `bRadius` içinde olanlar; heal'lerde çağıran da eklenir.
- Hedef başına `mesafe ≥ sRange` ise sessizce atlanır ([`GameServer/MagicInstance.cpp:1332-1334`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L1332-L1334)).
- `DirectType`: 1 = HP ±, 2 = MP + (oyuncu), 3 = MP ±, 4 = dayanıklılık, 5 = % HP, 8/9 = emme, 11 = saf hasar, 16 = MP emme.
- Heal `HpChange` ile maksimum HP'de kesilir; **overheal takibi yoktur**.
- Süreli etkiler 2 sn'de bir tick eder; **aynı skill'in tekrarı DoT'u üst üste ekler** (40 yuvaya kadar).
- **MEC-T3-01** Hedefte zaten HoT varsa tek hedefli dost HoT reddedilir; çağıranda HoT varsa party HoT reddedilir ([`GameServer/MagicInstance.cpp:462-523`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L462-L523)).

### 5.3 Type4 (buff/debuff) `[D]`

- **MEC-BUF-01** Depolama: `std::map<BuffType, …>`; **her BuffType için tek kayıt**.
- **MEC-BUF-02** Tek hedefe, hedefte **aynı BuffType zaten varsa** (buff ya da debuff) yeni **buff** reddedilir. Güçlü buff zayıfı ezmez; önce süresinin dolması veya iptal edilmesi gerekir.
- **MEC-BUF-03** Yeni **debuff** aynı tipteki kaydı (buff olsa bile) silip yerine yazılır ve süresi yenilenir. Örneğin bir yavaşlatma, hız buff'ını ezer.
- **MEC-BUF-04** Debuff'lar için genel başarı zarı yoktur. Counter Curse tüm debuff'ları engeller; Curse Refraction %25 yansıtır, aksi halde engeller.
- **MEC-BUF-05** Hız, yavaşlatma ve stun debuff'larında oyuncu hedefe direnç zarı atılır: ham direnç < 125 ise yaklaşık **%78,7 direnme**, ≥ 125 ise **%100 direnme** `[D]`/`[I]` (aritmetik). Direnilse bile kayıt sunucu buff haritasında kalır `[D]`; bunun oyundaki etkisi `[A]`.
- **MEC-BUF-06** **Hız, yavaşlatma, stun ve Wall of Iron hareket cezaları sunucuda uygulanmaz**; yalnızca istemciye bildirilir. Bot bunları kendisi uygulamalıdır (CLI-05).
- **MEC-BUF-07** Süre: `endTime = UNIXTIME + sDuration`; `Update()` çağrısı başına **en fazla bir** süresi dolmuş buff kaldırılır ([`GameServer/User.cpp:3443-3460`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L3443-L3460)).
- **MEC-BUF-08** Scroll buff'ları (skill ID > 500000) ölüm ve zone değişiminden sonra otomatik yeniden uygulanır ([`GameServer/User.cpp:5235-5269`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L5235-L5269)).
- **MEC-BUF-09** Tip 4 alan skill'leri 3×3 bölgedeki **tüm** düşman ulus oyuncularının gizliliğini kaldırır ([`GameServer/MagicInstance.cpp:1645-1652`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L1645-L1652)).

BuffType tablosu (sunucu etkisiyle) Ek A'dadır.

### 5.4 Type5 (cure ve diriltme) `[D]`

| Alt tip | Etki |
|---|---|
| `REMOVE_TYPE3` (1) | Tüm DoT'ları temizler, HoT'lar kalır |
| `REMOVE_TYPE4` (2) | Tüm **debuff**'ları kaldırır, kilitli scroll'ları yeniden uygular |
| `RESURRECTION` (3) | Hedefte `sNeedStone` adet taş olmalı; taşlar **hedeften** alınır; çağırana `sNeedStone/2+1` verilir; ardından `Regene(1, skill)` |
| `RESURRECTION_SELF` (4) | Kendini diriltme (kayıp EXP varsa) |
| `REMOVE_BLESS` (5) | `HP_MP` buff'ını kaldırır |

Skill ile diriltilen karakter ceset konumunda kalır, **MP 0** olur, buff'lar sıfırlanıp scroll'lar yeniden uygulanır; PvP ölümünde EXP iadesi yoktur ([`GameServer/AttackHandler.cpp:182-185`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/AttackHandler.cpp#L182-L185)).

### 5.5 Type8 (ışınlanma, summon) `[D]` ([`GameServer/MagicInstance.cpp:2229-2453`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L2229-L2453))

| WarpType | Davranış |
|---|---|
| 1 | Bind noktası veya ulus başlangıcı. Yalnızca Escape kimlikleri (109035/110035/209035/210035) zone > 31'de (Ronark dahil) başarısız; Gate (110015/210015) engellenmez (MEC-T8-04) |
| 11 | Dirilt: HP maksimum, EXP iadesi; `Regene` çağırmaz |
| **12** | **Aynı zone içinde summon:** hedef çağıranın zone'unda, çağıran değil, canlı, ışınlanabilir (No-Recall debuff'ı yok), zaten ışınlanmıyor |
| 13 | Zone'lar arası summon |
| 20 | İleri blink |
| 25 | Hedefe ışınlan (aynı zone, mesafe ≤ `sRadius`) |

- **MEC-T8-01** Summon (12) için party üyeliği yalnızca skill'in moral alanıyla zorlanır; **hedefin onayı yoktur**; `sRadius = 0` ise mesafe sınırı yoktur `[D]`.
- **MEC-T8-02** "Respawn'dan sonraki 180 sn summon edilemez" kuralı yalnızca **savaş zone'larında** uygulanır ([`GameServer/MagicProcess.cpp:130-140`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicProcess.cpp#L130-L140)); Ronark'ta uygulanmaz `[D]`. Tek hedefli summon friend (`Moral` 4, hedef kimliği verilir) bu yoldan geçmez: kural yalnızca hedef `-1` grup yolunda (`UserRegionCheck`, `Moral` 6) vardır, bkz. MEC-MAG-21 `[D]`.
- **MEC-T8-03** Ölü hedef summon edilemez (yalnızca WarpType 11 ölüyü hedef alır) `[D]`.
- **MEC-T8-04** T-MECH-T8-02'nin kod düzeyindeki cevabı: Ronark'ta (`ZONE_RONARK_LAND` 71) **Escape** (`110035`, `Moral` 6) sunucuca reddedilir, **Gate** (`110015`/`210015`, `Moral` 1) reddedilmez ve ulusun başlangıcına ya da bind noktasına ışınlar; descent (`WarpType 25`) hedefe yalnızca `Radius` (30 m) içinde ışınlar. Ayrıntı MEC-MAG-22 `[D]`; çalışma zamanı ölçümü F4-35 S1 bekliyor.

### 5.6 Moral (hedef türü) `[D]` ([`GameServer/MagicInstance.h:23-51`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.h#L23-L51))

| Değer | Ad | Tek hedef kuralı |
|---|---|---|
| 1 | SELF | Yalnızca kendisi |
| 2 | FRIEND_WITHME | Kendisi veya düşman olmayan |
| 3 | FRIEND_EXCEPTME | Düşman olmayan, kendisi değil |
| 4 | PARTY | Aynı party (party yoksa kendisi) |
| 6 | PARTY_ALL | Alan: aynı party |
| 7 | ENEMY | Düşman |
| 10 | AREA_ENEMY | Alan: düşmanlar |
| 11 | AREA_FRIEND | Alan: dostlar |
| 13 | SELF_AREA | Alan: düşman |
| 25 | CORPSE_FRIEND | Ölü, düşman olmayan |

- **MEC-AOE-01** Alan hedefleri çağıranın 3×3 bölgesinden alınır ve **istemcinin verdiği hedef noktasına** `radius` içinde olanlar seçilir. `radius = 0` sınırsızdır. Hedef sayısı sınırı yoktur. Hedef noktasının çağırana menzili kontrol edilmez `[D]`/`[I]`. Bot hedef noktasını çağıranın menzili içinde seçmelidir (CLI-07).

## 6. Potlar

### 6.1 Mekanizma `[D]`

- **MEC-POT-01** Pot kullanımı ayrı bir item opcode'u değildir; potun `ITEM.Effect1` alanındaki MAGIC satırı `WIZ_MAGIC_PROCESS` ile cast edilir. Sunucu `MAGIC.UseItem` alanındaki item'ı kontrol eder ve başarılı olursa tüketir. Anlık HP/MP potu (`Type1 = 3`, `DirectType` 1/2) HP/MP dolu olan kendine cast'te de `MAGIC_EFFECTING` (opcode 3) yayınlar `[V]` (F4-04 çalışma zamanı, 2026-10-02: `BotWP_K` HP 5650/5650 iken 1440 HP potu `op:3`, çantadan 1 adet düştü).
- **MEC-POT-02** Cooldown yalnızca **pot skill ID'sine özgü** recast'tir. HP ve MP potlarının cooldown'ları **ayrıdır**; farklı kademedeki potların (720 HP ile 1440 HP gibi) cooldown'ları da ayrıdır `[D]` `[V]`.
- **MEC-POT-03** Aynı saniye açığı (MEC-MAG-05) potlarda da geçerlidir.
- **MEC-POT-04** Ölü, sessizleştirilmiş (Silence) veya Kaul iken pot kullanılamaz. No-Potion debuff'ı yalnızca item sınıfı 0 olan **HP** potlarını engeller.
- **MEC-POT-05** Undead debuff'ı heal'i hasara çevirir.

### 6.2 Yerel veri: başlıca HP/MP potları `[V]`

Tümü `ReqLevel = 1`, cast `5`, recast `20` (2,0 sn; "(Store)" HP sürümlerinde 25), moral 1 (kendisi).

| Değer | Item (ID) | MAGIC | `UseItem` | Tüketiliyor mu |
|---|---|---|---|---|
| 360 HP | Water of grace `389013000` | 490013 | **0** | **Hayır** |
| 360 HP | Water of grace(Store) `389062000` | 490062 | **0** | **Hayır** |
| **720 HP** | Water of favors `389014000` | 490014 | **0** | **Hayır** |
| 720 HP | Water of favors(Store) `389063000` | 490063 | 389063000 | Evet |
| 1440 HP | Water of bless `389015000` | 490015 | 389015000 | Evet |
| 960 MP | Potion of wisdom `389019000` / `389081000` | 490019 / 490081 | **0** | **Hayır** |
| **1920 MP** | Potion of soul `389020000` / `389082000` | 490020 / 490082 | **0** | **Hayır** |
| 1920 MP | Potion of soul(Coloseum Event) `910010000` | 500010 | 910010000 | Evet |
| 2160 MP | Potion of Ancient Spirit `389220000` | 490701 | 389220000 | Evet |

Tam liste: `appendix/data/potions_hp_mp.csv`. Kaynak tablo yorumu ve pot dışı tüketilebilirler [11](11_RESOURCE_POTION_AND_SURVIVAL_MANAGEMENT.md)'dedir.

- **MB-01 (kritik veri anomalisi)** 360/720 HP ve 960/1920 MP NPC sürümlerinin MAGIC satırında `UseItem = 0`. Sunucu bu potları **ne kontrol ediyor ne de tüketiyor**. Bu yüzden ilgili skill ID'si gönderilerek envanterde item olmasa bile sınırsız kullanılabilir `[V]`. Durum `MAGIC_BAK_etc` yedeğinde de aynı; yani Etc düzeltmesinden kaynaklanmıyor, özgün veride böyle. Gerçek istemcinin bu potları envanterde yokken kullanmaya izin verip vermediği `[A]`. **Karar (K-5, ADR-0009):** Veri olduğu gibi kalır; kural insan ve bot için aynıdır. **Bot kuralı (CLI-06):** Bot yalnızca envanterinde en az bir adet bulunan potu kullanır (gerçek istemcinin davranışı varsayımı `[A]`, T-MECH-POT-05). Ayrıntı: [11](11_RESOURCE_POTION_AND_SURVIVAL_MANAGEMENT.md).

## 7. Hasar ve savunma formülleri `[D]`

| Kimlik | Kural | Kaynak |
|---|---|---|
| MEC-DMG-01 | Fiziksel temel: `B = 2·AP·saldırıOranı/(AC+240)`; R isabetinde `0,85B + 0,3·rand(0,B)` | [`GameServer/Unit.cpp:208-394`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/Unit.cpp#L208-L394) |
| MEC-DMG-02 | **Oyuncuya fiziksel hasar /2** | [`GameServer/Unit.cpp:383-386`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/Unit.cpp#L383-L386) |
| MEC-DMG-03 | Büyü: `230·hit·CHA/186/(R+250)` + rastgele − büyü gücü; **oyuncuya /3** | [`GameServer/MagicInstance.cpp:2558-2717`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L2558-L2717) |
| MEC-DMG-04 | Hava durumu bonusu hesaplanıyor ama sonucu kullanılmıyor | [`GameServer/MagicInstance.cpp:2707`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L2707) |
| MEC-DMG-05 | İsabet tablosu: isabet/kaçınma oranı ≥5 → %98, ≥3 → %96, ≥2 → %94, ≥1,25 → %92, ≥0,8 → %90, ≥0,5 → %80, ≥0,33 → %70, ≥0,2 → %60, altı → %50 | [`GameServer/Unit.cpp:718-804`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/Unit.cpp#L718-L804) |
| MEC-DMG-06 | Hasar üst sınırı 32000 | [`GameServer/Define.h:23`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/Define.h#L23) |
| MEC-DMG-07 | Mana absorb, Minak's Thorn (party'ye yansıtma), master pasifleri `HpChange` içinde uygulanır | [`GameServer/User.cpp:1860-1983`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L1860-L1983) |

## 8. Ölüm, respawn ve diriltme

| Kimlik | Kural | Kaynak | Etiket |
|---|---|---|---|
| MEC-DTH-01 | Ölümde tüm DoT/HoT ve buff/debuff'lar temizlenir; scroll kayıtları saklanır | [`GameServer/User.cpp:4727-4746`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L4727-L4746) | `[D]` |
| MEC-DTH-02 | Ronark'ta PvP ölümünde **EXP kaybı yok** | [`GameServer/User.cpp:4873-4881`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L4873-L4881) | `[D]` |
| MEC-DTH-03 | NP: tek öldüren +64, ölen −50 (ini varsayılanı, yerel ini aynı). Party ile öldürmede her **canlı** üye (mesafe kontrolü yok) 8 kişilik party'de 22 NP alır. Monument'ı tutan ulusa öldürme başına +5. | [`GameServer/GameServerDlg.cpp:310-311`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/GameServerDlg.cpp#L310-L311), `GameServer/User.cpp:3068-3184,642-643` | `[D]` `[V]` |
| MEC-DTH-04 | Öldüren (veya party) ölenin altınının %40'ını alır; ölen %50 kaybeder | [`GameServer/User.cpp:4160-4217`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L4160-L4217) | `[D]` |
| MEC-DTH-05 | Respawn istemcinin `WIZ_REGENE` isteğiyle olur; tip 2 (taşla) da şehre/bind'e gönderir, yerinde diriltmez | [`GameServer/AttackHandler.cpp:95-240`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/AttackHandler.cpp#L95-L240) | `[D]` |
| MEC-DTH-06 | Ronark respawn: bind nesnesi yoksa START_POSITION zone 71: **Karus (1380, 1090)**, **El Morad (630, 920)**, +rand(0..10) her eksende | yerel `START_POSITION` | `[V]` |
| MEC-DTH-07 | Respawn'da HP doldurulur, **MP doldurulmaz**; buff'lar sıfırlanır, scroll'lar yeniden uygulanır | [`GameServer/AttackHandler.cpp:223-228`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/AttackHandler.cpp#L223-L228) | `[D]` |
| MEC-DTH-08 | **Ronark'ta respawn sonrası blink (dokunulmazlık) yok**: `BlinkStart` rakip ulusa saldırılabilen zone'da hemen döner. Diğer zone'larda 15 sn; Debug derlemede tamamen kapalı. | [`GameServer/User.cpp:4525-4535`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L4525-L4535); [`GameServer/Define.h:61`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/Define.h#L61); [`GameServer/stdafx.h:7-9`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/stdafx.h#L7-L9) | `[D]` |
| MEC-DTH-09 | NP 0 iken Ronark'ta respawn olan karakter ulus ana zone'una atılır | [`GameServer/AttackHandler.cpp:231-239`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/AttackHandler.cpp#L231-L239) | `[D]` |
| MEC-DTH-10 | `/town` (Home) HP ≥ %50, Kaul değil, freeze değil iken çalışır | [`GameServer/User.cpp:3695-3722`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L3695-L3722) | `[D]` |
| MEC-DTH-11 | Ölüm, öldüreni ölenin 300 sn'lik "rival"i yapar (rival öldürme +150 NP) | [`GameServer/User.cpp:4836-4886`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L4836-L4886) | `[D]` |

**Not (MEC-DTH-08'in sonucu):** Ronark'ta respawn olan bot, respawn noktasında hemen saldırıya açıktır. Respawn noktası kendi ulusunun guard tower halkasının içindedir (§12.3); bu, sunucunun sağladığı tek "koruma"dır `[V]`/`[I]`.

## 9. Hareket ve konum

| Kimlik | Kural | Kaynak | Etiket |
|---|---|---|---|
| MEC-MOV-01 | `WIZ_MOVE`: `u16 x·10, z·10, y·10, i16 speed, u8 echo` | [`GameServer/CharacterMovementHandler.cpp:4-54`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/CharacterMovementHandler.cpp#L4-L54) | `[D]` |
| MEC-MOV-02 | Hız kontrolü yalnızca **istemcinin bildirdiği hız alanı**: warrior/mage/priest için > 67 ise koparma + sunucu duyurusu | [`GameServer/User.cpp:2862-2881`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L2862-L2881) | `[D]` |
| MEC-MOV-03 | Konum kontrolü yalnızca harita sınırı (`IsValidPosition`); **yürünebilirlik, yükseklik, çarpışma, mesafe/zaman kontrolü yok** | [`shared/SMDFile.cpp:194-198`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/SMDFile.cpp#L194-L198) | `[D]` |
| MEC-MOV-04 | Tek mesafe kontrolü istemcinin gönderdiği `WIZ_SPEEDHACK_CHECK` ile: son kontrolden bu yana mesafe² /100 ≥ hız+10 ise geri ışınlama. Kontrolün sıklığı istemciye bağlı | [`GameServer/User.cpp:3581-3607`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L3581-L3607) | `[D]` `[A]` |
| MEC-MOV-05 | Hareket gizliliği kaldırır; bölge değişiminde in/out paketleri gönderilir | | `[D]` |
| MEC-MOV-06 | `WIZ_WARP` yalnızca GM'den kabul edilir | [`GameServer/User.cpp:313-316`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L313-L316) | `[D]` |
| MEC-MOV-07 | Zone değişimi party'den çıkarır; tamamlanması istemcinin "Loaded" onayına bağlı | `GameServer/CharacterMovementHandler.cpp:309-500,668-699` | `[D]` |
| MEC-MOV-08 | Ronark'a giriş: seviye 35–80, NP > 0, savaş açık değil (Ardream tipi savaş hariç); ulus kısıtı yok | [`GameServer/CharacterMovementHandler.cpp:260-273`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/CharacterMovementHandler.cpp#L260-L273) | `[D]` |

Navigasyonun ayrıntısı [12](12_NAVIGATION_AND_POSITIONING.md)'dedir. **Sonuç:** Sunucu duvardan geçen bir hareketi reddetmez; bu nedenle botun yürünebilirlik kuralını kendisinin uygulaması zorunludur (CLI-08).

## 10. Party

| Kimlik | Kural | Kaynak |
|---|---|---|
| MEC-PTY-01 | En fazla 8 üye; slot 0 lider; üyeler oturum kimliğiyle tutulur | [`shared/database/structs.h:291-312`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/database/structs.h#L291-L312) |
| MEC-PTY-02 | Davet: hedef party'de olmamalı, aynı ulus (Moradon/Forgotten Temple hariç), aynı zone, seviye `[2/3·L, 1,5·L]` veya ±8 | [`GameServer/PartyHandler.cpp:85-166`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/PartyHandler.cpp#L85-L166) |
| MEC-PTY-03 | Kabul `PARTY_PERMIT 1`, ret 0; liderlik devri yalnızca liderden; lider ayrılırsa party silinir; tek kişi kalırsa party silinir | [`GameServer/PartyHandler.cpp:52-443`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/PartyHandler.cpp#L52-L443) |
| MEC-PTY-04 | Party üyelerine HP/MP güncellemesi (`PARTY_HPCHANGE`) yayınlanır | [`GameServer/User.cpp:2023-2031`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L2023-L2031) |
| MEC-PTY-05 | Zone değişimi party'den çıkarır | MEC-MOV-07 |

**MEC-PTY-03'ün sonucu:** Lider ayrılır veya kopar ise party **silinir**. "Lider ölünce yeni lider" senaryosunda ölüm party'yi silmez (ölüm ayrılma değildir) `[D]`/`[I]`; lider değişimi bot katmanında `PartyPromote` ile yapılmalıdır ([09](09_PARTY_COORDINATION_AND_TARGET_SELECTION.md)).

## 11. Chat

| Kimlik | Kural | Kaynak |
|---|---|---|
| MEC-CHT-01 | Party chat paketi: `WIZ_CHAT [u8 PARTY_CHAT=3][u8 ulus][i16 sid][u8-uzunluk isim][u16-uzunluk mesaj]`, `Send_PartyMember` ile | [`GameServer/ChatHandler.h:8-21`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/ChatHandler.h#L8-L21), `GameServer/ChatHandler.cpp:158,187-193` |
| MEC-CHT-02 | Mesaj uzunluğu 1–128; **oran sınırı veya spam kontrolü yok**; tüm mesajlar chat log dosyasına yazılır | [`GameServer/ChatHandler.cpp:89-276`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/ChatHandler.cpp#L89-L276) |
| MEC-CHT-03 | GM komutları `+` önekiyle, yalnızca GM | `GameServer/ChatHandler.cpp:109-114,344-370` |

## 12. Zone kuralları ve Ronark Land

### 12.1 Düşmanlık `[D]`

- **MEC-ZON-01** Zone 71'de farklı ulus her zaman düşman, aynı ulus hiçbir zaman düşman değildir ([`GameServer/Unit.cpp:1219-1256`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/Unit.cpp#L1219-L1256)). **8 vs 8 testleri Karus – El Morad olarak yapılmalıdır.**
- **MEC-ZON-02** Zone 71 için güvenli alan tanımı yoktur ([`GameServer/Unit.cpp:1311-1345`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/Unit.cpp#L1311-L1345)).
- **MEC-ZON-03** Savaş açıldığında zone 71 boşaltılır; savaş sırasında zone 71'e giriş reddedilir ([`GameServer/GameServerDlg.cpp:2069-2075`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/GameServerDlg.cpp#L2069-L2075), [`GameServer/CharacterSelectionHandler.cpp:177-186`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/CharacterSelectionHandler.cpp#L177-L186)). Test sırasında savaş zamanlayıcıları kapalı olmalıdır ([15](15_TEST_ARENA_SCENARIOS_AND_ACCEPTANCE_CRITERIA.md)).

### 12.2 Harita ve nesneler `[V]`

- Harita: `freezone_a_20050718.smd`, 2048 × 2048 m, 4 m ızgara. Ana oynanabilir alan yaklaşık 1,42 km²; iki ulusun respawn noktaları ve merkez aynı yürünebilir bileşende ([12](12_NAVIGATION_AND_POSITIONING.md)).
- Warp kapıları: El Morad (622, 911), Karus (1375, 1085) (`K_OBJECTPOS` tip 5 ve SMD nesne olayları uyuşuyor).
- Bu haritanın resmî 2005 Colony Zone haritasıyla birebir örtüştüğü dış kaynakla gösterildi `[S]` ([19](19_SOURCES_AND_EVIDENCE.md) W-10).

### 12.3 Guard tower'lar ve NPC'ler `[V]`

| Varlık | Adet | Özellik |
|---|---|---|
| El Morad guard tower (5300, lvl 90) | 18 | Kapı çevresinde ~25–50 m halka; arama menzili 35 m, saldırı menzili 30 m; HP 100 000; magic 300139 |
| El Morad guard tower (5310, lvl 120) | 4 | Kapıya ~10 m; saldırı menzili 20 m; magic 300199 |
| Karus guard tower (5400 / 5410) | 18 / 4 | Aynı düzen, Karus kapısı çevresinde |
| Bifrost Monument (601) | 1 | (1014, 992), HP 700 000 |
| Ulus askeri NPC'leri | El Morad 34, Karus 27 | Karşı ulusa saldırır |
| Canavarlar | 705 birim (212 + 10 boss satırı) | Haritaya yayılmış |

- **MEC-ZON-04** Guard tower'lar oyuncu tarafından saldırılamaz ([`GameServer/Unit.cpp:903-915`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/Unit.cpp#L903-L915)) `[D]`. Tower'ların karşı ulus oyuncularına saldırdığı veriden ve issue #8'den anlaşılıyor `[V]`/`[S]`. AIServer davranışı çalışma zamanında doğrulanmalı `[A]`.
- **Sonuç:** Kapı önü (≈ 85 m yarıçap içi) karşı ulus için tarafsız değildir. Test arenası seçimi [15](15_TEST_ARENA_SCENARIOS_AND_ACCEPTANCE_CRITERIA.md) §2'dedir.

## 13. İstemci tarafı sınırlar (bot adalet kuralları)

Sunucunun **uygulamadığı** ama gerçek istemcinin uyguladığı sınırlar. `BotFairnessGuard` bunları uygular; değerler F1'de gerçek 1453 istemcisi ölçülerek kesinleştirilir (T-MECH-CLIENT-*). Ölçüm yapılana kadar aşağıdaki **muhafazakâr** başlangıç değerleri kullanılır.

| Kimlik | Sınır | Başlangıç değeri | Dayanak | Etiket |
|---|---|---|---|---|
| CLI-01 | Normal saldırı aralığı | `max(silah.Delay/100 sn, 1,0 sn)`; saldırı hızı buff'ı varsa `Delay/hızÇarpanı`; `delaytime` alanı `silah.Delay + 10` olarak gönderilir | 1.298 istemci kodu: aralık = `AttackInterval/100` ÷ saldırı hızı; sunucu MEC-R-04, MEC-R-07 | `[B]` + `[D]`, ölçüm `[A]` |
| CLI-02 | Skill ile normal saldırı arası kilit | **Kilit yok**: R ile skill ayrı zamanlayıcılarda, komşu aksiyon arası en az ~61 ms ölçüldü (§13.2, tek warrior, tek oturum); cast sırasında R yok `[A]`. İlk başlangıç değeri "skill sonrası 0,3 sn R yok" ölçümle **kaldırıldı** (§13.3); botun guard'ında bu kilit kodlanmadı | 1.298 istemci (`m_fDelay`) `[B]` (eski varsayım); §13.2 ölçümü | `[V]` (warrior), `[A]` (priest/mage) |
| CLI-03 | Cast süresi | `CASTING` gönder → `bCastTime·100 ms` bekle → `EFFECTING` gönder; cast sırasında hareket varsa iptal | MAGIC.bCastTime (0,1 sn birimi, potlarda 5 = 0,5 sn) | `[V]` birim `[A]` |
| CLI-04 | Skill recast | `ReCastTime·100 ms` **gerçek zamanla** (saniye yuvarlaması olmadan); aynı tipten ID < 400000 skill'ler arası ≥ 1,0 sn | MEC-MAG-02, MEC-MAG-03 | `[D]` |
| CLI-05 | Hareket hızı, paket sıklığı ve yavaşlatma | Yürüyüş `speed=45` (4,5 m/s), sprint `67` (6,7 m/s), sürekli harekette `WIZ_MOVE` **~1,5 sn'de bir** (p50 1502 ms; §13.2/§13.3, `BotCore::kMovePeriodMs`); hız/yavaşlatma/stun/Wall of Iron buff'larının yüzdesi bot hareketine uygulanır; stun süresince hareket yok. Paket konumu hedef noktayı taşır: iki paket arası ~6,75 m (yürüyüş) / ~10 m (sprint) tek adımdır; ara noktaları sunucu doğrulamaz (bkz. CLI-08 kiriş denetimi, `docs/12` §13) | MEC-BUF-06 | `[V]` (hız, sıklık), `[A]` (yavaşlatma yüzdeleri) |
| CLI-06 | Pot kullanımı | Yalnızca envanterde en az bir adet bulunan pot (tüketilen potlarda sayaç düşer; MB-01 potlarında sayı azalmaz, K-5); **HP ve MP potları için tek ortak 2,5 sn** muhafazakâr cooldown (F4-04: `BotCore::kPotCooldownMs = 2500`, `BotFairnessGuard`; ayrı HP/MP zamanlayıcısı ölçülene kadar ortak `[A]`, T-MECH-POT-03); aynı saniyede tekrar yok | MB-01, MEC-POT-02/03; §13.2 ölçümü (2504–2665 ms, HP→MP 2540 ms); dönem oyuncu ifadesi "720'lik potu 2 sn'de çekiyoruz" `[S]` | `[Ö]` |
| CLI-07 | Alan skill'i hedef noktası | Hedef noktası çağırana `sRange` içinde | MEC-AOE-01 | `[Ö]` |
| CLI-08 | Yürünebilirlik | Bot yalnızca SMD olay ızgarasında yürünebilir hücreler üzerinden, yükseklik farkı sınırına uyarak hareket eder; teleport yok | MEC-MOV-03 | `[Ö]` |
| CLI-09 | Ayakta skill | `UseStanding=1` skill öncesi durma hareketi ve en az bir tick bekleme | MEC-MAG-07 | `[D]` |
| CLI-10 | Hedef HP bilgisi | Gözlem sözleşmesine uyum (§16): bot yalnızca **tek seçili hedefin** HP'sini ister; hedef botun 3×3 bölgesinde (`VIEW_DISTANCE` 48 m) olmalı (`out_of_view`); aynı hedefin yoklaması ≥ 2,0 sn arayla (`poll`; farklı hedefi seçmek serbest); her istek CLI-11 penceresine sayılır; seçimde `echo=1`, yoklamada `echo=0` (F4-06: `BotCore::CheckTargetHp`, `kTargetHpPollMs = 2000`) | `GameServer/User.cpp:356-361` istek, `:2356-2387` cevap (menzil/görüş denetimi yok); Q-18 | `[D]` + `[Ö]`; `[A]` (2,0 sn F1 ölçümü Q-18 p50 2001 ms; bot seçimde tek paket yollar, istemci iki). `[V]` (F4-06 doğrulaması, 2026-10-02): sunucunun `WIZ_TARGET_HP` cevabı botun alıcısına ulaşır (`observed`, gecikme 3–9 µs); cevaptaki HP `list` `hp=` ile aynı (5266/5650); ölü hedefe cevap gelmez (`no_result`); komşu bölge (delta 1) kabul, delta 2 `out_of_view` |
| CLI-11 | Aksiyon hızı tavanı | Bot başına toplam aksiyon ≤ 6/sn (hareket hariç): **kaba emniyet ağıdır**, oyuncuyla eşdeğerlik kanıtı değildir. Ölçülen insan tavanı ≤ 3/sn (§13.2, yalnız warrior R+skill+pot); 6/sn bunun iki katıdır. Adaleti asıl taşıyan **aksiyon başına zamanlayıcılar**dır (CLI-01/03/04/06/13...); üç katman ayrımı §13.4 | §13.2 `ACTIONS max=3` | `[Ö]` (bot sınırı), `[V]` (insan gözlemi, tek karakter) |
| CLI-12 | Speedhack kontrol paketi | Gerçek istemci `WIZ_SPEEDHACK_CHECK` gönderiyorsa bot da aynı sıklıkla göndermeli (sunucu tarafında hiçbir avantaj sağlamaz; tutarlılık için) | MEC-MOV-04 | `[A]` |
| CLI-13 | Duruş geçişi (otur/kalk) | Bot yalnızca durmuşken ve yürüme/saldırı/cast serisi yokken oturur (`WIZ_STATE_CHANGE` tip 1); otururken hareket, saldırı ve cast **başlatmaz** (gerçek istemci önce kalkar; sunucu oturan oyuncunun yürümesini/vurmasını engellemez, ama otururken yürüyüp oturma yenilenmesi alması adaletsizdir); ardışık iki duruş paketi arası ≥ 1,0 sn; her geçiş CLI-11 penceresine sayılır (F4-05: `BotCore::CheckStance`, `kStanceToggleMinMs = 1000`) | `GameServer/User.cpp:2710-2723` `StateChange`, `:2817-2819` yayın, `HPTimeChange` `:3359` | `[V]` sunucu `WIZ_STATE_CHANGE` yayını botun kendi alıcısına gelir ve oturma yenilenmesi işler (F4-05 Doğrulama Tur 1: Karus warrior maxHP 5650, oturunca +296 HP / ~5–6 sn ≈ %5,2, ayakta HP artışı yok); pot `m_bResHpType`'i değiştirmez (sunucu kodu); `[A]` (1,0 sn istemci ölçümü değil, muhafazakâr; gerçek istemcinin pot ile kalkıp kalkmadığı Q-06) |
| CLI-14 | Yeniden doğuş (`Regene`) | Bot yalnızca ölüyken ve ölümünü fark ettiği andan **≥ 3,0 sn sonra** `WIZ_REGENE` tip 1 yollar (`dead_wait`; gerçek istemci ölüm animasyonu ve "yeniden doğ" düğmesi nedeniyle anında basamaz); tip 2 (taş) ve diriltme skill'i kullanılmaz; her istek CLI-11 penceresine sayılır; NP 0 iken `Regene` gönderilmez (`no_np`: sunucu bu oyuncuyu doğuş sonunda ana zone'a ışınlar, KI-013) (F4-07: `BotCore::CheckRegene`, `kRegeneMinDeadMs = 3000`) | `GameServer/User.cpp:334-336` istek, `GameServer/AttackHandler.cpp:100-245` işleyici (bekleme/menzil denetimi yok), MEC-DTH-05..09 | `[D]` + `[Ö]`; `[A]` (3,0 sn ölçülmedi: gerçek istemcinin ölüm → `WIZ_REGENE` gecikmesi `T-REGENE`, `docs/STATUS.md` "Proje sahibi testleri") |
| CLI-15 | Party daveti ve kabulü | Bot yalnızca party'siz iken veya party lideriyken davet eder (`not_leader`); davet edilen hedef botun 3×3 bölgesinde olmalı (`out_of_view`, CLI-10 ile aynı sınır); iki davet arası ≥ 1,0 sn (`invite_gap`); kabul (`PARTY_PERMIT 1`) yalnızca alınmış bir davet varken ve davetten **≥ 1,0 sn sonra** (`no_invite` / `accept_wait`); her davet/kabul CLI-11 penceresine sayılır; davet adı hedefin gerçek karakter adıdır (F4-08: `BotCore::CheckPartyInvite`/`CheckPartyAccept`, `kPartyInviteGapMs = kPartyAcceptMinMs = 1000`) | `GameServer/PartyHandler.cpp:6-50` istek, `:85-166` `PartyRequest` (hız/görüş/kabul gecikmesi denetimi yok; `INSERT` için lider denetimi yok), `:168-266` `PartyInsert`; MEC-PTY-01..03 | `[D]` + `[Ö]`; `[A]` (1,0 sn değerleri ölçülmedi: gerçek istemcinin davet aralığı ve `PARTY_PERMIT` cevap gecikmesi `T-PARTY-01`) |
| CLI-16 | Party reddi ve ayrılma | Bot alınmış bir daveti yalnızca davetten **≥ 1,0 sn sonra** reddeder (`PARTY_PERMIT 0`; `no_invite` / `decline_wait`; gerçek istemci popup'ı okuyup düğmeye basmadan reddedemez); party'den yalnızca party'deyken ve bekleyen davet yokken ayrılır (`PARTY_REMOVE` + kendi oturum kimliği; `not_in_party` / `invite_pending`) ve party'ye girişinden (kendi kurma/kabulü) **≥ 1,0 sn sonra** (`leave_wait`); liderin ayrılması sunucuda party'yi dağıtır (MEC-PTY-03), devir yapmaz; her ret/ayrılma CLI-11 penceresine sayılır (F4-09: `BotCore::CheckPartyDecline`/`CheckPartyLeave`, `kPartyDeclineMinMs = kPartyLeaveMinMs = 1000`) | `GameServer/PartyHandler.cpp:28-33` ret, `:41-43` ayrılma isteği, `:52-83` `PartyCancel` (reddedene cevap yok, lidere `PARTY_INSERT -1`), `:337-407` `PartyRemove`, `:409-444` `PartyDelete`; sunucunun kendi kopma yolu `User.cpp:205-217` (`PartyPromote` + `PartyRemove`); MEC-PTY-03 | `[D]` + `[Ö]`; `[A]` (1,0 sn değerleri ve gerçek istemcinin liderken hangi alt-opcode'u yolladığı ölçülmedi: `T-PARTY-02`) |
| CLI-17 | Party liderlik devri ve atma | Bot yalnızca party lideriyken ve **kendi party'sinin üyesi** olan başka bir oyuncuya devir (`PARTY_PROMOTE` + hedef kimliği) veya atma (`PARTY_REMOVE` + **hedef** kimliği) yapar (`not_leader` / `not_member`; gerçek istemcinin party paneli yalnızca üyeleri listeler ve yalnızca lider devir/atma menüsünü açar); iki devir/atma arası **≥ 1,0 sn** (`manage_gap`, devir ve atma aynı sayaç); her devir/atma CLI-11 penceresine sayılır; hedef atılınca yalnızca lider kalıyorsa sunucu party'yi dağıtır (MEC-PTY-03) (F4-10: `BotCore::CheckPartyManage`, `kPartyManageGapMs = 1000`) | `GameServer/PartyHandler.cpp:34-43` istek, `:268-335` `PartyPromote` (liderlik + aynı party denetimi var, hız denetimi yok), `:337-407` `PartyRemove` (atmada hedefin üyeliği denetlenmez: KI-015), `:409-444` `PartyDelete`; MEC-PTY-03 | `[D]` + `[Ö]`; `[A]` (1,0 sn değeri ölçülmedi: gerçek istemcinin art arda devir/atma aralığı `T-PARTY-03`) |
| CLI-18 | Party chat | Bot yalnızca party'deyken (bekleyen davet yokken) **party kanalına** (`PARTY_CHAT`) yazar; metin 1–128 bayt, yazdırılabilir ASCII (0x20–0x7E) ve `+` ile başlamaz (`bad_text`; sunucu karakter denetimi yapmaz, `+` GM komut önekidir, MEC-CHT-03); iki mesaj arası **≥ 4,0 sn** (`chat_gap`), aynı metin **8,0 sn** içinde tekrar yok (`chat_dup`), dakikada **≤ 6** mesaj (`chat_minute`); her mesaj CLI-11 penceresine sayılır (F4-11: `BotCore::CheckChat`, `kChatGapMs = 4000`, `kChatDupMs = 8000`, `kChatPerMinute = 6`) | `GameServer/ChatHandler.cpp:92-278` (`:104` susturma/hapis, `:108` uzunluk, `:190-195` `PARTY_CHAT` → `Send_PartyMember`; oran sınırı yok: MEC-CHT-02); `docs/09` §12, `docs/07` `P-PRI-CHAT-RATE` | `[D]` + `[Ö]` (4 sn / 8 sn / 6 dk tasarım sınırları, insan ölçümü değil); ASCII sınırı geçici, `[A]` (Q-16 / T-PTY-09) |
| CLI-19 | Bölge değişiminde kullanıcı bilgisi isteği | Bot yalnızca sunucunun son `WIZ_REGIONCHANGE` ile bildirdiği ve gözlem tablosunda olmayan kimlikleri (botun kendi kimliği hariç) `WIZ_REQ_USERIN` ile ister; istek başına **≤ 32** kimlik (`bad_count`), iki istek arası **≥ 1,0 sn** (`gap`: ihlal sayılmaz, kimlikler bekler, olay yazılmaz); keyfi kimlik istenmez; bu istek oyuncu aksiyonu değil istemcinin otomatik ağ trafiğidir, **CLI-11 penceresine sayılmaz**; cevaba (`WIZ_REQ_USERIN`, her zaman gelir) göre sonuç `received`/`no_result` (F4-13: `BotCore::CheckUserIn`, `PendingIds`, `kUserInMaxIds = 32`, `kUserInMinGapMs = 1000`) | `GameServer/User.cpp:1230-1253` `RequestUserIn` (menzil/hız denetimi yok, sayı > 1000 → 1000, oyunda olmayan kimlikler atlanır), `GameServer/GameServerDlg.cpp:1340-1358` `RegionUserInOutForMe`; `docs/03` §16 | `[D]` + `[A]` (gerçek istemcinin bu paketi hangi aralık ve toplulukla yolladığı ölçülmedi: `T-PERC-01`; 1,0 sn / 32 muhafazakâr tasarım değerleri) |
| CLI-20 | Bölge değişiminde NPC bilgisi isteği | Bot yalnızca sunucunun son `WIZ_NPC_REGION` ile bildirdiği ve NPC gözlem tablosunda olmayan kimlikleri `WIZ_REQ_NPCIN` ile ister; istek başına **≤ 32** kimlik (`bad_count`), iki istek arası **≥ 1,0 sn** (`gap`: ihlal sayılmaz, kimlikler bekler, olay yazılmaz; CLI-19'dan bağımsız sayaç); keyfi kimlik istenmez; bu istek oyuncu aksiyonu değil istemcinin otomatik ağ trafiğidir, **CLI-11 penceresine sayılmaz**; cevaba (`WIZ_REQ_NPCIN`; başlıktaki sayı *istenen* sayıdır, kayıtlar yalnızca bulunan canlı NPC'ler için gelir) göre sonuç `received`/`no_result` (F4-15: `BotCore::CheckNpcIn`, `PendingIds`, `kNpcInMaxIds = 32`, `kNpcInMinGapMs = 1000`) | `GameServer/User.cpp:1260-1298` `RequestNpcIn` (menzil/hız denetimi yok, sayı > 1000 → 1000, ölü/yok kimlikler atlanır, `m_bPointCheckFlag` kapalıysa cevapsız), `GameServer/GameServerDlg.cpp:1591-1614` `GetRegionNpcList`; `docs/03` §16 | `[D]` + `[A]` (gerçek istemcinin bu paketi hangi aralık ve toplulukla yolladığı ölçülmedi: `T-PERC-01` eki; 1,0 sn / 32 muhafazakâr tasarım değerleri) |

### 13.1 "Skill + R" kombosu

Dönem kaynakları kombonun özünü şöyle tarif ediyor: normal saldırı ile skill istemcide ayrı zamanlayıcılarla çalışır ve hareket tuşu skill animasyonunu keser `[S]`/`[B]`. Bu depodaki sunucu tarafında:

1. R ve skill arasında zamanlama bağı **yoktur** (MEC-MAG-06).
2. R saniyede bir ile sınırlıdır (MEC-R-07); silah gecikmesini ise yalnızca istemci uygular (MEC-R-04, CLI-01).
3. Type1 skill'ler kendi recast'leri (çoğu 0,5 sn sınıfında, bkz. [05](05_SKILL_CATALOG_AND_COMBAT_RULES.md)) ve tip kapısı (saniyede bir Type1) ile sınırlıdır.

**Bot için kural:** Bot, "skill + R" desenini **ayrı ayrı geçerli** iki aksiyonun aynı pencerede gönderilmesi olarak uygular. R, CLI-01 ve CLI-02'ye uymalıdır. Skill CLI-03 ve CLI-04'e uymalıdır. Animasyon iptali bir istemci görsel davranışıdır; sunucu açısından bot ek bir şey yapmaz. Saniyede bir R ve saniyede bir Type1 skill, warrior için **sunucunun izin verdiği en yüksek tempo** olarak kabul edilir ve bot bunu aşamaz. Gerçek istemcinin bu tempoya ulaşıp ulaşmadığı F1'de ölçülür (T-MECH-CLIENT-01).

**İstemci animasyonu ile sunucu kabulü arasındaki fark:** İstemcide görülen vuruş animasyonu sunucunun kabul ettiği saldırıyı göstermez. Örneğin aynı saniyedeki ikinci R, istemcide animasyon oynatsa da sunucuda `CanCastRHit` nedeniyle sessizce düşer. Telemetri, gönderilen ile kabul edilen aksiyonu ayrı sayar ([16](16_TELEMETRY_DEBUGGING_AND_PERFORMANCE.md) MET-ACT-02).

### 13.2 Ölçülen istemci değerleri (F1-02 oturumu, 2026-10-02) `[V]` (tek karakter, tek oturum)

Kaynak: `tools/trace-session.sh collect war-r` kaydı (sunucuya varış zamanı, yerel ağ; 153 paket, tek warrior, silah gecikmesi 164 olan bir ağır silah, çoğunlukla R). Tek oturum olduğu için değerler **ön bulgudur**; diğer silahlar ve sınıflarla tekrarlanana kadar CLI tablosundaki `[A]` etiketi tamamen kalkmaz.

| Kimlik | Ölçüm | Sonuç | Yorum |
|---|---|---|---|
| CLI-01 | R aralığı | 27 vuruşta aralık **1640–1644 ms** (p50 1642) | Silah `Delay=164` ile **birebir**: aralık = `Delay × 10 ms` (+0–4 ms gecikme). Sunucunun 1 sn tavanının (MEC-R-07) çok üstünde, istemci kendi sınırını uyguluyor |
| CLI-01 | `delaytime` alanı | 174 (27/27) | `Delay + 10` formülü **doğrulandı** (`164 + 10`) |
| T-MECH-CLIENT-02 | `distance` alanı | 19 (27/27); silah `Range = 20` | Mesafe değeri sabit çıktı; ölçeği (hedefe gerçek mesafe mi, silah menzili mi) henüz belirsiz `[A]`: farklı mesafelerden vurularak ayrıca ölçülecek |
| T-MECH-CLIENT-02 | `type` / `result` | 1 / 1 (27/27) | Normal saldırı |
| CLI-12 | `WIZ_SPEEDHACK_CHECK` sıklığı | **10,0 sn** (p50 10002 ms, yük 5 bayt: `u8 bayrak, f32` istemci saniyesi, her pakette +10,0) | Bot aynı sıklıkta ve aynı yük düzeniyle gönderebilir |
| Q-18 | `WIZ_TARGET_HP` isteği | Hedef seçiliyken **2,0 sn**'de bir (p50 2001 ms, `echo=0`); hedef seçildiği anda 1 ms arayla iki paket (`echo=1` ve `echo=0`) | Saldırıdan bağımsız, hedef seçiliyken sürekli |
| CLI-05 / Q-02 | `WIZ_MOVE` | Hareket sırasında ~1,5 sn'de bir paket; `speed` alanı koşuda **67** (yerinde 0); hesaplanan koşu hızı medyan **6,75 m/s** (≈ alan/10) | Hız alanı 0,1 m/s birimi. `p95=88,9 m/s` sıçramalar (ışınlanma/zone) kaynaklı; temiz koşu ölçümü `war-move` etiketiyle yapılacak |
| CLI-04 | Type1 skill tekrar aralığı (106560 sword dancing, veri `ReCastTime=5` = 0,5 sn) | 15 kullanımda **1303–1470 ms** (p50 1382), `war-skill` etiketi | İstemci, veri sınırının ~2,6 katı yavaş gönderiyor. Oyuncunun bastığı tempo mu, istemcinin kendi sınırı mı ayrılamadı `[A]`; farklı skill'lerle tekrar ölçülecek |
| CLI-06 | HP (490014) ve MP (490020) pot aralığı (veri `ReCastTime=20` = 2,0 sn) | Her biri 9 kullanımda **2504–2665 ms** (p50 2570 HP, 2559 MP); HP→MP geçişi 2540 ms, `pot` etiketi, oyuncu en hızlı tempoda bastı | İstemci HP ve MP potlarını **~2,5 sn'de bir** gönderiyor; veri sınırı (2,0 sn) altına inilmiyor. HP/MP ortak bekleme süresi muhtemel (geçiş de 2,5 sn) ama ayrı kanıtlanmadı `[A]` |
| CLI-05 | `WIZ_MOVE` hız alanı, ikinci kayıt | Yürüyüşte `speed=45` (11 paket), hareket hızı ≈ 5,5 m/s | `speed=67` yalnızca `war-r` kaydında (orada `sprint` skill'i 106001 kullanılmıştı): hız buff'ı olabilir `[A]`; `war-move` ile ayrı ölçülecek |

**Cast zamanlaması ve iptal (priest/mage oturumu, 2026-10-02) `[V]` (tek karakter/sınıf, 4 kayıt):**

| Kimlik | Ölçüm | Sonuç | Yorum |
|---|---|---|---|
| CLI-03 | Heal 112545 Superior healing (`CastTime=15`, kendi üzerine) CASTING → EFFECTING | 20 kullanımda **1565–1572 ms** (p50 1569) | Cast süresi `CastTime×100 ms` + **~70 ms**; `sData` hep 0, `caster=target=kendi` |
| CLI-03 | Mage 110518 Ignition (`CastTime=10`, tek hedef) CASTING → EFFECTING | 33 kullanımda **1089–1095 ms** (p50 1091) | `CastTime×100 ms` + **~90 ms**; CASTING'te `target` = canavar kimliği; EFFECTING'te `sData[0..2]` = hedef konumu |
| CLI-04 | Ardışık cast aralığı (EFFECTING → sonraki CASTING) | Heal: en kısa **1704 ms** (cast 1565 + ~139); Ignition: en kısa **1228 ms** (cast 1091 + ~137) | İki skill'de de bir önceki EFFECTING'ten sonra **~135–140 ms** boşluk; veri `ReCastTime=1` (0,1 sn) bunun çok altında. Bot döngü süresi ≥ `CastTime×100 + ~70 + ~140 ms` olmalı |
| CLI-03 | Uçan alan büyüsü 110533 Fire burst (`CastTime=15`, alan, `target=-1`, hedef noktası `sData[0..2]`) | CASTING → **FLYING (opcode 2) +1539 ms** → **EFFECTING +2576 ms** (n=3); EFFECTING ile aynı ms'de `opcode 4`, `sData[3]=-101` | Uçan skill'de istemci EFFECTING'i mermi hedefe varınca gönderiyor (~1,0 sn uçuş); sıra 1 → 2 → 3; tek örnek, uçuş süresi mesafeye bağlı olabilir `[A]` |
| CLI-03 | Cast iptali (yürüyerek) | İstemci **`MAGIC_PROCESS opcode 4` (MAGIC_FAIL), `sData[3] = -100`** gönderiyor, **MOVE'dan 5–8 ms önce**; `opcode 6` (CANCEL) **yok** | `opcode 6` buff iptali içindir; sunucu gelen `MAGIC_FAIL`'i yalnızca iletir (`MagicInstance.cpp:40-43`, `:157`), cast durumu tutmaz. İptal anı oyuncunun seçimi: CASTING'ten sonra priest 1140–1408 ms, mage 786–883 ms (n=12 / 11), ölçülen bir alt sınır yok |
| CLI-05 / Q-02 | Yürüyüş hızı (sprint yok) | `speed=45` (32/34 paket), `run_speed` median **4,51 m/s** (p95 5,65); sürekli yürürken paket **~1,5 sn**'de bir (p50 1502 ms); durma paketi `speed=0`, `echo=0` (hareket `echo=3`) | `speed` alanı 0,1 m/s birimi doğrulandı (45 → 4,5 m/s; sprint 67 → 6,7 m/s). İlk `WIZ_MOVE` **hedef noktayı** taşır (durma/başlama paketlerinde konum zıplaması, hesaplanan 58 m/s sahte), bu yüzden hız yalnızca ardışık periyodik paketlerden hesaplanmalı |
| CLI-05 / Q-02 | Yürüyüş ve sprint hızı, yeniden ölçüm (2026-10-03, Karus ve El Morad W-G, `/town` → arena yürüyüşü, 50–110 paket) | `speed=45` → **4,49–4,54 m/s**, `speed=67` (Sprint `206001`) → **6,80 m/s**; paket aralığı ikisinde de p50 **1502 ms** (sprint paket sıklığını değiştirmez); `speed=-15` tek paket (başlangıç/dönüş anı, gözardı) | Önceki ölçümle tutarlı, `[V]` pekişti. İnsan rotası ızgara A* yolundan Karus'ta ~%24, El Morad'da ~%8 uzun (`docs/15` §2.4.1) |
| CLI-12 | `WIZ_SPEEDHACK_CHECK` | Yine **10,0 sn** (p50 10001–10003 ms) dört kayıtta | Üç oturumda tutarlı |
| Q-18 | `WIZ_TARGET_HP` | Hedef seçiliyken **2,0 sn** (`msg-cancel`: p50 2001 ms); `pri-cast` (hedefsiz) 0 | Önceki bulguyla tutarlı |

**Warrior kombo ve hızlı R oturumu (2026-10-02) `[V]` (tek karakter, 3 kayıt):**

| Kimlik | Ölçüm | Sonuç | Yorum |
|---|---|---|---|
| CLI-01 | Hızlı R (`war-r-fast`, 39 vuruş) ve kombo R'ler | Aralık **en az 1640 ms**, p50 1642; hızlı basış aralığı kısaltmıyor | R aralığı istemci tarafında **silah `Delay × 10 ms`**'ye kilitli (`Delay=164`); bot için gerçek-zaman alt sınırı bu |
| T-MECH-CLIENT-02 | `WIZ_ATTACK.distance` | 19 (hedef silah menzilinde), 10, 7, 2 (yaklaştıkça) | Alan **hedefe mesafe × 10 (0,1 m)**; silah `Range=20` = 2,0 m; `delaytime=174` (= `Delay+10`) ve `type=1, result=1` sabit |
| CLI-02 | Skill ile R arası kilit (`war-combo`, 21 R, 36 skill, potlar çıkarıldı) | Skill → sonraki R **en az 61 ms** (p5 165–215); R → sonraki skill **en az 62 ms** (p5 ~208) | **Karşılıklı kilit yok**; R ve skill bağımsız zamanlayıcılarda. `docs/03` §13'teki "skill sonrası 0,3 sn R yok" başlangıç değeri **gereksiz** (aşağıdaki özete bak) |
| CLI-04 | Type1 skill 106560 sword dancing, kombo içinde | Ardışık aralık **en az 889 ms**, çoğu 0,97–1,07 sn | Saf tekrarda (`war-skill`) en az 1303 ms görülmüştü: alt sınır skill'e değil oyuncunun basış temposuna bağlı görünüyor; istemci alt sınırı ≤ 889 ms. Sunucu tip kapısı saniyede 1 (MEC-MAG-03) |
| CLI-04 | Type1 skill 106557 sword aura (`war-skill2`, 20 kullanım) | **1317–1680 ms** (p50 1394) | Saf tekrar tempoları 1,3–1,4 sn |
| CLI-06 | Pot kombo/hızlı R sırasında | HP pot en kısa **2510–2530 ms**, HP+MP karışık en kısa 2629 ms | `pot` kaydıyla (2504 ms) tutarlı: **~2,5 sn** ortak bekleme |
| CLI-11 | Aksiyon hızı (R + skill + pot) | Saniyede en fazla **3** aksiyon (`ACTIONS max=3`) | İnsan tavanı ~3/sn; planlanan bot tavanı 6/sn rahat |
| CLI-05 | Sprint (`106001`) | Sprint sonrası `speed=67` (13 paket); yürüyüş `speed=45` | Sprint süresince 6,7 m/s, aksi 4,5 m/s |

**Paket düzeni düzeltmesi (§14):** Gerçek istemci `WIZ_MAGIC_PROCESS` paketini **21 bayt** gönderiyor (`u8 opcode, u32 skill, i16 caster, i16 target, i16 data[6]`); sunucu 7. alanı okuyamadığı için 0 sayıyor (`shared/ByteBuffer.h:110-116`, taşan okuma `0` döner). Pot ve skill paketlerinde bu oturumda yalnızca `opcode 3` (EFFECTING) görüldü, `CASTING` (opcode 1) yok; `caster` ve `target` 0 idi. Cast süreli skill'lerin `CASTING → EFFECTING` zamanlaması (CLI-03) hâlâ ölçülmedi.

### 13.3 Ölçülmüş CLI özeti (F1 insan oturumları, 2026-10-02) `[V]`

Bot adalet korumasının (F4) kullanacağı **ölçülmüş** değerler; tek karakter/sınıf, dört oturum (ayrıntı §13.2). Muhafazakâr başlangıç değerleri (§13 tablosu) bununla güncellenir:

| Kimlik | Ölçülmüş kural | Başlangıç değerine göre |
|---|---|---|
| CLI-01 | R aralığı = `silah.Delay × 10 ms` gerçek zamanda (alt sınır), `delaytime = Delay + 10`, `distance` = hedefe mesafe × 10 | Doğrulandı (başlangıç formülüyle aynı) |
| CLI-02 | **R ile skill arasında kilit yok**; her biri kendi zamanlayıcısında (en az ~60 ms komşu aksiyon) | **"Skill sonrası 0,3 sn R yok" kaldırılır** |
| CLI-03 | Cast skill'inde CASTING → EFFECTING = `CastTime × 100 ms + 70–90 ms`; uçan alan büyüsünde CASTING → FLYING `+cast`, EFFECTING mermi varınca (~1 sn uçuş) | Doğrulandı + 70–90 ms eklenir; uçuş fazı eklenir |
| CLI-04 | Cast skill döngüsü `CastTime×100 + ~70 + ~140 ms`; Type1 skill alt sınır ~0,9–1,0 sn, saf tekrar tempo ~1,3–1,4 sn; sunucu tip kapısı ≥ 1 sn | `ReCastTime` (0,1 sn) tek başına yeterli değil; istemci tempoları ekleniyor |
| CLI-05 | Yürüyüş `speed=45` (4,5 m/s); sprint `speed=67` (6,7 m/s); sürekli hareketli periyodik konum paketi ~1,5 sn'de bir; durma `speed=0` | Ölçüldü; `[A]` kalktı |
| CLI-06 | HP ve MP potları ortak **~2,5 sn** bekleme | 2,0 sn başlangıç değeri **2,5 sn'ye çıkar** |
| CLI-11 | Gözlenen insan tavanı saniyede 3 aksiyon | 6/sn güvenlik tavanı korunur (insanı aşmaz) |
| CLI-12 | `WIZ_SPEEDHACK_CHECK` her **10,0 sn**, yük `u8 bayrak + f32` istemci saniyesi | Ölçüldü |
| Cast iptali | `MAGIC_PROCESS opcode 4` (MAGIC_FAIL) `sData[3] = -100`, **hareketten 5–8 ms önce** | Yeni kural: bot cast'i iptal ederken önce opcode 4 gönderir |
| Q-18 | `WIZ_TARGET_HP` hedef seçiliyken 2,0 sn'de bir | Ölçüldü |

Kapsam sınırı: Tek karakter, tek silah (`Delay=164`), tek oturum günü. Farklı silah gecikmeleri ve ek skill'ler için `Delay × 10 ms` kuralı ve skill tempoları ikinci bir karakterle doğrulanabilir.

### 13.4 Üç katman: ölçülmüş istemci / sunucu kuralı / bot sınırı (değerlendirme 2026-10-02)

Aynı mekanik için üç ayrı şey vardır ve karıştırılmaz: (1) **ölçülmüş istemci davranışı** (insan oyuncunun gerçekte yaptığı, `[V]`, tek karakter/oturum), (2) **sunucu kuralı** (sunucunun kabul ettiği alt/üst sınır, `[D]`), (3) **ihtiyatlı bot sınırı** (`BotFairnessGuard`'ın kodladığı değer, `[Ö]`/`[A]`). Bot sınırı sunucu kuralından değil, ölçülmüş istemciden ve bir payla türetilir. **Tek bir toplam aksiyon sınırı (CLI-11) skill/R/pot zamanlamalarının oyuncuyla eşdeğer olduğunu kanıtlamaz**; eşdeğerlik aksiyon başına satırlarla gösterilir. Yeni bir CLI değeri eklenirken üç sütun da doldurulur; bot sınırı ölçülmüş değerin altına inemez (altına iniyorsa `[A]` ve gerekçe yazılır).

| Mekanik | Ölçülmüş istemci `[V]` | Sunucu kuralı `[D]` | Bot sınırı (kodda) | Not |
|---|---|---|---|---|
| Normal saldırı (R) aralığı | `Delay × 10 ms` (Delay 164 → 1640–1644 ms) | ≤ 1 başarılı R/sn, menzil (MEC-R-04/07) | `max(Delay×10, 1000 ms)` (`kRMinIntervalMs`, `AttackIntervalMs`) | Tek silah ölçüldü |
| Skill ↔ R | **Kilit yok**, komşu aksiyon ≥ 61/62 ms | Bağ yok (MEC-MAG-06) | **Kilit uygulanmaz**; yalnız CLI-11 toplamı | 0,3 sn varsayımı kaldırıldı; priest/mage için ölçüm yok `[A]` |
| Type1 skill tekrarı | kombo içi ≥ 889 ms, saf tekrar 1,3–1,4 sn | tip kapısı ≥ 1 sn + skill recast (MEC-MAG-03) | `kTypeGateMs 1000` + recast | Oyuncu temposu mu istemci sınırı mı ayrılamadı `[A]` |
| Cast döngüsü | `CastTime×100 + 70–90 ms`; ardışık +~135–140 ms | MP EFFECTING'de, cast durumu tutulmaz | `kCastExtraMs 80`, `kCastGapMs 140` | Uçan skill: FLYING fazı (§13.2) |
| Pot (HP/MP) | ~2,5 sn (2504–2665 ms; HP→MP 2540 ms) | pot skill'i başına recast 2,0 sn (Store HP 2,5), HP/MP ayrı | **ortak** 2500 ms (`kPotCooldownMs`) | Kapasite tek sayaçla 1920/2,5 = 768 MP/sn; ayrı sayaç kanıtı T-MECH-POT-03 |
| Hareket paketi | ~1,5 sn (p50 1502 ms), yürüyüş 45 / sprint 67 | hız alanı ≤ 67 (`SpeedHackUser`), harita sınırı | `kMovePeriodMs 1500`, adım ≤ hız×süre×1,10+0,15 m | Yürünebilirliği sunucu denetlemez (CLI-08, F5) |
| Hedef HP isteği | 2,0 sn (p50 2001 ms) | sınır yok | `kTargetHpPollMs 2000` | CLI-10 |
| Respawn (`WIZ_REGENE`) | ölçülmedi (T-REGENE-01 bekliyor) | bekleme/menzil denetimi yok | `kRegeneMinDeadMs 3000` `[A]` | "Hemen respawn" = bu bekleme sonrası |
| Toplam aksiyon hızı | ≤ 3/sn (yalnız warrior R+skill+pot) | tek toplam sınır yok; mekanik başına sınırlar (R 1/sn, Type1 1/sn, pot recast) ≈ 3–4/sn üst | 6/sn toplam (`kMaxActionsPerWindow`) | Kaba emniyet ağı; priest/mage ölçümü Q-25 |

## 14. Bot aksiyonları için paketler `[D]`

| Aksiyon | Opcode | Payload | Kaynak |
|---|---|---|---|
| Hareket | `WIZ_MOVE 0x06` | `u16 x·10, u16 z·10, u16 y·10, i16 speed, u8 echo` | [`GameServer/CharacterMovementHandler.cpp:15`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/CharacterMovementHandler.cpp#L15) |
| Normal saldırı | `WIZ_ATTACK 0x08` | `u8 type, u8 result, i16 tid, i16 delaytime, i16 distance` | [`GameServer/AttackHandler.cpp:10`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/AttackHandler.cpp#L10) |
| Skill / pot | `WIZ_MAGIC_PROCESS 0x31` | `u8 opcode, u32 skill, i16 caster, i16 target, i16 data[7]` | `GameServer/MagicProcess.cpp:22,41-49` |
| Hedef HP isteği | `WIZ_TARGET_HP 0x22` | `u16 uid, u8 echo` | [`GameServer/User.cpp:326-333`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L326-L333) |
| Respawn | `WIZ_REGENE 0x12` | `u8 type` | [`GameServer/User.cpp:304-305`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L304-L305) |
| Oturma/kalkma | `WIZ_STATE_CHANGE 0x29` | tip 1 | [`GameServer/User.cpp:2685-2690`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L2685-L2690) |
| Party | `WIZ_PARTY 0x2F` | alt opcode 1 create, 2 permit, 3 insert, 4 remove, 5 delete, 0x1C promote | [`shared/packets.h:238-249`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/packets.h#L238-L249) |
| Chat | `WIZ_CHAT 0x10` | `u8 type, string` | [`GameServer/ChatHandler.cpp:89-276`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/ChatHandler.cpp#L89-L276) |
| Item taşıma | `WIZ_ITEM_MOVE 0x1F` | (bkz. kod) | [`GameServer/ItemHandler.cpp:541-743`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/ItemHandler.cpp#L541-L743) |

Paket alanlarının gerçek istemcideki değerleri (ör. `distance` ölçeği, `echo`) T-MECH-CLIENT-02 ile kayıttan doğrulanır `[A]`.

## 15. Mekanik hatalar ve tuhaflıklar

Botlar sunucu kurallarını **olduğu gibi** oynar; hatalar bot tarafında "düzeltilmez". Düzeltme yapılacaksa sunucu kodunda, `[MECH]` etiketli ayrı commit ve ADR ile yapılır ve hem botlara hem oyunculara uygulanır.

| Kimlik | Hata/tuhaflık | Kaynak | Bot etkisi | Öneri |
|---|---|---|---|---|
| MB-01 | 360/720 HP ve 960/1920 MP NPC potlarında `UseItem=0`, tüketilmiyor | yerel MAGIC | İnsan ve bot için sınırsız (aynı kural) | **K-5: olduğu gibi kalır** (ADR-0009) |
| MB-02 | Aynı saniye içinde skill recast kontrolü geçiliyor | [`GameServer/MagicInstance.cpp:360-370`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L360-L370) | Pot/heal spam mümkün | CLI-04, CLI-06 |
| MB-03 | Mage armor yansıması %25 yerine tam hasarı yansıtıyor | [`GameServer/MagicInstance.cpp:2942-2976`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L2942-L2976) | Mage armor'a vurmak ağır cezalı | Mekanik olarak kabul; hedef skoru hesaba katar ([09](09_PARTY_COORDINATION_AND_TARGET_SELECTION.md)) |
| MB-04 | AC debuff'ı iki kez uygulanıyor | [`GameServer/User.cpp:2211`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L2211), [`GameServer/Unit.cpp:240-241`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/Unit.cpp#L240-L241) | Malice/Torment beklenenden güçlü. **Ölçüldü (2026-10-02, T-MECH-DMG-02) `[V]`:** `BotWG_K` → `BotWP_E` R vuruşu, Malice öncesi AC 857, ort. 96,9 (n=7, model 92,9); Malice sonrası hasar kaydındaki AC 658 (tek uygulama), ort. 134,5 (n=4), min 121, maks 144. Tek uygulamayla (AC 658) beklenen ort. ~114, aralık ~97–131: ölçülen ortalama +4σ, maks 144 aralık dışı. İkinci uygulamayla (etkili AC 658 × 0,75 ≈ 494) beklenen ort. ~139, aralık ~118–160: ölçümle uyumlu. Kod: `User.cpp:2245` AC'yi yüzdeyle düşürür, `Unit.cpp:240-241` aynı `m_sACPercent`'i toplam AC'ye bir daha uygular. Örnek küçük (n=4); kod okumasıyla birlikte çift uygulama kesin kabul edilir | Değer [05](05_SKILL_CATALOG_AND_COMBAT_RULES.md)'te etkili değer olarak not edilir |
| MB-05 | İsabet/kaçınma buff çarpanları tamsayı /100: < 100 → 0, 101–199 → 1 | [`GameServer/User.cpp:2213-2215`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L2213-L2215) | İsabet debuff'ı hedefin isabetini 0'a indirir | Kabul; test edilecek |
| MB-06 | HP-drain item bonusu savunanı iyileştiriyor | [`GameServer/Unit.cpp:629-632`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/Unit.cpp#L629-L632) | Bu item'lar referans setlerde kullanılmaz ([04](04_CHARACTER_BUILDS_STATS_AND_EQUIPMENT.md)) | Kaçın |
| MB-07 | Mirror-damage item bonusu sahibine (saldırana) hasar veriyor | [`GameServer/Unit.cpp:667-671`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/Unit.cpp#L667-L671) | Kaçın | Kaçın |
| MB-08 | Boldness (+%20 AC, HP < %30) asla etkinleşmiyor | [`GameServer/User.cpp:2272`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L2272) | Değerlendirmede yok sayılır | — |
| MB-09 | Direnilen yavaşlatma/stun sunucu buff haritasında kalıyor | [`GameServer/MagicInstance.cpp:1816-1843`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L1816-L1843) | Sonraki hız buff'ı engellenebilir `[A]` | Test T-MECH-12 |
| MB-10 | Type7 başarıda `false` döndürüyor, cooldown kaydedilmiyor | [`GameServer/MagicInstance.cpp:2225`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L2225) | Kapsam dışı sınıflar | — |
| MB-11 | AIServer A* yürünebilirliği ters yorumluyor | [`AIServer/MAP.cpp:124-127`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/AIServer/MAP.cpp#L124-L127) | NPC davranışı test sonuçlarını etkileyebilir | Arena izolasyonu ([15](15_TEST_ARENA_SCENARIOS_AND_ACCEPTANCE_CRITERIA.md)) |
| MB-12 | `m_bMaxWeightAmount` başlatılmıyor | [04](04_CHARACTER_BUILDS_STATS_AND_EQUIPMENT.md) | Kod düzeyinde **doğrulandı** `[D]` (F1-05): GameServer'da yalnızca buff atamaları (`MagicProcess.cpp:373,:729`), kurucu/`Initialize` dokunmuyor (AIServer'da `AIUser.cpp:37` 100 atar); uint8 değeri 1–99 ise `m_sMaxWeight = 0` (`User.cpp:2184`, tamsayı bölme). Gerçek değer çalışma zamanında ölçülecek `[A]` | Test T-DATA-05 |
| MB-13 | Respawn tip 2 taş harcıyor ama yine şehre gönderiyor | [`GameServer/AttackHandler.cpp:109-175`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/AttackHandler.cpp#L109-L175) | Bot tip 1 kullanır | — |
| MB-14 | El Morad `/town`/respawn rastgele ofset alırken Karus başlangıç noktasının tam kendisine konuyor (zone değişiminde) | [`GameServer/CharacterMovementHandler.cpp:336-337`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/CharacterMovementHandler.cpp#L336-L337) | Karus botları zone girişinde üst üste spawn olabilir | Bot spawn kendi ofsetini uygular |
| MB-15 | 70. seviye skill'lerin quest kimlikleri (51–54, 32 satır) sunucu `MAGIC` tablosunda `Etc` yerine `UseStanding` sütununda; `Etc` 0 (düzeltme öncesi 1) | `MAGIC` verisi; `tools/client-tbl-quests.py --server`; `GameServer/MagicInstance.cpp:306`, `:328-329`, `:353` | Sunucu bu quest'leri uygulamaz (istemci uygular); `UseStanding` 51–54 olduğu için bu skill'lerde ayakta şartı ve EFFECTING menzil denetimi de çalışmaz (KI-017) | Veri olduğu gibi kalır (karar Q-27); bot ve insan için kural aynı; bot `quest_locked` kuralı yalnızca `Etc != 0` skill'lere uygulanır |

## 16. Gözlemlenebilirlik (istemcinin aldığı bilgi)

Bu tablo [14](14_LEARNING_AND_ADAPTATION.md) §5.2'deki gözlem sözleşmesinin dayanağıdır.

| Bilgi | İstemciye nasıl gelir | Bot kullanabilir mi | Etiket |
|---|---|---|---|
| Görüş alanındaki oyuncular: isim, ulus, ırk, sınıf, seviye, party lideri bayrağı, görünür ekipman (10 yuva), gizlilik durumu | `WIZ_USER_INOUT` / `GetUserInfo` (3×3 bölge) | Evet | `[D]` |
| Görüş alanındaki konum ve hareket | `WIZ_MOVE` yayını | Evet | `[D]` |
| Hasar verdiği hedefin **kesin HP / maks HP**'si | Her hasardan sonra saldırana `WIZ_TARGET_HP` ([`GameServer/User.cpp:1979`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L1979)) | Evet | `[D]` |
| Seçili hedefin HP'si | İstemcinin `WIZ_TARGET_HP` isteğine cevap; sunucu menzil kontrolü yapmıyor | Evet, **tek seçili hedef** ve ≤ 2 istek/sn sınırıyla (`P-OBS-TARGETHP-RATE`) | `[D]` `[Ö]` |
| Party üyelerinin HP ve MP'si | `PARTY_HPCHANGE` | Evet | `[D]` |
| Skill cast ve etki olayları (kim, kime, hangi skill) | `WIZ_MAGIC_PROCESS` bölge yayını ([`GameServer/MagicInstance.cpp:758-772`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L758-L772)) | Evet; düşman üzerindeki buff/debuff'lar **görülen olaylardan** takip edilir | `[D]` |
| Ölüm | `WIZ_DEAD` bölge yayını | Evet | `[D]` |
| Görüş alanındaki NPC/canavar/kule/kapı: prototip kimliği, tip, ad, ulus (canavarda 0), seviye, konum, kapı durumu; hareket; ölüm | Toplu kayıt `WIZ_REQ_NPCIN` (spawn'da ve istemci isteğiyle; cevaptaki sayı **istenen** sayıdır), `WIZ_NPC_INOUT` (tip tek bayt), `WIZ_NPC_MOVE`, bölge değişiminde kimlik listesi `WIZ_NPC_REGION`, `WIZ_DEAD` (NPC için çıkış paketi gelmez, yeniden doğuşta `WIZ_NPC_INOUT` IN gelir) (`GameServer/Npc.cpp:73-155`, `GameServer/GameServerDlg.cpp:1478-1614`) | Evet; NPC HP'si yalnızca seçili hedef için `WIZ_TARGET_HP` ile (bu satır kapsamı dışı) | `[D]` |
| Düşmanın MP'si, cooldown'ları, envanteri, pot stoku | Gönderilmez | **Hayır** | `[D]` |
| Görüş alanı dışındaki birimler | Gönderilmez | **Hayır** | `[D]` |
| Hedeflenmemiş/hasar verilmemiş düşmanın HP'si | Gönderilmez (seçim yapılmadıkça) | Hayır | `[D]` |
| Kendi skill sonucu ve fail sebebi | `MAGIC_FAIL` yalnızca çağırana | Evet | `[D]` |

Görüş hattı: sunucuda görüş hattı kontrolü yoktur (§3, [12](12_NAVIGATION_AND_POSITIONING.md)). İnsan oyuncunun ekranında bir birimin duvar arkasında görünüp görünmediği istemcinin çizimine bağlıdır. Bu nedenle bot, **görüş alanı (3×3 bölge)** bilgisini kullanabilir. "Duvar arkasından hedefleme" ise [12](12_NAVIGATION_AND_POSITIONING.md)'deki yaklaşık görüş hattı testiyle sınırlanır (`P-NAV-LOS-MODE`).

## Ek A — BuffType ve sunucu etkisi `[D]` ([`GameServer/GameDefine.h:745-804`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/GameDefine.h#L745-L804), [`GameServer/MagicProcess.cpp:268-631`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicProcess.cpp#L268-L631))

| Değer | Ad | Buff/debuff | Sunucu etkisi |
|---|---|---|---|
| 1 | HP_MP | buff | Maks HP/MP artışı |
| 2 | AC | işarete göre | AC sabit/yüzde |
| 4 | DAMAGE | ≥100 buff | Saldırı çarpanı |
| 5 | ATTACK_SPEED | ≥100 buff | **Sunucuda kullanılmıyor** (istemci) |
| 6 | SPEED | ≥100 buff | **Yalnızca istemci** |
| 7 | STATS | buff | Stat artışı |
| 8 | RESISTANCES | buff | Direnç artışı |
| 9 | ACCURACY | ≥100 buff | İsabet/kaçınma (MB-05) |
| 10 | MAGIC_POWER | ≥100 buff | Büyü hasarı |
| 19 | DAMAGE_DOUBLE (Critical Point) | buff | PvP AP %, heal ikiye katlama şansı |
| 20 | DISABLE_TARGETING | debuff | R atamaz (cast edebilir) |
| 22 | FREEZE | debuff | R ve hedefli skill'lere karşı bağışık; `/town` yok |
| 24 | DECREASE_RESIST | debuff | Direnç yüzdesi |
| 25 | MAGE_ARMOR | buff | Yansıtma (MB-03) |
| 27 | Elysian Web | buff | Büyü hasarı azaltma |
| 28 | Wall of Iron | buff | +%300 AC, hız yarı (istemci) |
| 29 | BLOCK_CURSE (Counter Curse) | buff | Tüm debuff'ları engeller |
| 30 | BLOCK_CURSE_REFLECT | buff | %25 yansıt, aksi engelle |
| 31 | MANA_ABSORB | buff | Hasarın bir kısmı MP'den |
| 32 | IGNORE_WEAPON | debuff | Silah devre dışı |
| 40 | SPEED2 (Cold Wave) | debuff | Hız % (istemci) |
| 44 | MIRROR_DAMAGE_PARTY (Minak's Thorn) | buff | Hasarı party'ye yay |
| 47 | STUN | debuff | Yalnızca hız (istemci) |
| 150 | NO_RECALL | debuff | Summon/warp engeli |
| 152 | SILENCE_TARGET | debuff | Skill ve pot yok |
| 153 | NO_POTIONS | debuff | HP potu yok |
| 155 | UNDEAD | debuff | Heal → hasar |
| 157 / 158 | BLOCK_PHYSICAL / MAGICAL | buff | İlgili hasar 0 |

Tam liste: `appendix/research/B_combat.md` Tablo A.

## Değişiklik günlüğü

| Tarih | Sürüm | Değişiklik |
|---|---|---|
| 2026-10-01 | v1.0 | İlk sürüm |
| 2026-10-02 | v1.1 | §13.2 ölçülen istemci değerleri (F1-02 `war-r` oturumu); MAGIC paketi 21 bayt notu; `war-skill`, `pot`, priest/mage cast ve iptal ölçümleri |
| 2026-10-02 | v1.2 | Değerlendirme (`docs/reports/degerlendirme-2026-10-02.md`): CLI-02 satırı ölçümle hizalandı (kilit yok), CLI-05 ölçülmüş hız/paket sıklığı, CLI-11 gerekçesi düzeltildi, §13.4 üç katman tablosu |
| 2026-10-02 | v1.3 | MEC-MAG-12 (uçan Type3 skill'de MP iki kez düşer, `[D]`; F4-25) |
| 2026-10-02 | v1.4 | MEC-MAG-12 `[D]` → `[V]` (F4-25 doğrulaması S1: MP 6021 → 5971 → 5921) |
| 2026-10-02 | v1.5 | MEC-MAG-13 (çift tipli Type3 + Type4 skill: yanıtı yalnızca Type4 yayınlar, `[D]`; F4-26) |
| 2026-10-03 | v1.6 | MEC-MAG-14 (skill'in quest ile açılması, MB-15; F4-27) ve MEC-MAG-15 (tek hedefli Type4: aynı `BuffType` reddi, MP yalnızca başarıda, debuff yenileme, `[D]`; F4-28) |
| 2026-10-03 | v1.7 | MEC-MAG-16 (düşman alan skill'i: hedef kimliği `-1`, hedef noktası `sData[0]`/`sData[2]`, boş alan başarılı ve MP düşer, kurban başına yayın, `[D]`; F4-29) |
| 2026-10-03 | v1.8 | MEC-MAG-17 (uçan düşman alan skill'i: MEC-MAG-12 + MEC-MAG-16 birlikte, cast başına 2 × `Msp`, ağaç denetimi CASTING'te, `[D]`; F4-30) |
| 2026-10-03 | v1.9 | MEC-MAG-17 `[D]` → `[V]` (F4-30 doğrulaması: uçan alan çalışma zamanında ölçüldü) |
| 2026-10-03 | v1.10 | MEC-MAG-18 (party hedefli skill: `Moral` 4 tek üye, `Moral` 6 grup yolu hedef `-1`, kurban `PartyID` + `Radius`, grup buff'ta aynı `BuffType` üyesi sessizce atlanır, `MAGIC.HP >= 10000` sacrifice, `[D]`; F4-31) |
| 2026-10-03 | v1.11 | MEC-MAG-18 `[D]` → `[V]` (F4-31 doğrulaması: party hedefli skill çalışma zamanında ölçüldü) |
| 2026-10-03 | v1.12 | MEC-MAG-19 (Type5 cure `Moral` 2: kaldırılacak şey olmasa da `effected`, MP Type5'te EFFECTING'te bir kez, ölü hedef yayınsız, `[D]`; F4-32) |
| 2026-10-03 | v1.13 | MEC-MAG-19 `[D]` → `[V]` (F4-32 doğrulaması: Type5 cure `Moral` 2 çalışma zamanında ölçüldü; ölü hedefte `no_result` ve MP düşer) |
| 2026-10-03 | v1.14 | MEC-MAG-20 (diriltme `Moral` 25: taşlar ölü hedeften alınır, çağırana `NeedStone/2+1` verilir, hedef ölen yerinde tam HP ve MP 0 ile dirilir, taş eksikse yayın yine gider, `[D]`; F4-33) |
| 2026-10-03 | v1.15 | MEC-MAG-20 `[D]` → `[V]` (F4-33 doğrulaması: diriltme çalışma zamanında ölçüldü; taşlar ölü hedeften alınır, hedef ölen yerinde tam HP ve MP 0 ile dirilir; çağıran taş değişimi ve exp iadesi `[D]` kaldı) |
| 2026-10-03 | v1.16 | MEC-MAG-21 (summon `Moral` 4 `WarpType` 12: party dışı hedef CASTING'te `srv_fail`, ölü/başka zone/`self` hedefte yayın `sData[1] = 0` ile gider ve MP düşer, `Warp` `m_bWarp` kurmaz, `[D]`; F4-34); MEC-T8-02 netleştirildi (180 sn kuralı yalnızca hedef `-1` yolunda) |
| 2026-10-03 | v1.17 | MEC-MAG-21 `[D]` → `[V]` (F4-34 doğrulaması: summon çalışma zamanında ölçüldü; ışınlanma, ölü hedefte `effected` + hedef yerinde + MP −5, party dışı `srv_fail`; zone dışı/`canTeleport()`/`m_bWarp`/Forgotten Temple dalları `[D]` kaldı) |
| 2026-10-03 | v1.18 | MEC-MAG-22 (warp: Gate `Moral` 1 `WarpType` 1 ve descent `Moral` 4 `WarpType` 25: Gate Ronark'ta engelli değil, Escape engelli; descent `Radius 30` aşılırsa `MAGIC_FAIL` + yine de EFFECTING ve MP gider, `[D]`; F4-35); MEC-T8-04 (T-MECH-T8-02'nin kod cevabı); §5.5 WarpType 1 satırı düzeltildi |
