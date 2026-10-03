# ADR-0020: `B0-NAIVE` karşılaştırma botu ve "anlamlı yener" başarı eşiği (Q-30)

Durum: KABUL (başarı eşiği); `B0-NAIVE` tanımı ÖNERİLDİ (F6-01'de `[A]`, proje sahibi plan incelemesinde onaylar)
Tarih: 2026-10-03 · Karar verenler: proje sahibi (soru-cevap oturumu)
İlgili: F6-01 (`B0-NAIVE`), F6-10 (`EVAL-1v1`), `docs/15` §5, `docs/17` F6 kabulü, açık soru Q-30

## Bağlam

`docs/17` F6 kabulü: "L0 politikası `B0-NAIVE`'i EVAL-1v1'de **anlamlı** yener" `[D]`. "Anlamlı" tanımsızdı; `AC-EVAL-01` (SPRT, +100 Elo) yalnızca 8v8 içindir. `B0-NAIVE`'in kapsamı da (pot %30'da HP için mi MP için mi, priest/mage için skill kümesi) yazılı değildi (`docs/reports/plan-bagimlilik-F6-2026-10-03.md` §4 madde 3).

## Karar

1. **Başarı eşiği:** L0 politikası, aynı sınıf ve ekipmanla `B0-NAIVE`'e karşı **40 maçta en az %65** galibiyet oranına ulaşır (beraberlik yarım galibiyet; kazanma kuralı ADR-0031-DEG). 40 maç aynı koşuda yapılır; başlangıç yerleşimi ve durum sıfırlaması `ScenarioReset` (F8-05) ile sağlanır.
2. **`B0-NAIVE` tanımı** (F6-01 planında `[A]` olarak yazılır, proje sahibi plan incelemesinde onaylar): hiçbir karar zekâsı yoktur; en yakın düşmanı hedefler, ona yürür, sınıfının temel saldırısını (warrior: R) kullanır, HP %30'un altına düşünce HP pot içer, geri çekilmez, kaçmaz.

## Değerlendirilen alternatifler

| Alternatif | Artılar | Eksiler | Neden seçilmedi |
|---|---|---|---|
| 20 maçta en az %70 | Yarı sürede ölçüm | Şans eseri kazanma ihtimali yüksek (20 maçta %70 güven aralığı alt sınırı ≈ %48) | Güvenilirlik tercih edildi |

## Sonuçlar

Olumlu: sayısal, tekrarlanabilir kabul ölçütü. Olumsuz: 40 maçlık koşu zaman alır; `ScenarioReset` ve sunucu maç olayları (F8-05/F8-06) olmadan resmî ölçüm yapılamaz. Risk: eşik 1v1 içindir, 8v8 kabulü (`AC-EVAL-01`) ayrıdır. Geri alma: eşik `docs/15` §5 parametresidir.

## Doğrulama

F6-10 EVAL-1v1 koşusu: `tools/bot-outcome-eval.py` çıktısında L0 ↔ `B0-NAIVE` 40 maç, galibiyet oranı ≥ 0,65 (beraberlik 0,5); koşu `ScenarioReset` ile başlatılmış olmalı.
