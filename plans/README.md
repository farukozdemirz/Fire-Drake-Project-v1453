# plans/ — Uygulama Planları

Bu klasör, **Claude**'un hazırladığı ve **opencode üzerinden DeepSeek**'in uyguladığı iş planlarını tutar. Her plan tek bir dosyadır ve küçük, doğrulanabilir bir iş birimini tarif eder.

## Roller

| Rol | Kim | Ne yapar |
|---|---|---|
| Karar verici | Proje sahibi | Planı onaylar, DeepSeek'e verir, doğrulama sonrası birleştirir |
| Planlayıcı ve denetçi | Claude (Claude Code, `/plan-olustur` ve `/plan-dogrula` skill'leri) | Planı yazar, ayrıntıları belirler, uygulanan işi doğrular |
| Uygulayıcı | DeepSeek (opencode, kurallar `AGENTS.md`'de) | Planı uygular, derler, raporunu plan dosyasına yazar |

## Akış

```
1. Claude: /plan-olustur          → plans/<PLAN-ID>.md   (durum: HAZIR)
2. Sen:    opencode'da DeepSeek'e  "plans/<PLAN-ID>.md planını uygula" de
3. DeepSeek: bot/<PLAN-ID> branch'i, commit'ler, "Uygulayıcı Raporu" bölümü  (durum: UYGULANDI)
4. Claude: /plan-dogrula plans/<PLAN-ID>.md  → "Doğrulama Raporu" bölümü
           → DOĞRULANDI  veya  DÜZELTME GEREKLİ (+ DeepSeek'e verilecek düzeltme talimatı)
5. DÜZELTME GEREKLİ ise: talimatı DeepSeek'e ver → 3'e dön (yeni tur)
6. DOĞRULANDI ise: sen branch'i main'e birleştirirsin → durum: KAPANDI
```

## Plan kimliği ve dosya adı

`<FAZ>-<NN>-<kisa-ad>.md`. Örnek: `F0-01-ortam-dogrulama.md`, `F2-03-bot-slot-havuzu.md`.

- FAZ: `docs/17_IMPLEMENTATION_ROADMAP_AND_PHASE_GATES.md`'deki faz (F0–F10).
- Branch: `bot/<FAZ>-<NN>` (ör. `bot/F2-03`).
- Commit mesajı: `[<FAZ>-<NN>] <özet>`.

## Plan durumları

| Durum | Kim koyar | Anlamı |
|---|---|---|
| `TASLAK` | Claude | Yazılıyor, verilmeye hazır değil |
| `HAZIR` | Claude | DeepSeek'e verilebilir |
| `UYGULANIYOR` | DeepSeek | Çalışma başladı |
| `UYGULANDI` | DeepSeek | Kod yazıldı, derlendi, rapor yazıldı. **İş bitti anlamına gelmez.** |
| `DÜZELTME GEREKLİ` | Claude | Doğrulamada eksik/hata bulundu; düzeltme talimatı planın sonunda |
| `DOĞRULANDI` | Claude | Planın kabul kriterleri kanıtla karşılandı |
| `KAPANDI` | Proje sahibi / Claude | Branch birleştirildi |
| `İPTAL` | Proje sahibi | Plan geçersiz |

Plan durumu ile faz durumu (`docs/21` §1) ayrıdır. Bir fazın `KABUL_EDILDI` olması için faz raporu gerekir. Tek tek planların `DOĞRULANDI` olması yetmez.

## Plan listesi

| Plan | Başlık | Faz | Durum | Branch |
|---|---|---|---|---|
| [F0-01](F0-01-ortam-dogrulama-araclari.md) | Ortam doğrulama araçları ve Debug/Release farkı raporu | F0 | KAPANDI (2026-10-01, `main` @ `43d3500`) | `bot/F0-01` |
| [F0-02](F0-02-sunucu-calistirma-betigi.md) | Sunucu çalıştırma betiği (start / stop / status) | F0 | KAPANDI (2026-10-02, `main` @ `93d0dcf`) | `bot/F0-02` (taban: `main`) |
| [F1-01](F1-01-paket-izleyici.md) | Paket izleyici (`FDP_PACKET_TRACE`, derleme bayrağıyla kapalı) ve özet betiği | F1 | KAPANDI (2026-10-02, `main` @ `93d0dcf`) | `bot/F1-01` (taban: `bot/F0-02`) |
| [F1-02](F1-02-zamanlama-oturumu-araclari.md) | Zamanlama oturumu araçları (`tools/trace-session.sh`, `packet-trace-summary.py --cli`) | F1 | KAPANDI (2026-10-02, `main` @ `017f88e`) | `bot/F1-02` (taban: `main`) |
| [F1-03](F1-03-magic-etc-sql-betigi.md) | `MAGIC.Etc = 1` düzeltmesi için kalıcı, geri alınabilir SQL betiği (ADR-0003, KI-001) | F1 | KAPANDI (2026-10-02, `main` @ `f4e4298`) | `bot/F1-03` (taban: `main`) |
| [F1-04](F1-04-bot-karakter-kurulum-betigi.md) | Level 80 bot karakter kurulum betiği: 12 karakter (6 profil × 2 ulus) + geri alma (ADR-0002, T-DATA-01) | F1 | KAPANDI (2026-10-02, `main` @ `0061930`) | `bot/F1-04` (taban: `main`) |
| [F1-05](F1-05-bot-ekipman-agirlik-raporu.md) | Bot ekipman uygunluğu ve ağırlık raporu (`tools/bot-gear-report.py`, MB-12/Q-21, Q-05 veri tarafı) | F1 | KAPANDI (2026-10-02, `main` @ `acc3adb`) | `bot/F1-05` (taban: `main`) |
| [F1-06](F1-06-hasar-modeli-ve-baslangic-hp.md) | Fiziksel hasar/istatistik hesaplayıcısı (`tools/stat-model.py`) ve bot başlangıç HP/MP düzeltmesi (`db/002`) | F1 | KAPANDI (2026-10-02, `main` @ `7ca8a12`) | `bot/F1-06` (taban: `main`) |
| [F1-07](F1-07-buyu-ve-heal-modeli.md) | Büyü hasarı ve heal modeli (`tools/spell-model.py`, mage/priest Type3) | F1 | KAPANDI (2026-10-02, `main` @ `d66935d`) | `bot/F1-07` (taban: `main`) |
| [F1-08](F1-08-arena-a-veri-dogrulamasi.md) | Arena A veri doğrulaması (`tools/arena-report.py`: spawn/tower payı, başlangıç ekseni, yürüme süresi, zone 71 zamanlayıcıları) | F1 | KAPANDI (2026-10-02, gece/2026-10-02) | `bot/F1-08` (taban: `main`) |
| [F1-09](F1-09-sunucu-hasar-kaydi.md) | Sunucu tarafı hasar kaydı (`FDP_DAMAGE_TRACE`, derleme bayrağıyla kapalı; T-MECH-DMG ölçüm altyapısı) | F1 | KAPANDI (2026-10-02, gece/2026-10-02) | `bot/F1-09` (taban: `gece/2026-10-02`) |
| [F1-10](F1-10-hasar-logu-ozet-betigi.md) | Hasar logu özet ve model karşılaştırma betiği (`tools/damage-trace-summary.py`, ± %15 hükmü, T-MECH-DMG analizi) | F1 | KAPANDI (2026-10-02, gece/2026-10-02) | `bot/F1-10` (taban: `gece/2026-10-02`) |
| [F2-01](F2-01-bot-alicisi-ve-slot-havuzu.md) | Bot alıcısı (`m_botSink`) ve ayrılmış oturum slot havuzu (S1+S2; `[BOT] ENABLED=0` varsayılan; öz-sınama) | F2 | KAPANDI (2026-10-02, gece/2026-10-02) | `bot/F2-01` (taban: `gece/2026-10-02`) |
| [F2-02](F2-02-bot-tick-iocp-olayi.md) | `BOT_TICK` IOCP olayı ve bot zamanlayıcı thread'i (S9, ADR-0005; `Tick()` boş, öz-sınama logu; `[BOT] ENABLED=0` varsayılan) | F2 | KAPANDI (2026-10-02, gece/2026-10-02) | `bot/F2-02` (taban: `gece/2026-10-02`) |
| [F2-03](F2-03-bot-girisi-spawn.md) | Bot girişi (spawn): hesap/karakter ataması, `WIZ_SEL_CHAR` DB isteği, `GameStart(1/2)` taklidi (S3, S7; ADR-0014; `[BOT] SPAWN_ON_START`, varsayılan kapalı) | F2 | KAPANDI (2026-10-02, gece/2026-10-02) | `bot/F2-03` (taban: `gece/2026-10-02`) |
| [F2-04](F2-04-bot-cikisi-despawn.md) | Bot çıkışı (despawn): `OnDisconnect`/`LogOut` taklidi, slotu DB kaydı bittikten sonra iade, `Update()` ve zaman aşımı muafiyeti (S4, S5, S8; `[BOT] DESPAWN_AFTER_SEC`, varsayılan kapalı) | F2 | KAPANDI (2026-10-02, gece/2026-10-02) | `bot/F2-04` (taban: `gece/2026-10-02`) |
| [F2-05](F2-05-bot-yeniden-spawn-dongusu.md) | Bot yeniden spawn döngüsü (`[BOT] RESPAWN_CYCLES`): slot iadesinden sonra aynı oturumla yeniden spawn, ilerleme/özet logları; T-PERF-06 (1000 spawn/despawn) altyapısı | F2 | KAPANDI (2026-10-02, gece/2026-10-02) | `bot/F2-05` (taban: `gece/2026-10-02`) |
| [F2-06](F2-06-bot-calisma-zamani-komutlari.md) | Bot çalışma zamanı komutları: konsol `/bot spawn\|despawn\|list` ve `BotCommands.txt` komut dosyası (ADR-0015; yalnızca `ENABLED=1`) | F2 | KAPANDI (2026-10-02, gece/2026-10-02) | `bot/F2-06` (taban: `gece/2026-10-02`) |
| [F3-01](F3-01-telemetri-kuyrugu-ve-yazici.md) | Telemetri kuyruğu, yazıcı thread ve `PERF_SAMPLE` (`[BOT] TELEMETRY`, ADR-0007; JSONL, `Logs/bots/`) | F3 | KAPANDI (2026-10-02, gece/2026-10-02) | `bot/F3-01` (taban: `gece/2026-10-02`) |
| [F3-02](F3-02-mac-baslangic-bitis-ve-summary.md) | Maç bağlamı: `MATCH_START`/`MATCH_END`, `<match>.jsonl` ve `summary.json` (`/bot match start\|end`; ADR-0007/0015 ekleri) | F3 | KAPANDI (2026-10-02, gece/2026-10-02) | `bot/F3-02` (taban: `gece/2026-10-02`) |
| [F3-06](F3-06-telemetri-analiz-araci.md) | Telemetri analiz aracı (`tools/bot-telemetry-report.py`): JSONL → Markdown rapor, MET-PERF-02 bütçe hükmü, geçersiz maç bayrakları (F3-03..05 numaraları ScenarioRunner/GM komutları/birim test çatısı için ayrılmıştır) | F3 | KAPANDI (2026-10-02, gece/2026-10-02) | `bot/F3-06` (taban: `gece/2026-10-02`) |
| [F3-03](F3-03-senaryo-kosucusu.md) | `ScenarioRunner`: senaryo dosyası (`./Scenarios/<ad>.yaml`, YAML alt kümesi) → bot spawn, seed/tekrar başına maç, despawn (`/bot scenario run\|stop\|status`; ADR-0015 Eki) | F3 | KAPANDI (2026-10-02, gece/2026-10-02) | `bot/F3-03` (taban: `gece/2026-10-02`) |
| [F3-05](F3-05-botcore-birim-test-catisi.md) | `BotCore` statik kütüphanesi + mini birim test çatısı (`BotCoreTests`, `tools/run-tests.sh`) ve belirlenimli `Rng` (ADR-0016) | F3 | KAPANDI | `bot/F3-05` (taban: `gece/2026-10-02`) |
| [F3-04](F3-04-gm-bot-komutlari.md) | Oyun içi GM komutu `+bot` (`spawn`/`despawn`/`match`/`scenario` komut kuyruğuna; `list` anlık görüntüden yanıtlanır; ADR-0015 Eki) | F3 | KAPANDI (2026-10-02, gece/2026-10-02) | `bot/F3-04` (taban: `gece/2026-10-02`) |
| [F4-01](F4-01-aksiyon-yurutucu-hareket.md) | `ActionExecutor` çekirdeği: `Move`/`Stop` aksiyonları gerçek `WIZ_MOVE` + `HandlePacket` ile, `BotFairnessGuard` hız/adım kuralı (`BotCore/BotMotion.h`, birim testli), `/bot move\|stop`, `ACTION_SUBMIT/RESULT`/`FAIRNESS_REJECT` telemetrisi (ADR-0017) | F4 | KAPANDI (2026-10-02, gece/2026-10-02) | `bot/F4-01` (taban: `gece/2026-10-02`) |
| [F4-02](F4-02-aksiyon-yurutucu-saldiri.md) | `ActionExecutor` saldırı dilimi: `Attack` (R) gerçek `WIZ_ATTACK` + `HandlePacket` ile, `BotFairnessGuard` menzil/CLI-01 aralık/CLI-11 aksiyon hızı kuralları (`BotCore/BotCombat.h`, birim testli), sonuç yayınlanan sonuç paketinden, `/bot attack`, `ACTION_*`/`FAIRNESS_REJECT` telemetrisi (ADR-0017 Eki) | F4 | KAPANDI (2026-10-02, gece/2026-10-02) | `bot/F4-02` (taban: `gece/2026-10-02`) |
| [F4-03](F4-03-aksiyon-yurutucu-cast.md) | `ActionExecutor` cast dilimi: `CastStart`/`CastEffect` (tek hedefli Type1/Type3 skill) gerçek `WIZ_MAGIC_PROCESS` + `HandlePacket` ile, `BotFairnessGuard` CLI-03/CLI-04/CLI-09/MEC-MAG-03/-08/-11 kuralları (`BotCore/BotCombat.h`, birim testli), sonuç yayınlanan sonuç paketinden, `/bot cast`, `ACTION_*`/`FAIRNESS_REJECT` telemetrisi (ADR-0017 Eki F4-03); uçan/alan/buff skill'leri, cast iptali ve pot kapsam dışı | F4 | HAZIR | `bot/F4-03` (taban: `gece/2026-10-02`) |

Şablon: [`_SABLON.md`](_SABLON.md)

Otonom döngü tasarımı (2026-10-01'de başlatıldı): [`OTONOM_DONGU.md`](OTONOM_DONGU.md)
