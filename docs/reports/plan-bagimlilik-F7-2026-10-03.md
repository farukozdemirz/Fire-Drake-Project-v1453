# F7 taslak planları: bağımlılık özeti (2026-10-03, planlayıcı yardımcısı)

> Bu dosya bir **özet**tir; plan değil. F7 planlarının hepsi `TASLAK`: F6 kapanmadan `HAZIR` yapılmaz. Okuma tabanı: `gece/2026-10-02` @ `7891f74` (F4-60 `DOĞRULANDI`, birleşti); F6 planları `drafts/f6/F6-01..F6-08` (hepsi `TASLAK`; **F6-09 ve F6-10 henüz yazılmamış**, adları `F6-01` §3.1 ve `F6-07` §8'den). Repo dosyası değiştirilmedi; çıktılar `drafts/f7/` altındadır.

## 1. Yazılan planlar ve Durum

| Plan | Başlık (kısa) | Durum | Dosya sayısı / test (taslak) | Kapı |
|---|---|---|---|---|
| F7-01 | `TeamBlackboard` iskeleti, rezervasyon tablosu, `pending_heals` kaynağı + F6-07 `hp_pred` bağlaması | TASLAK | 6 / ≈ 32 | G7a |
| F7-02 | İki priest görev koordinasyonu (`PriestCoord.h`) + heal-stall kararı (`HealStall.h`) | TASLAK | 6 / ≈ 40 (F7-02a/b'ye bölünebilir) | G7a, G7c |
| F7-03 | Ortak hedef: F6-02 hedef seçiminin takıma genişletilmesi, `TargetCall` kaydı, takım override'ları (`TeamTarget.h`) | TASLAK | 6 / ≈ 34 | G7c |
| F7-04 | Debuff seçimi + başarı doğrulaması, hedef çağrısı + party chat (CLI-18), healer'a hedef değiştirme emirleri | TASLAK | 8 / ≈ 40 (F7-04a/b'ye bölünebilir) | G7a, G7c |
| F7-05 | Mage: debuff sonrası patlama penceresi (Absolute power) + güvenli summon SUM-01..07 + `MemberStatus` | TASLAK | ≤ 10 / ≈ 40 (F7-05a/b'ye bölünebilir) | G7b, G7c |
| F7-06 | Priest destek: buff matrisi/takibi, direnç planı, cure skoru ve rezervasyon kapısı | TASLAK | 6 / ≈ 45 | G7a |
| F7-07 | Priest diriltme kapıları, skill/taş seçimi, `RESPAWN_HOLD`, sonuç doğrulama | TASLAK | 4 / ≈ 38 | G7a |

Dosya adları: `drafts/f7/F7-0N-*.md`. Proje sahibinin önerdiği F7-05 (summon + buff + cure + diriltme) **üçe bölündü** (F7-05 / F7-06 / F7-07): hepsi tek günlük sınırı ve ≤ 10 dosya kuralını aşardı. Toplam ≈ 270 birim test (tabanı 259'dan ≈ 530'a çıkarır; F4-60/F4-61 ve F6 planları da sayıyı değiştirir).

**Yazılmayan, bu seri için gerekli planlar** (kimlik ayrıldı, taslak yazılmadı):

- **F7-08: sunucu bağlaması ve oyun içi doğrulama.** Kapsam: parti başına tek `TeamBlackboard` sahipliği ve yaşam döngüsü (`BotManager`; ADR-0005 tek IOCP thread'i ⇒ kilitsiz; yeni ADR gerekir, numara yazım turunda), F6-06 `BrainDriver`'ın takım girdisiyle genişletilmesi (`DecisionInput`'a takım işaretçisi: F6-01 yapısına **ekleme**), `ObservedStatusTable`/`HealObsRing`/`SkillEventRing`/`TeamView`'dan F7 girdi yapılarının kurulması, `RES_*`/`TARGET_*`/`STALL_*`/`SUMMON_*`/`CHAT_*`/`BUFF_COVERAGE_LOST` telemetri olayları, `/bot team` snap dökümü, `MET-HEAL-04`, `MET-STALL-01`, `MET-DEBUFF-04`, `MET-CHAT-01`, `MET-SUM-02`, `MET-TGT-03/04` ölçümü ve `T-PRI-03/04/05/06/08`, `T-MAG-05/06`, `T-PTY-02/03/04/09`, `T-IGT-PRI-01`, `T-IGT-MAG-01`, `T-IGT-PTY-01` çalışma zamanı koşuları. Büyük olacağından **F7-08a..e** olarak bölünmesi önerilir.
- **F7-00: `BrainParams` F7 parametre bloğu.** F6-01 §3.1 tablosu P-PRI-HEAL-*, P-MAG-*, P-TGT-* değerlerini içerir ama **P-MAG-SUMMON-\*, P-MAG-BURST-WINDOW, P-PRI-BUFF-REFRESH, P-PRI-CURE-DOT-MIN, P-PRI-RES-\*, P-PRI-CALL-DEDUP, P-PRI-CHAT-RATE, P-PTY-\*, P-TEAM-\*** değerlerini içermez ve "sonraki planlar satır eklemez" der. F7 planları bu sabitleri başlıkta `constexpr` `[Ö]` tutar; AC-LRN-04 aralık denetimi için `kParamTable`'a taşınmaları ayrı küçük plandır.

**Bu serinin kapsamı dışında kalan F7 işleri** (`docs/17` F7 bloğu, proje sahibinin istediği kapsam dışı; plan kimliği ayrılmadı): party kurulumu ve kompozisyon kuralı (`T-PTY-01`, `AC-PTY-06`), lider/vekil lider (`T-PTY-05`, `AC-PTY-04`, `P-TEAM-LEADER-TIMEOUT`), takım modu `ENGAGE/HOLD/RETREAT/REGROUP`, regroup/formasyon (`T-PTY-06`, `AC-PTY-05`), ikinci düşman party (`T-PTY-08`), tam yenilgi (`T-PTY-07`, EVAL-WIPE), warrior takım davranışları (`AC-WAR-04` peel p50 ≤ 1,5 sn, `AC-WAR-06` Defense/AC buff çakışması = 0, W-G anchor), `TeamPlan`/`EnemyIntel`/`PeelRequest` kayıtları ve birleşik takım görünümü (`shared_snapshot`), mage'in direnç buff'ını **atması** (`docs/08` §5 madde 7), DoT/HoT izleyicisi (F4-53 kapsam dışı), `P-TEAM-HUMAN-CALLS` (varsayılan kapalı), insan+bot karma party.

## 2. F6 ve diğer planlarla bağımlılık tablosu

`●` = bağımlılık (plan başlamadan `KAPANDI`), `◐` = girdi/tek kaynak sözleşmesi (yazım turunda imza doğrulanır), `—` = yok.

| F7 planı | F6-01 (karar katmanı iskeleti) | F6-02 (hedef seçimi solo) | F6-07 (priest heal döngüsü) | F6-08 (mage saldırı) | F6-09 (priest buff/cure/debuff solo; **taslak yok**) | Diğer F7 | Algı/aksiyon (F4/F5) |
|---|---|---|---|---|---|---|---|
| F7-01 | ● `BotRole`, `BrainParams` (`P-PRI-HEAL-EMERG/PREHEAL-K`) | — | ● **`hp_pred` tek kaynağı**; `PriestHeal.h` bağlama noktası (küçük değişiklik) | — | — | — | F4-18 `TeamView`, F4-53 `HealObsRing` (kaynak sınıfı), F4-60 |
| F7-02 | ● `BotRole`, `BrainParams` | — | ● `PickHeal`/`hp_pred` (hedef süzgeci; skill seçimi F6-07'de) | — | — | F7-01 ● | F4-51 `HpTable`, F4-53 `HealObsRing`, F4-60/F4-61 |
| F7-03 | ● `ReasonCode` (yeni ad eklenmez), `BrainParams` `P-TGT-*` | ● **`SelectTarget`, `TargetExtras`, sert filtreler, bağlılık, override'lar** (tek kaynak; `TargetSelect.h`'ye küçük ekleme) | — | — | — | F7-01 ●, F7-02 ● (`StallDecision`) | F5 `NavReach` (üye yolu: çağıran girdisi), F4-50/51 |
| F7-04 | ● `BotRole`, `BrainParams` | ◐ (`TargetCall` ⇄ `TargetChoice`) | ◐ | — | ◐ **P-HD tek hedefli debuff seçimi** (varsa onu çağırır) | F7-02 ●, F7-03 ● | F4-28/F4-44, F4-11 (`CheckChat`), F4-53/F4-60 |
| F7-05 | ● `BotState` (F7 durumlarını **yasal kılar**: `Brain.h` değişir), `BotRole` | — | — | ● **`PickSingle` patlama sırası** (`MageCombat.h`'ye `teamBurstWindow` ekleme) | — | F7-01 ●, F7-03 ● (`TargetCall`), F7-04 ◐ (`shortBurstWindow`), F7-02 ◐ (`BURST_NOW`) | F4-34/F4-35 summon/Gate/descent, F4-36 `no_item`, F4-52 `SkillEvent.data[1]` |
| F7-06 | ● `BotRole`, `BotState`, `BrainParams` | — | ◐ | — | ◐ **buff/cure tek hedefli hâli** (varsa onu genişletir) | F7-01 ●, F7-02 ● (`CureGateAllows`, `BuffCoverageLost`), F7-05 ● (`MemberStatus`, `Reintegrate`) | F4-28/F4-31/F4-32/F4-42/F4-43, F4-53/F4-60/F4-61 |
| F7-07 | ● `BotRole`, `BotState` | — | ◐ (priest MP/HP rezervi) | — | ◐ **diriltmenin tek müttefik hâli** (F6-07 §3 kapsam dışı satırı) | F7-01 ●, F7-05 ● (`MemberStatus.lifeStones`, `BotState` geçişleri), F7-02 ◐, F7-06 ◐ (`MemberBuffState`) | F4-33, F4-07 (`Regene`, CLI-14), F4-40 (taş stoğu), F4-52/F4-60 |

Dolaylı F6 bağımlılıkları: F6-04 (pot: priest cast sırasında pot yok), F6-05 (geri çekilme/`role_adj`), F6-06 (`BrainDriver`, `L0Policy`: F7-08'in genişleteceği sunucu bağlaması). F5: F5-55 dilimleri (F5-59..F5-66) F7 birim planlarını **bloklamaz** (yalnızca F7-08 çalışma zamanı).

**Önerilen yazım ve uygulama sırası:** F7-01 → F7-02 → F7-03 → F7-04 → F7-05 → F7-06 → F7-07 → F7-08 (F7-02/F7-03 sırası değişebilir; F7-04, F7-02 ve F7-03'ten sonra; F7-05, F7-04'ün `shortBurstWindow` bayrağını kullanır; F7-06/F7-07, F7-05'in `MemberStatus`'unu kullanır). `docs/17` G7a = F7-01, F7-02a, F7-04, F7-06, F7-07; G7b = F7-05; G7c = F7-02b, F7-03, F7-04b, F7-05b + F7-08.

## 3. F6 ile tek kaynak kararları (çakışma çözümü)

| Konu | Tek kaynak | F7 planındaki karşılığı |
|---|---|---|
| `hp_pred`, `IncomingEst`, `HpHistory`, `KnownHealLog`, heal skill seçimi | F6-07 `PriestHeal.h` | F7-01 yalnızca `pending_heals` **kaynağını** tabloya bağlar; `emerg`/`preHealK` `BrainParams`'tan |
| Hedef skoru `K/R/T/D/Risk`, sert filtreler, bağlılık/marj (negatif skor çözümü), `Finishable/SelfDefense/TargetDead/TargetLostVis/TargetUnreachable` | F6-02 `TargetSelect.h` | F7-03 yalnızca takım `R`, `forced` hedef, benimseme, `HealerSwitch`/bireysel `PeelThreat`, `TargetCall` ekler |
| Rol/durum türleri `BotRole`, `BotState`, `ReasonCode` | F6-01 `Brain.h` | F7 planları ikinci enum tanımlamaz (taslakta yazım sırasında yapılan `ROLE_*`/`MemberState` adları `BotRole`/`BotState`'e çevrildi); F7-05 yalnızca F7 durum **geçişlerini** yasal kılar |
| Mage patlama sırası (incineration/Prismatic → Pillar/Ice comet), menzil, MP rezervi | F6-08 `MageCombat.h` `PickSingle` | F7-05 yalnızca pencere kaynağı (`teamBurstWindow`) ve Absolute power'ı ekler |
| P-HD tek hedefli debuff, priest buff/cure/diriltme (solo/self/tek müttefik) | **F6-09 (yok)** | F7-04/F7-06/F7-07 onun üstüne takım katmanını ekler; F6-09 yazılınca çakışma çözülür, çakışırsa F6-09 kazanır |
| Parametre varsayılanları `P-PRI-HEAL-*`, `P-TGT-*`, `P-MAG-MP-RESERVE` | F6-01 `BrainParams` | F7 planları literal tekrar etmez; F7'ye özgü sabitler `constexpr` (F7-00 ile registry'ye taşınır) |

## 4. Kabul ve test eşlemesi (birim vs çalışma zamanı)

| Kabul/test | Birim düzeyi (bu seri) | Çalışma zamanı (F7-08) |
|---|---|---|
| AC-PRI-09 | F6-07 (tek sahip) + F7-01 (takım: artar/`maxhp`; her kapanma nedeni ayrı test; ikinci priest `< EMERG`) | T-PRI-03 |
| AC-PRI-03 (MET-HEAL-04 ≤ %5) | F7-02 (çift heal engeli kararı) | **yalnızca burada** |
| AC-PRI-04 (MET-BUFF-01/03) | F7-06 (`Buff_*`) | T-PRI-04 |
| AC-PRI-05 (MET-CURE-01) | F7-06 (`Cure_*`) | T-PRI-05 |
| AC-PRI-06 (MET-DEBUFF-04 = 0; MET-CHAT-01 = 0) | F7-04 (`Call_*`, `ChatFormat_*`) | T-PRI-06, T-PTY-09 |
| AC-PRI-07 | — (F6-07/F7-08) | T-IGT-PRI-01 |
| AC-PRI-08 | F7-07 (`Res_*`, `IsUnsafeRes`) | T-PRI-08 |
| AC-PTY-01/02 | F7-03 (`TeamTarget_NoThrash_Sim60s`, `Adopt_*`) | T-PTY-02/04 |
| AC-PTY-03 (MET-STALL-01) | F7-02 (`Stall_*`, `Decision_*`) | T-PTY-03, EVAL-HEALSTALL |
| AC-PTY-07 | F7-04 | T-PTY-09 |
| AC-MAG-04/05 | F7-05 (`Sum*`, `Result_*`; **AC-MAG-05 birim düzeyinde karşılanır**) | T-MAG-05/06, T-IGT-MAG-01 |
| AC-PTY-04/05/06, AC-WAR-04/06 | **kapsam dışı** (bu seri yazmıyor) | — |

Hiçbir F7 planı oyun içi kabulü iddia etmez; `docs/17` §4 oyun içi kanıt kuralı korunur.

## 5. HAZIR yapmadan önce Claude'un doküman işleri (planlarda "Neden TASLAK" altında ayrıntılı)

1. `docs/09` §4.3: `TAMAMLANDI` ("veya bitiş + 0,3 sn") ve `ZAMAN_ASIMI` (bitiş + 1,0 sn) çelişkisi; `docs/07` §5.1: "bitişten sonra sayılmaz" (F7-01).
2. `docs/07` §6: "üyeye en yakın **ve** MP'si yüksek priest" birleşimi (F7-02).
3. `docs/09` §6.1: `stall` hangi pencerede (5/10 sn; her ikisi mi) (F7-02).
4. `docs/09` §5.2: takım `R` tanımı (F6-02'nin solo çelişkisi takımda da var) (F7-03).
5. `docs/07` §9.2: "skor ≥ eşik" değeri (öneri `P-TGT-CALL-MIN`) (F7-04).
6. `docs/09` §12: `GERİ` ASCII dışıdır; CLI-18 yalnızca 0x20-0x7E (`BotCore/BotCombat.h:956-982`); Q-16/T-PTY-09 sonuçlanana kadar `GERI` (F7-04).
7. `docs/07` §9.1: debuff **öncelik sırası** (F7-04).
8. `docs/08` §8.1: SUM-02/SUM-04 sınırları ("içinde", "bir başka"), lider çağrısında "kısa pencere" süresi, `docs/13` §6'ya F7 durum geçişleri (F7-05).
9. `docs/07` §7.1: priest HP buff'ı hücresi (massiveness) ile `BUF-HP-01` (priest için Undying) çelişkisi; T-MECH-BUF-03 (F7-06).
10. `docs/07` §8: cure skoru sayısal ağırlıkları (F7-06).
11. `docs/07` §7.3: **veriye göre** (`docs/appendix/data/skills_mage.csv`) Immunity fire `110548`/cold `110648` (ağaç 48 puan, süre 300 sn) iki referans mage'in de erişiminde, Immunity lightning `110748` hiçbirinde yok; "M-F ateş, M-I buz" ifadesi dardır (F7-06).
12. `docs/09:196` (ölen priest neden `RESPAWN_HOLD`'a girmiyor), `res_conditions_likely` tanımı, `docs/07:199` MP rezervi (F7-07).
13. `docs/adr/ADR-0018` **Ek 28** (skill türü sınıflandırması, `skill-turu-siniflandirma.md`) ve `docs/18` Q-22 kapanışı ("Type7 kullanılmaz": `ExecuteType7` yalnızca 10 sabit hasar) `docs/17` §2.1 satırı güncellemesi.

## 6. Doğrulayamadığım / varsayım içeren noktalar

- **F6-09 ve F6-10 taslakları yok**; F7-04/F7-06/F7-07'nin F6-09 ile örtüşmesi **tahmin**dir (yalnızca `F6-07` §8 "Çelişkiler" 1 ve §3 kapsam dışı satırı). Yazıldıklarında üç F7 planı yeniden kontrol edilmeli.
- F6 taslaklarındaki **fonksiyon/alan adları** (`hp_pred` imzası, `BrainParams.priHealEmerg` alan adı, `PickSingle`, `forcedId` eklemesinin `SelectTarget` içine uyumu) F6 planları uygulanmadan doğrulanamaz; F7 planlarında "yazım turunda doğrulanır" diye işaretlendi.
- Kendi `EFFECTING` heal yayınının bota gelip `HealObsRing`'e girip girmediği (çift sayım riski) çalışma zamanında ölçülmeli; kaynak kodda doğrulanmadı (F7-01 §0 madde 4).
- Counter Curse/Curse Refraction ve No-Recall `BuffType` kimlikleri ve **gözlenebilirlikleri** doğrulanmadı (uydurulmadı; F7-03/F7-04/F7-05 bu girdiyi `false` sabitleyebilir).
- Karakter adlarının her zaman ASCII olup olmadığı (chat çağrısı) doğrulanmadı; `GERİ`/Türkçe karakter istemcide görünür mü (Q-16) bilinmiyor.
- Type7 (Binding/provoke) için **istemci** davranışı bilinmiyor; karar yalnızca sunucu kodu okumasına (`MagicInstance.cpp:2160-2231`) dayanır.
- Skill sayıları ve "kapıda reddedilen 9 skill" bu oturumdaki Python simülasyonudur (`docs/appendix/data/skills_*.csv` + `BotCombat.h:323-500` kuralları); `BeginCast`'in kendisi çalıştırılmadı, veritabanına bağlanılmadı.
- F4-61 (`UnitView` durum alanları) ve F5-59..F5-66 planlarının dosyalarını ayrıca okumadım; kimlikleri F6 taslaklarından ve `STATUS`/`README` satırlarından aldım.
- Satır numaraları `gece/2026-10-02` @ `f4daa27`/`7891f74` okumasına aittir; döngü dalı ilerlediği için yazım turunda yeniden doğrulanmalıdır.
- Hiçbir test/derleme çalıştırılmadı (salt-okunur görev); test sayıları (`≈`) plan metinlerinden sayılmış **tahmin**dir.
