# Faz Sonuç Raporu — F4 Aksiyon yürütme ve adalet koruması (TASLAK)

Tarih: 2026-10-02 · Hazırlayan: Claude (otonom gece döngüsü) · Onaylayan: — (bekliyor)
Değerlendirilen commit: `671c1f6` (`gece/2026-10-02`; F4-23 birleştirmesi) · Sunucu commit: `0f52027` + F1-01/F1-09 kancaları (bayrakla kapalı) + F2–F4 bot kodu (`[BOT] ENABLED=0` varsayılan) · DB özeti: yerel `FDP_kn_online`; 12 bot karakteri `db/002` ile kurulu

> **GÜNCELLEME (2026-10-02, ADR-0018):** F4 kapsamı genişletildi; §3'teki ertelenen aksiyon desteği dilimleri (F4-24 ve sonrası) bitmeden F4 tamamlanmış sayılmaz. Aşağıdaki "DeepSeek'in yapabileceği F4 işleri bitti" ifadesi bu güncellemeden önceki durumdur.

> **Durum: TASLAK.** DeepSeek'in yapabileceği F4 işleri bitti (F4-01..F4-23 `KAPANDI`). Faz kabulü için kalan işler: (a) **insan istemcisi testleri** (T-ARCH-05, 13..17, T-REGENE-01, T-PARTY-01..03, T-PERC-01), (b) **ölçüm maddeleri** (§3, §5): F3'ten devreden "`decisions` seviyesinde 16 bot telemetri ek maliyeti" ve "telemetri açık/kapalı tick farkı", (c) "T-MECH-SKILL'in bot tarafından yeniden çalıştırılması" (§3). Bu rapor `KABUL_EDILDI` önermez: çıkış kararı §10'da **kısmi**. F1, F2 ve F3 kabulü de verilmedi (gece modu sıra sapması, §6).

## 1. Amaç (`docs/17` §2'den)

Botların tüm temel aksiyonları **gerçek handler'lar üzerinden** ve CLI sınırları içinde yapabilmesi. Kapsam: `ActionExecutor` (Move, Stop, Attack, CastStart/Effect, UsePotion, Sit, Regene, Party, Chat, TargetHpReq); sonuç eşleme; `BotFairnessGuard` (CLI-01..12); `Perception` (gözlem sözleşmesi); betikli "test botu" ile T-MECH-SKILL'in bot tarafından yeniden çalıştırılması. Kapsam dışı: akıllı karar. Kabul: betikli dizilerde sunucuya giden geçersiz aksiyon ≤ %1; fairness ihlali (sunucuya ulaşan) 0; sözleşme dışı algı erişimi 0; tick bütçesi içinde.

## 2. Teslim edilen kapsam

| İş kalemi | Durum | Commit(ler) / plan | Not |
|---|---|---|---|
| `ActionExecutor` hareket (`Move`/`Stop`) + `BotFairnessGuard` (`BotCore/BotMotion.h`, CLI-05 hız tavanı, "ışınlanma yok" adım sınırı), `ACTION_SUBMIT/RESULT`/`FAIRNESS_REJECT` telemetrisi, `/bot move\|stop` | GELIŞTIRILDI, TEST_EDILDI (çalışma zamanı) | F4-01 (merge `a104ae3`) | ADR-0017; aksiyon = gerçek `WIZ_MOVE` + `CUser::HandlePacket()`; T-ARCH-06 proje sahibi GEÇTİ |
| Saldırı (`Attack`, R; CLI-01/CLI-11) | GELIŞTIRILDI, TEST_EDILDI | F4-02 (`a83ebde`) | T-ARCH-07 GEÇTİ; `/bot attack` yalnızca R vurur |
| Cast (`CastStart`/`CastEffect`, tek hedefli Type1/Type3; CLI-03/04/09, MEC-MAG-03/08/11) | GELIŞTIRILDI, TEST_EDILDI | F4-03 (`3bb40c7`) | T-ARCH-08 GEÇTİ; uçan/alan/buff skill'leri `unsupported_skill` (§3) |
| Pot (`UsePotion`, CLI-06 çanta + ortak 2,5 sn) | GELIŞTIRILDI, TEST_EDILDI | F4-04 (`355feb2`) | T-ARCH-09 GEÇTİ; envanter doldurma açık (§3) |
| Duruş (`StateSit`, CLI-13) | GELIŞTIRILDI, TEST_EDILDI | F4-05 (`612462e`) | T-ARCH-10 GEÇTİ |
| Hedef HP (`TargetHpReq`, CLI-10) | GELIŞTIRILDI, TEST_EDILDI | F4-06 (`f8a74b1`) | T-ARCH-11 GEÇTİ |
| Yeniden doğuş (`Regene`, CLI-14; NP 0 → `no_np`, KI-013) | GELIŞTIRILDI, TEST_EDILDI | F4-07 (`ade97ec`) | T-ARCH-12 GEÇTİ; T-REGENE-01 (CLI-14 `[A]` ölçümü) bekliyor |
| Party: kurulum, ret/ayrılma, devir/atma (`PartyInvite/Accept/Decline/Leave/Promote/Kick`; CLI-15/16/17; KI-014, KI-015) | GELIŞTIRILDI, TEST_EDILDI (sunucu tarafı) | F4-08 (`851afdc`), F4-09 (`6dc7979`), F4-10 (`3e0e985`) | T-ARCH-13..15, T-PARTY-01..03 (insan) bekliyor |
| Party chat (`ChatParty`, CLI-18) | GELIŞTIRILDI, TEST_EDILDI (sunucu tarafı) | F4-11 (`ca677c0`) | T-ARCH-16 (insan) bekliyor |
| `Perception`: görünür oyuncu tablosu `/bot see`, `WIZ_REQ_USERIN` isteği (CLI-19) | GELIŞTIRILDI, TEST_EDILDI | F4-12 (`dcd8f80`), F4-13 (`f1acc48`) | T-ARCH-17, T-PERC-01 (insan) bekliyor |
| `Perception`: NPC/canavar/kule tablosu `/bot npcs`, `WIZ_REQ_NPCIN` isteği (CLI-20) | GELIŞTIRILDI, TEST_EDILDI | F4-14 (`03a5e72`), F4-15 (`bfdd839`) | CLI-20 `[A]` ölçümü T-PERC-01 ekinde |
| `Perception`: `PerceptionSnapshot` + `/bot snap`, `SelfState` genişletme (pot stoku, soğuma, buff), `TeamView` (party üyeleri) | GELIŞTIRILDI, TEST_EDILDI | F4-16 (`bee4fe4`), F4-17 (`6f66164`), F4-18 (`a7349a1`) | F4-18 Tur 1'de `PARTY_INSERT` üye adı `u16` hatası bulundu, Tur 2'de düzeldi `[V]`; buff yolu yalnızca birim test + kod okuması |
| Betikli test dizisi: ayrıştırıcı `BotCore/ScriptPlan.h`, `ScriptRunner` (`/bot script run\|stop\|status`), senaryo–betik bağlaması (`script:` anahtarı) | GELIŞTIRILDI, TEST_EDILDI (çalışma zamanı) | F4-19 (`66342b7`), F4-20 (`e36d9d1`), F4-22 (`1a42d6a`) | betik yeni yetki vermez: her adım aynı komut çekirdeği + `ActionExecutor` + `BotFairnessGuard` yolundan; `SCRIPT_*` olayları `<match>.jsonl` içinde |
| MET-ACT-02 / MET-FAIR-01 raporu (`tools/bot-telemetry-report.py`) | GELIŞTIRILDI, TEST_EDILDI | F4-21 (`d803438`) | gerçek koşu telemetrisiyle çapraz denetlendi `[V]` |
| Algı sözleşmesi statik denetimi (`tools/check-perception-contract.py`, R1-R5) | GELIŞTIRILDI, TEST_EDILDI | F4-23 (`671c1f6`) | gerçek ağaçta `RESULT: PASS`: `R1 0/0 R2 0/28 R3 0/18 R4 0/0 R5 0/0` `[V]` |

Birim testler: `tools/run-tests.sh` → `82 tests, 0 failed` (F4 başında 6) `[V]`. `./tools/build.sh Release` hatasız, ilgili dosyalarda uyarı 0 (her planın doğrulamasında).

## 3. Kapsam dışında kalanlar / ertelenenler

- **T-MECH-SKILL'in bot tarafından yeniden çalıştırılması** (`docs/17` F4 kapsamı): betik altyapısı hazır (F4-19..F4-22) ama (a) senaryo, arena konumuna yerleştirme ve envanter doldurma ister (F3'ten devreden: `ScenarioRunner` envanter doldurma/konum sıfırlama yok), (b) `cast` dilimi uçan, alan, Type4 (buff/debuff) ve çift tipli skill'leri atamıyor (M-I'nin buz skill'leri, mage'in Fire ball/Ice arrow'u, Malice, Prismatic: `unsupported_skill`; proje sahibi test sonuçları, "Yarım kalan test oturumu" §Paket 2-3). Çekirdek skill ölçümleri bu yüzden gerçek istemcilerle yapıldı (T-MECH-DMG-01..03 GEÇTİ). **Açık iş:** `cast` dilimi genişletmesi (uçan + alan + Type4 + çift tip) ve envanter doldurma/konum sıfırlama, ayrı plan(lar) olarak F5/F6 başında ya da F4'ün ek dilimi olarak; proje sahibi karar verir.
- **Çalışma zamanı algı denetimi** (`docs/17` F4 testi "AC-LRN-03 statik/**çalışma zamanı** denetimi"): yalnızca statik kısım yapıldı (F4-23). Çalışma zamanı assert'i kapsam dışı bırakıldı (ADR-0017 Eki F4-23). Statik aracın sınırları: satır tabanlı sezgi (tek satırlık `struct X { int hp; };` R5'te görülmez), alan semantiği denetlenmez.
- **R3 istisna tablosu** (5 girdi, 18 isabet): test komutları (`/bot attack` vb.) hedef botun konumunu/kimliğini kendi oturumundan okur (ADR-0017 Ek F4-02 madde 4). Bilinçli, **geçici** test sürücüsü sapmasıdır; davranış motoru (F6) bu okumaları algı tablosundan almalı, istisnalar aracın tablosundan silinmeli.
- **Algı eksikleri:** başkalarının oturma bayrağı (`WIZ_STATE_CHANGE` yayını), `PARTY_LEVELCHANGE`/`STATUSCHANGE` (seviye/sınıf değişimi izlenmez), gizli oyuncu semantiği (ADR-0017 Eki F4-16 madde 3 açık soru, `[Ö]`), harita izi (F5).
- **Örnek betik kütüphanesi:** yalnızca `bots/config/script_smoke_2bot.txt` var; gerçek senaryo betikleri arena konumu ve envanter gerektirir (yukarıdaki ilk madde).
- Pot seçimi/karar, cast iptali + hareketle iptal + `UseStanding` otomatik durdurma, `Regene` tip 2 (taş), diriltme skill'i, chat için ASCII dışı metin (Q-16): karar katmanı ya da ilgili faz işi.

## 4. Çalıştırılan testler ve bekleyenler

| Test kimliği | Tekrar | Sonuç | Kanıt kaydı |
|---|---|---|---|
| Her F4 diliminin plan doğrulaması (çalışma zamanı S-serisi: gerçek sunucu, bot spawn, komut, log/telemetri çapraz denetimi) | F4-01..F4-22, 6 planda Tur 2 (F4-03, F4-08, F4-10, F4-18 dahil) | GEÇTİ | `plans/F4-NN-…` Doğrulama Raporları; `docs/STATUS.md` "Son doğrulamalar" |
| Birim testler (`BotCoreTests`) | Release + Debug | GEÇTİ: `82 tests, 0 failed` | `tools/run-tests.sh` |
| Algı sözleşmesi statik denetimi (`--selftest`, gerçek ağaç, enjeksiyon kopyası) | 1 | GEÇTİ: `RESULT: PASS`; enjeksiyonda rc=1 | `plans/F4-23-…` Doğrulama Raporu |
| MET-ACT-02 / MET-FAIR-01 gerçek koşu çapraz denetimi (`script_smoke`-türü koşu, `live-184432.jsonl`) | 1 koşu, 3 `ACTION_SUBMIT` | GEÇTİ: geçersiz aksiyon 0 (`PASS`); küçük örnek | `plans/F4-21-…` Doğrulama Raporu |
| T-ARCH-02, 06..12 (spawn, görünüm, hareket, saldırı, cast, pot, oturma, hedef HP, regene; gerçek istemci) | — | **GEÇTİ** (proje sahibi) | `docs/STATUS.md` "Proje sahibi test sonuçları" |
| T-MECH-DMG-01..03 (R, Type1, ateş büyüleri, Malice; iki gerçek istemci + bot) | — | **GEÇTİ** (± %15 içinde) | `docs/STATUS.md`; `Logs/DamageTrace_2_10_2026.log` |
| **T-ARCH-05** (F3-04 `+bot`), **T-ARCH-13..17**, **T-REGENE-01**, **T-PARTY-01..03**, **T-PERC-01** (CLI-19/20 `[A]` değerleri) | — | **BEKLİYOR (insan)** | `docs/STATUS.md` "Proje sahibi testleri (bekleyen)" |
| 16 bot ile `decisions` telemetri ek maliyeti ve açık/kapalı tick farkı (F3'ten devreden) | — | **YAPILMADI** | §5 |

## 5. Kabul kriterleri

| AC kimliği | Kriter | Ölçülen değer | GA (%95) | Karşılandı mı |
|---|---|---|---|---|
| (F4 kabul-1) / MET-ACT-02 | Betikli dizilerde sunucuya giden geçersiz aksiyon ≤ %1 | Araç hazır (F4-21). Gerçek koşuda 0/3 (`PASS`); geçersiz-aksiyon yolu yalnızca sentetik veriyle sınandı (`srv_fail`/`no_result` gerçek veride yok). **Anlamlı örnek (GA) yok**: betikli diziler henüz arena/envanter ile büyük ölçekte koşulmadı | — | **Kısmen** (araç + küçük örnek; büyük örnekle yeniden ölçülmeli) |
| (F4 kabul-2) / MET-FAIR-01 | Fairness ihlali (sunucuya ulaşan) 0 | Guard her aksiyonda ön denetim yapar; ihlal sunucuya ulaşmadan `FAIRNESS_REJECT` olur. Sunucuya ulaşan ihlal telemetriden ölçülemez (ADR-0017 Eki F4-21); gerçek istemci karşılaştırması CLI `[A]` ölçümleri (T-REGENE-01, T-PARTY-01..03, T-PERC-01) bekliyor | — | **Kısmen** (guard kuralları birim testli ve çalışma zamanında sınandı; `[A]` değerlerinin insan ölçümü açık) |
| (F4 kabul-3) / AC-LRN-03, AC-ARCH-06 | Sözleşme dışı algı erişimi 0 | Statik: `R1 0/0` (sıfır hoşgörü), `R2 0/28`, `R3 0/18`, `R4 0/0`, `R5 0/0`; yeni erişim rc=1 | — | **Evet (statik kısım)**; çalışma zamanı assert'i yapılmadı (§3) |
| (F4 kabul-4) / MET-PERF-02 | Tick bütçesi içinde | Komut pencerelerinde `tick_p95_us` 500'ü aşabildi (bütçe 5000 µs); 4 botla `tick_p95_us` ≤ 408 (F3-01); **12-16 bot yürürken ölçüm yok** | — | **Kısmen** (bütçenin çok altında ama hedef yükte ölçülmedi) |
| (F3'ten devreden) | `decisions` seviyesinde 16 bot için telemetri ek maliyeti ≤ MET-PERF-02'nin %10'u; açık/kapalı tick farkı | Ölçülmedi: bot tablosu 12 ile sınırlı (F2 §6 sapma 1); `decisions` olayları artık var (`ACTION_*`), ölçüm mümkün | — | **Hayır** (F4 sonu ölçümü; öneri: F5 başında küçük bir ölçüm planı) |

## 6. Sapmalar ve açıklamaları

1. **Sıra sapması:** F4 gece modunda F1, F2 ve F3 kabulü verilmeden başladı; bu fazların insan testleri açık.
2. **Kabul kriterleri kısmen kanıtlı:** MET-ACT-02 ve MET-FAIR-01 için araç ve küçük örnek var, ama istatistiksel güven için betikli büyük koşu (arena + envanter doldurma) yapılmadı (§3, §5). Proje sahibi, faz kabulünde bunu "F5/F6 başında ölçüm" koşuluna bağlayıp bağlamayacağına karar verir.
3. **T-MECH-SKILL bot yeniden koşusu ertelendi** (§3): bot `cast` kapsamı ve envanter doldurma önkoşul.
4. **Algı sözleşmesi çalışma zamanı denetimi yapılmadı** (§3).
5. **Otonom kararlar:** ADR-0017 ve tüm Ekleri (F4-02..F4-23) proje sahibi onayı olmadan kabul edildi; gözden geçirilmeli. Kapsamı etkileyen kararlar: R3 istisnasının geçici kabulü (F4-23), betiğin yeni yetki vermemesi ve yaşam döngüsü komutlarının betikte yasaklanması (F4-19), `no_np` reddi (F4-07), "ilk `PARTY_INSERT` kaydı = lider" `[A]` kuralı (F4-18).

## 7. Yeni bilinen sorunlar (KNOWN_ISSUES kimlikleri)

KI-012 (`+bot` yardım metinleri yeni fiilleri listelemiyor), KI-013 (NP'si 0 olan bot yeniden doğamaz), KI-014 (sunucu, daveti kabul etmeden hedefi "party'de" sayar; bot tarafı çözümü `PartyDecline`), KI-015 (`PartyRemove(memberid)` atmada üyelik denetlemez). Ayrıntılar `docs/KNOWN_ISSUES.md`. Gözlemler: F4-22'de dış kaynaklı maç sonunda `SCRIPT_END` maç bitiminden sonra düşebilir (nadir, plan gereği); `bot lost` yolunda `StopScript` iki kez çağrılır (ikincisi etkisiz); periyodik `BuildSnapshot` kurulursa tick maliyeti ayrıca ölçülmeli.

## 8. Bu fazda alınan kararlar (ADR kimlikleri)

ADR-0017 (aksiyon yürütücü ve adalet koruması) ve Ekleri F4-02..F4-23 (saldırı, cast, pot, duruş, hedef HP, regene, party kurulum/ret/devir, party chat, `Perception` dilimleri 1-7, betik ayrıştırıcı, `ScriptRunner`, MET-ACT-02/MET-FAIR-01 tanımı, senaryo–betik bağlaması, algı sözleşmesi denetimi). Hepsi "otonom döngüde Claude kararı — gözden geçirilmeli". ADR-0015 Ekleri (komut çekirdeği) F3'ten sürdü.

## 9. Doküman güncellemeleri

| Dosya | Bölüm | Özet |
|---|---|---|
| `docs/adr/ADR-0017-…` | Ekler F4-02..F4-23 | Yeni kararlar |
| `docs/03` | CLI-06, CLI-10, CLI-13..20, MEC-MAG-11, §16 NPC satırı | CLI değerleri ve `[A]`/`[V]` etiketleri |
| `docs/13` | komut tablosu | `/bot` yeni fiilleri |
| `docs/16` | §3.2 olay tablosu | `ACTION_*`, `FAIRNESS_REJECT`, `CHAT_SENT`, `SCRIPT_*` |
| `docs/KNOWN_ISSUES.md` | KI-012..KI-015 | Yeni sorunlar |
| `docs/STATUS.md`, `plans/README.md` | — | Plan durumları, bekleyen insan testleri |

## 10. Çıkış kararı

- [ ] Tüm çıkış koşulları karşılandı → KABUL_EDILDI
- [x] Kısmi → TEST_EDILDI olarak kalır; eksikler: insan testleri (T-ARCH-05, 13..17, T-REGENE-01, T-PARTY-01..03, T-PERC-01); MET-ACT-02/MET-FAIR-01'in büyük örnekle ölçümü; 12-16 bot yürürken tick bütçesi ve `decisions` telemetri ek maliyeti; T-MECH-SKILL bot yeniden koşusu (`cast` kapsam genişletmesi + envanter doldurma önkoşul); algı sözleşmesi çalışma zamanı denetimi — bunların F4'te mi F5/F6'da mı kapanacağına proje sahibi karar verir

## 11. Geri alma bilgisi

`[BOT] ENABLED=0` (varsayılan) ile bot sistemi kapalıdır; F4 kodu bu durumda çalışmaz ve sunucu davranışı değişmez. Kod olarak geri almak için `gece/2026-10-02` dalında F4-01..F4-23 birleştirmeleri (`a104ae3` … `671c1f6`, listesi `git log --merges --oneline | grep 'F4-'`) ters sırayla geri alınır.
