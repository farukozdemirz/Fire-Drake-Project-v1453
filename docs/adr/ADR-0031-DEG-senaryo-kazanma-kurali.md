# ADR-0031-DEG: Senaryo kazanma kuralı (değerlendirme ajanında Claude kararı — gözden geçirilmeli)

Durum: ÖNERİLDI (arka plan değerlendirme ajanı kabul etti; proje sahibi gözden geçirecek)
Tarih: 2026-10-02 · Karar veren: Claude (değerlendirme ajanı, soru sorulamadı)
İlgili: docs/15 §6b; docs/16 MET-OUT-01/05; F8; `docs/reports/degerlendirme-2026-10-02.md` DEG-24

## Bağlam

- `ScenarioRunner` (F3-03) süre dolunca maçı `completed` kapatır (`MATCH_END.result` yalnızca `completed`/`aborted`); `MET-OUT-01` "senaryo hedefine göre" der ama hedef tanımsızdı. Süre dolması takımın kazandığı anlamına gelmez.
- Ronark'ta ölen bot ≥ 3 sn sonra yeniden doğabilir (CLI-14), bu yüzden "tüm takım aynı anda ölü" nadirdir.
- Hasar/isabet zarı ve sunucu rastgeleliği tek maçı gürültülü yapar; tekrar ve SPRT ile değerlendirilir (docs/14 §8).

## Karar

`win_rule` senaryo anahtarı: **`killdiff_timed`** (EVAL varsayılanı), `first_death` (1v1), `timed_score` (yalnız ölçüm). `killdiff_timed`: süre ilk hasardan (`engage`) işler (EVAL-8v8 300 sn); bitişte kill farkı ≥ +2 galibiyet, ≤ −2 yenilgi, |fark| ≤ 1 berabere (0,5); erken bitiş: WIPE veya fark ≥ takım büyüklüğü; hasar hiç olmazsa `invalid` (`NO_ENGAGE`). Kill = yalnız bot–bot PvP `DEATH`. `MATCH_END.result` ∈ `win_a|win_b|draw|invalid`.

## Değerlendirilen alternatifler

| Alternatif | Artılar | Eksiler | Neden seçilmedi |
|---|---|---|---|
| İlk takımın tamamen ölmesi (tek başına) | Basit | Yeniden doğuşla nadir, çoğu maç sonuçsuz | Ölçülemez sonuç |
| İlk N kill | Kısa maç | Seed gürültüsüne aşırı duyarlı, ilk patlama ödüllenir | Gürültülü |
| Süre sonunda kill farkı (seçilen) | Respawn'la uyumlu, her maç sonuç verir | Eşik/beraberlik bandı tasarım değeri `[Ö]` | — |
| Alan kontrolü | Hedef odaklı | Arena A'da kontrol noktası yok, ek mekanik | Kapsam dışı |

## Sonuçlar

Olumlu: `completed ≠ win`; MET-OUT-01 hesaplanabilir; WIPE/EVAL-WIPE senaryosu tanımlı. Olumsuz: Eşik (±2, beraberlik ±1) başlangıç hipotezi; F6/F7 pilot koşularında kalibre edilir (ADR güncellemesi). Geri alma: `win_rule` anahtarı; eski davranış `timed_score`.

## Doğrulama

F8 planında `ScenarioRunner` `win_rule` birim/entegrasyon testi; ilk 20 tekrarlı koşuda `invalid` oranı ≤ %10 (T-IGT-EVAL-01).
