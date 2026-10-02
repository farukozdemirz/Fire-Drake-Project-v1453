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
| [F2-01](F2-01-bot-alicisi-ve-slot-havuzu.md) | Bot alıcısı (`m_botSink`) ve ayrılmış oturum slot havuzu (S1+S2; `[BOT] ENABLED=0` varsayılan; öz-sınama) | F2 | DOĞRULANDI | `bot/F2-01` (taban: `gece/2026-10-02`) |

Şablon: [`_SABLON.md`](_SABLON.md)

Otonom döngü tasarımı (2026-10-01'de başlatıldı): [`OTONOM_DONGU.md`](OTONOM_DONGU.md)
