# Faz Sonuç Raporu — F3 Telemetri ve test altyapısı (TASLAK)

Tarih: 2026-10-02 · Hazırlayan: Claude (otonom gece döngüsü) · Onaylayan: — (bekliyor)
Değerlendirilen commit: `6122256` (`gece/2026-10-02`; F3-04 birleştirmesi) · Sunucu commit: `0f52027` + F1-01/F1-09 kancaları (bayrakla kapalı) + F2/F3 bot kodu (`[BOT] ENABLED=0` varsayılan) · DB özeti: yerel `FDP_kn_online`; 12 bot karakteri `db/002` ile kurulu

> **Durum: TASLAK.** DeepSeek'in yapabileceği F3 işleri bitti (F3-01..F3-06 `KAPANDI`). Faz kabulü için kalan işler: (a) **insan istemcisi testi** T-ARCH-05 (`+bot` oyun içi yolu), (b) iki **ölçüm maddesi karar motoru gelmeden yapılamaz** (aşağıda §3, §5). Bu rapor bu yüzden `KABUL_EDILDI` önermez: çıkış kararı §10'da **kısmi**. Gece modunda F4'e geçildi (F4-01 planı yazıldı), kabul bekleniyor.

## 1. Amaç (`docs/17` §2'den)

Her sonraki fazın ölçülebilir olması. Kapsam: telemetri kuyruğu + yazıcı thread (`docs/16`); `ScenarioRunner` (senaryo YAML, envanter doldurma, maç başlat/bitir); `+bot`/`/bot` komutları; `BotCore` statik kütüphane + birim test projesi; Python analiz aracı (JSONL → metrikler, harita izi). Kapsam dışı: davranış.

## 2. Teslim edilen kapsam

| İş kalemi | Durum | Commit(ler) / plan | Not |
|---|---|---|---|
| Olay şeması, sınırlı kuyruk, yazıcı thread, `PERF_SAMPLE` (ADR-0007) | GELIŞTIRILDI, TEST_EDILDI (çalışma zamanı) | F3-01 (merge `d73c295`) | Öz-sınama: 8192 kabul / 856 yumuşak / 952 sert düşürme, 8192 satır 130 ms'de yazıldı `[V]`; 4 botla ~95 sn'de 19 `PERF_SAMPLE`, hepsi geçerli JSON, `tick_p95_us` ≤ 408 (bütçe 5000), `dropped_*=0`; `off`/`bogus`/`ENABLED=0` dosya üretmez |
| `MATCH_START`/`MATCH_END`, `<match>.jsonl`, `summary.json`, `/bot match start\|end` | GELIŞTIRILDI, TEST_EDILDI | F3-02 (merge `f0dc266`) | Maç penceresinde `live-*`'de satır yok; `lines` = `events` toplamı; `CTRL_BREAK` ile `aborted` `[V]` |
| Telemetri analiz aracı (`tools/bot-telemetry-report.py`): JSONL → Markdown/JSON, MET-PERF-02 hükmü, geçersiz maç bayrakları | GELIŞTIRILDI, TEST_EDILDI | F3-06 (merge `aa5e325`) | Gerçek log verisiyle MET tablosu üretildi; `--selftest`, `--strict`, deterministik çıktı `[V]` |
| `ScenarioRunner`: senaryo YAML alt kümesi → bot spawn, seed/tekrar başına maç, despawn (`/bot scenario run\|stop\|status`) | GELIŞTIRILDI, TEST_EDILDI | F3-03 (merge `0660890`) | `smoke` (2 bot, seed 7/8) `completed` 12,07 sn, `finished: 2/2`, `pool free 16/16`; 16 ret durumu `[V]` |
| `BotCore` statik kütüphanesi + `BotCoreTests` mini çatı + belirlenimli `Rng` (ADR-0016) | GELIŞTIRILDI, TEST_EDILDI | F3-05 (merge `13526d9`) | `tools/run-tests.sh`: `6 tests, 0 failed` Release+Debug; bozulan `CHECK` ile çıkış kodu 1 `[V]` |
| Oyun içi GM komutu `+bot` (`spawn`/`despawn`/`match`/`scenario` kuyruğa, `list` anlık görüntüden) | GELIŞTIRILDI, TEST_EDILDI (sunucu tarafı) | F3-04 (merge `6122256`) | `list` biçimi konsol yoluyla aynı; oyun içi yol T-ARCH-05 (insan) bekliyor |

## 3. Kapsam dışında kalanlar / ertelenenler

- **`ScenarioRunner` envanter doldurma ve konum sıfırlama** (`docs/17` F3 kapsamı "envanter doldurma"): F3-03'te bilerek kapsam dışı bırakıldı. Pot/skill tüketen aksiyonlar (F4+) gelince gerekecek; **F4'ün ilgili planıyla birlikte** (UsePotion/Attack) ele alınmalı. Açık iş.
- **Python analiz aracında "harita izi"** (`docs/17` F3 kapsamı): F3-06 yalnızca MET tablosu ve geçersiz maç bayraklarını üretir; konum izi/ısı haritası, konum telemetrisi (`NAV_*`, hareket) olmadan anlamsızdır. F5 (navigasyon) ile birlikte eklenmeli. Açık iş.
- **`docs/17` F3 testi "telemetri açıkken/kapalıyken tick süresi farkı" ve Kabul "`decisions` seviyesinde 16 bot için telemetri ek maliyeti MET-PERF-02'nin %10'unu geçmez":** `decisions` olaylarını üreten karar/aksiyon katmanı henüz yok (`DECISION` yok; `ACTION_*` F4-01 ile gelir). Ölçüm F4-01 sonrası mümkün olur, 16 bot yürürken yapılmalı; bugünkü ölçüm yalnızca `summary` seviyesidir (`PERF_SAMPLE`) ve 12 botluk tablo sınırıyla sınırlıdır (F2 §6 sapma 1). **Açık iş.**
- `+bot why/pause/resume/policy/verbose/testtp` alt komutları (`docs/13` §10): karar motoru/politika yok; ilgili fazlarda.

## 4. Çalıştırılan testler ve bekleyenler

| Test kimliği | Tekrar | Sonuç | Kanıt kaydı |
|---|---|---|---|
| Telemetri yük/taşma öz-sınaması (F3 "sahte olaylarla yük testi") | 1 | GEÇTİ: sınırlar kesin (8192/856/952), yazıcı 130 ms | `plans/F3-01-…` Doğrulama Tur 1 |
| Maç bağlamı ve `summary.json` tutarlılığı | 4 maç | GEÇTİ | `plans/F3-02-…` Doğrulama Tur 1 |
| Analiz aracı örnek maçtan MET tablosu | 4 maç, gerçek log | GEÇTİ (`VALID`/`OK`, `Warnings (none)`) | `plans/F3-06-…` Doğrulama Tur 1 |
| `ScenarioRunner` uçtan uca | 2 koşu + `stop` + bot kaybı + 16 ret | GEÇTİ | `plans/F3-03-…` Doğrulama Tur 1 |
| Birim test çatısı (`run-tests.sh`) | Release + Debug + bozma denemesi | GEÇTİ | `plans/F3-05-…` Doğrulama Tur 2 |
| `+bot` işleyici (sunucu tarafı) | `cmd list`/`despawn all`, F2-05/F3-03 gerilemesiz | GEÇTİ | `plans/F3-04-…` Doğrulama Tur 1 |
| **T-ARCH-05** (oyun içi `+bot`, GM/GM olmayan hesap) | — | **BEKLİYOR (insan)** | `docs/STATUS.md` "Proje sahibi testleri (bekleyen)" |

## 5. Kabul kriterleri

| AC kimliği | Kriter | Ölçülen değer | GA (%95) | Karşılandı mı |
|---|---|---|---|---|
| (F3 kabul-1) | Düşürülen olay sayacı çalışıyor | `dropped_soft`/`dropped_hard` `PERF_SAMPLE`'da; öz-sınamada 856/952 sayıldı | — | Evet |
| (F3 kabul-2) | Analiz aracı örnek maçtan MET tablosu üretiyor | Gerçek logdan MET-PERF-02 tablosu ve hükmü | — | Evet |
| (F3 kabul-3) | `decisions` seviyesinde 16 bot için telemetri ek maliyeti MET-PERF-02'nin %10'unu geçmez | `summary` seviyesinde 4 botla `tick_p95_us` ≤ 408 µs (bütçe 5000 µs). `decisions`/16 bot ölçülmedi | — | **Hayır (ölçülemedi: karar/aksiyon olayı yok, 12 bot sınırı)** — F4-01 sonrası |
| (F3 testi) | Telemetri açık/kapalı tick farkı | Ölçülmedi (yalnızca açık durumda `p95`) | — | **Hayır** — F4-01 sonrası |

## 6. Sapmalar ve açıklamaları

1. **Sıra sapması:** F3 gece modunda F1 ve F2 kabulü verilmeden başladı; F1/F2 insan testleri açık.
2. **Kabul-3 ve açık/kapalı tick farkı ertelendi** (§3, §5): bunlar F3'ün "telemetri açıkken/kapalıyken tick" kabulünü çıkış koşulunda açık bırakır. Öneri: faz kabulünde bunu "F4 sonu ölçümü" koşuluna bağlamak (proje sahibi karar verir).
3. **Envanter doldurma ve harita izi** ertelendi (§3).
4. **Otonom kararlar:** ADR-0007, ADR-0015 Ekleri (F3-02, F3-03, F3-04), ADR-0016 proje sahibi onayı olmadan kabul edildi; gözden geçirilmeli.

## 7. Yeni bilinen sorunlar (KNOWN_ISSUES kimlikleri)

KI-011 (F3-06 notları, bkz. `docs/KNOWN_ISSUES.md`). Gözlemler: `PREPARE` aşamasında başka yolla despawn edilen bot 60 sn zaman aşımını bekler (F3-03 notu); test artıkları depo dışında (`C:\dev\fdp\server\Scenarios\`, `Logs\bots\`).

## 8. Bu fazda alınan kararlar (ADR kimlikleri)

ADR-0007 (telemetri formatı ve depolama; Ekleri F3-02), ADR-0015 Ekleri (F3-02 `match`, F3-03 `scenario`, F3-04 `+bot`), ADR-0016 (`BotCore` + birim test çatısı). Hepsi "otonom döngüde Claude kararı — gözden geçirilmeli".

## 9. Doküman güncellemeleri

| Dosya | Bölüm | Özet |
|---|---|---|
| `docs/adr/` | ADR-0007, 0015 Ekleri, 0016 | Yeni kararlar |
| `docs/16` | §3.3 | Telemetri uygulama notları (F3-01/F3-02) |
| `docs/13` | §4.4 | `[BOT]` ini anahtarları |
| `docs/STATUS.md`, `plans/README.md` | — | Plan durumları |

## 10. Çıkış kararı

- [ ] Tüm çıkış koşulları karşılandı → KABUL_EDILDI
- [x] Kısmi → TEST_EDILDI olarak kalır; eksikler: T-ARCH-05 insan testi; `decisions`/16 bot ek maliyet ölçümü ve açık/kapalı tick farkı (F4-01 sonrası); envanter doldurma ve harita izi (§3) — bunların F3'te mi F4/F5'te mi kapanacağına proje sahibi karar verir

## 11. Geri alma bilgisi

Telemetri seviyesi `off` (`[BOT] TELEMETRY=off`) veya `[BOT] ENABLED=0` (varsayılan) ile devre dışı. Kod olarak geri almak için `gece/2026-10-02` dalında `d73c295`, `f0dc266`, `aa5e325`, `0660890`, `13526d9`, `6122256` birleştirmeleri ters sırayla geri alınır.
