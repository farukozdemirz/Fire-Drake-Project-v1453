# ADR-0002: Level 80 master karakterlerin oluşturulması

Durum: KABUL · Tarih: 2026-10-01 · Karar veren: proje sahibi (oturumda tek tek soru-cevap ile)
İlgili: K-2, K-3, REQ-NEW-04, F1–F2

## Bağlam
Bu kurulumda oyun içi sınıf yükseltme akışı yok; başlangıç sınıfı skill puanı dağıtamıyor; master skill'ler sınıf koduna birebir bağlı (MEC-CHR-01..03) `[D]`/`[V]`.

## Karar
Bot karakterleri sunucu kapalıyken bir DB kurulum betiğiyle oluşturulur: level 80, master sınıf kodu, stat (toplam 577, ≤ 255), skill puanları (142; ağaç ≤ 80, master ≤ 20), `strItem` (referans set), zone 71, NP > 0, WAREHOUSE satırı. Betik değişmezleri doğrular (T-DATA-01). **İnsan test hesapları da aynı betikle, aynı stat dağılımı ve aynı referans setle hazırlanır (K-3).**

## Değerlendirilen alternatifler
| Alternatif | Artılar | Eksiler | Neden seçilmedi |
|---|---|---|---|
| Oyuna sınıf atlama eklemek | Gerçekçi | Bot işinden bağımsız ek geliştirme | Kapsam |
| Yeni GM komutu | Sunucu açıkken ayar | Sunucu kodu değişir; tekrarlanabilirlik düşük | Tekrarlanabilirlik |

## Sonuçlar
Karakter verisi betikte tek kaynaktan gelir; test tekrarlanabilir. Girişte sunucu bu alanları doğrulamadığı için doğrulama betiğin sorumluluğundadır.

## Doğrulama
T-DATA-01, T-DATA-03; insan değerlendirme oturumunda karakterlerin set uyumu.
