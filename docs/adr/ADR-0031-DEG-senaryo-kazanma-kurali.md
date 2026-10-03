# ADR-0031-DEG: Senaryo kazanma kuralları: `killdiff_timed` ve ayrı tür `wipe_first`

Durum: KABUL (proje sahibi, 2026-10-02: "respawn içeren süreli maçlarda kill farkı; ilk takımın tamamen ölmesiyle biten maçlar ayrı senaryo türü; ±2/±1 örneklerle kesinleşsin; eşikler yapılandırılabilir")
Tarih: 2026-10-02 · Karar veren: proje sahibi (ilk taslak: Claude değerlendirme ajanı)
İlgili: docs/15 §6b; docs/16 MET-OUT-01/05; F8; `docs/reports/degerlendirme-2026-10-02.md` DEG-24, `…-ek.md` madde 2

## Bağlam

- `ScenarioRunner` (F3-03) süre dolunca maçı `completed` kapatır; `MATCH_END.result` yalnızca `completed`/`aborted`'tir; `MET-OUT-01` "senaryo hedefine göre" der ama hedef tanımsızdı. Süre dolması kazanma değildir.
- Ronark'ta ölen bot ≥ 3 sn sonra yeniden doğabilir (CLI-14): respawn açıkken "tüm takım aynı anda ölü" nadirdir; respawn kapalıyken anlamlıdır.
- Sunucu rastgeleliği tek maçı gürültülü yapar; değerlendirme tekrar + taraf değişimi + SPRT ile yapılır.

## Karar: iki ayrı senaryo türü + ölçüm modu

### Ortak tanımlar

- **Kill:** maç içinde, bir takımın üyesinin diğer takımın üyesini öldürmesi (`DEATH` olayında öldüren = rakip takım botu). Canavar/kule/bilinmeyen kaynak/intihar sayılmaz.
- `K_A`, `K_B`: A ve B takımının kill sayıları. `fark = K_A − K_B` (tamsayı).
- **Başlangıç (`engage`):** maçtaki ilk hasar olayı. Süre `engage` anından işler. `engage_timeout_sec` (varsayılan 60) içinde hasar yoksa maç **geçersiz** (`invalid`, neden `NO_ENGAGE`).
- Sonuç kodu `MATCH_END.result` ∈ `win_a` | `win_b` | `draw` | `invalid` | `no_result` (yalnız `timed_score`). Kazanma oranına `draw` 0,5, `invalid` payda dışı (MET-OUT-01).

### Tür 1 — `killdiff_timed` (respawn **açık**; EVAL varsayılanı)

Parametreler (senaryo dosyasında, yoksa varsayılan): `duration_sec` (EVAL-8v8: 300; 2v2..5v5: 120), `win_margin` (varsayılan **2**), `early_end_margin` (varsayılan **0 = kapalı**).

| Kural | Sonuç |
|---|---|
| `fark >= win_margin` | **`win_a`** (A galip, B mağlup) |
| `fark <= −win_margin` | **`win_b`** (B galip, A mağlup) |
| `\|fark\| < win_margin` | **`draw`** (varsayılan `win_margin = 2`: fark −1, 0, +1) |
| `early_end_margin > 0` ve `\|fark\| >= early_end_margin` | süre dolmadan o anda bitir, yukarıdaki kuralla sonuçlandır |
| `duration_sec` dolunca | yukarıdaki kural (süre dolması kendiliğinden kazanma değildir) |

Örnekler (8v8, 300 sn, `win_margin = 2`):

| K_A | K_B | fark | Sonuç | Neden |
|---|---|---|---|---|
| 14 | 11 | +3 | `win_a` | ≥ +2 |
| 10 | 8 | +2 | `win_a` | sınır dahil (≥ 2) |
| 9 | 8 | +1 | `draw` | \|fark\| < 2 |
| 8 | 8 | 0 | `draw` | eşit |
| 8 | 9 | −1 | `draw` | \|fark\| < 2 |
| 7 | 9 | −2 | `win_b` | ≤ −2 |
| 3 | 12 | −9 | `win_b` | |
| 0 | 0 | 0 | `draw` (hasar vardı) / `invalid` (`NO_ENGAGE`, hasar hiç yoktu) | |

`win_margin = 3` ile aynı maçlar: (14, 11) +3 `win_a`; (10, 8) +2 `draw`; (7, 9) −2 `draw`. Küçük takım (2v2, 120 sn, margin 2): (3, 1) `win_a`; (2, 1) `draw`.

### Tür 2 — `wipe_first` (ilk tam yok oluşla biten; respawn **kapalı**)

Parametreler: `duration_sec` (varsayılan 300), `respawn` (varsayılan **off**: ölen bot `WIZ_REGENE` göndermez, ölü kalır; `on` verilirse tür anlamını yitirir ve doğrulama reddeder), `resurrection` (varsayılan off).

| Durum | Sonuç |
|---|---|
| Bir takımın **tüm** üyeleri ölü (canlı sayısı 0), diğerinin canlısı var | diğer takım galip (`win_a`/`win_b`), maç o anda biter |
| Her iki takım da aynı tick (≤ 100 ms) içinde 0 canlıya iner | `draw` |
| Süre dolunca wipe yok | **canlı üye sayısı** fazla olan galip; eşitse `draw` |
| 1v1 | tür özel durumu: ilk ölen kaybeder (`wipe_first` ve boyut 1) |

Örnekler (8v8): B'nin son botu ölür, A'da 3 canlı → `win_a`; süre dolar, A 4 canlı B 2 canlı → `win_a`; A 3 / B 3 → `draw`; iki takımın son botları aynı tick'te ölür → `draw`.

### Tür 3 — `timed_score` (yalnız ölçüm)

Kazanan ilan edilmez (`no_result`); kill/death, hasar, heal metrikleri raporlanır. Mekanik/davranış testleri (T-WAR/T-PRI/T-MAG) için.

### Yapılandırılabilirlik ve pilot kalibrasyonu

Eşikler kodda sabit değildir; senaryo anahtarları: `win_rule`, `duration_sec`, `win_margin` (1..8), `early_end_margin` (0 veya ≥ `win_margin`), `engage_timeout_sec`, `respawn`. Değerler `MATCH_START` olayına yazılır (tekrar edilebilirlik). **Pilot:** `baseline-v1` aynı-aynıya, 20 maç (taraf değişimli): `fark` dağılımı (ortalama, standart sapma) ve beraberlik oranı raporlanır; hedef beraberlik oranı %15–35 (daha yüksekse `win_margin` düşürülür, daha düşükse artırılır). Değişiklik bu ADR'ye ek (pilot tablosuyla) olarak yazılır; kod değişmez.

## Değerlendirilen alternatifler

| Alternatif | Artılar | Eksiler | Neden |
|---|---|---|---|
| Yalnız "ilk takımın tamamen ölmesi" | Basit | Respawn açıkken nadir/ölçülemez | Ayrı tür olarak (`wipe_first`, respawn kapalı) tutuldu |
| İlk N kill | Kısa maç | Seed gürültüsüne duyarlı | Seçilmedi |
| Süre sonunda kill farkı (seçilen, `killdiff_timed`) | Respawn'la uyumlu, her maç sonuç verir | Eşik tasarım değeri | Seçildi, eşikler yapılandırılabilir |
| Alan kontrolü | Hedef odaklı | Arena A'da kontrol noktası yok | Kapsam dışı |

## Sonuçlar

Olumlu: `completed ≠ win`; MET-OUT-01 hesaplanabilir; WIPE ve süreli türler ayrı ölçüm yapar. Olumsuz: `wipe_first` için bot yürütücüsüne "respawn kapalı" modu gerekir (F8 planı); eşikler pilotla kalibre edilmeli. Geri alma: `win_rule: timed_score`.

## Doğrulama

F8 planında `ScenarioRunner` `win_rule` birim/entegrasyon testi (yukarıdaki örnek tabloları birim test vektörü olarak); ilk 20 tekrarlı pilotta `invalid` ≤ %10 ve beraberlik oranı raporu (T-IGT-EVAL-01).

## Ek F8-01: Sonuç değerlendiricisi girdi sözleşmesi ve ADR'nin tanımlamadığı noktalar (otonom döngüde Claude kararı — gözden geçirilmeli)
Tarih: 2026-10-03 · Plan: `plans/F8-01-bot-sonuc-degerlendirici.md` · Dayanak: bu ADR (kurallar ve örnekler), `docs/15` §6b, `docs/16` §3.2/§3.3, MET-OUT-01/05.

1. **Araç ayrı bir oracle'dır `[Ö]`:** `tools/bot-outcome-eval.py` kuralları bu ADR'den **bağımsız** gerçekler: telemetri olaylarından maç sonucunu hesaplar. Sunucu tarafı (`ScenarioRunner` `win_rule`) yazıldığında iki gerçekleme birbirini denetler. Bu ADR'nin örnek tabloları ve `docs/15` §6b örnekleri aracın `--selftest` vakalarıdır; kural yanlış okunursa araç kendini yakalar.
2. **Girdi sözleşmesi `[A]` (alan adları `docs/16` §3.3'te):** `MATCH_START` takım üyeliğini `team_a`/`team_b` (bot kimliği listeleri) ve isteğe bağlı kural parametrelerini (`win_rule`, `duration_sec`, `win_margin`, `early_end_margin`, `engage_timeout_sec`) taşır; `DAMAGE` yalnızca `t` ile `engage`'i belirler; `DEATH` olay sahibi (`bot`) ölendir ve `killer` öldürenin birim kimliğidir (`-1`/yok = bilinmeyen/çevre). `docs/16` §3.2 alan adlarını yalnızca "öldüren, hedef" diye tarif ediyordu; sunucu bugün bu olayları yazmıyor, dolayısıyla bu sözleşme sunucu planına gereksinimdir (ayrı plan), gerçek log bununla uyuşmazsa **sözleşme** değil emisyon düzeltilir ya da bu Ek güncellenir.
3. **ADR'nin tanımlamadığı noktalar `[A]`:** (a) süre `engage`'den işler (ortak tanım) ve **her üç türde** geçerlidir; (b) sınırlar dahildir: `fark == win_margin` galibiyet, `t == engage + duration` sayılır, `engage − start == engage_timeout` geçerli; (c) `duration_sec` varsayılanı `killdiff_timed`'de takım boyutu ≥ 6 ise 300, değilse 120 (ADR yalnızca 8v8 ve 2v2..5v5'i söyler), `wipe_first`/`timed_score`'da 300; (d) `wipe_first` "aynı tick" = `tick_ms` (100) içinde; 1v1'de ilk ölen kaybeder, yalnızca **aynı `t`** beraberliktir; (e) `win_margin`/`early_end_margin` kısıtları bu ADR'nindir (1..8; 0 ya da ≥ `win_margin`), `duration_sec` ve `engage_timeout_sec` için 1..3600 üst sınırı eklenir.
4. **Ek geçersizlik nedenleri `[A]`** (`docs/16` MET-OUT-05 `NO_ENGAGE`/`SETUP_FAIL`/`TEST_TELEPORT`/`THIRD_PARTY` listesine ek): `ABORTED` (`MATCH_END.result = aborted`, sunucu kapanışı), `TRUNCATED` (kayıt maç sonlanmadan kesilmiş: wipe/erken bitiş yok ve gözlem süre dolmadan bitmiş; yalnızca başka neden yokken), `RESPAWN_IN_WIPE_FIRST` (bu ADR "respawn on olursa doğrulama reddeder" der; çalışma zamanında gözlenirse sonuç geçersiz). Öncelik: `SETUP_FAIL` > `ABORTED` > `TEST_TELEPORT` > `NO_ENGAGE` > `RESPAWN_IN_WIPE_FIRST` > `TRUNCATED`. `THIRD_PARTY` için olay şeması yok; ertelendi.
5. **Bilinçli ertelenen:** toplulaştırma (MET-OUT-01 Wilson, `fark` ortalaması/standart sapması, pilot kalibrasyonu, SPRT), taraf değişimi eşleştirme, sunucu tarafı emisyon ve `ScenarioRunner` `win_rule`, "respawn kapalı" yürütücü modu.
