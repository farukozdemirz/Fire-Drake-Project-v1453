# ADR-0030-DEG: Öğrenme düzeyi: rol profili (değerlendirme ajanında Claude kararı — gözden geçirilmeli)

Durum: ÖNERİLDI (arka plan değerlendirme ajanı kabul etti; proje sahibi gözden geçirecek)
Tarih: 2026-10-02 · Karar veren: Claude (değerlendirme ajanı, soru sorulamadı)
İlgili: docs/14 §6, §6.1; F9, F10; `docs/reports/degerlendirme-2026-10-02.md` DEG-25

## Bağlam

- Proje hedefi "deneyimlerinden gelişen botlar" üretmektir (kullanıcı hedefi).
- `docs/14` §6 politikayı **rol profili** düzeyinde tutar, karakter düzeyinde tutmaz `[Ö]`; ama bu ayrım proje sahibi tarafından bir K-kararı olarak kayıtlı değildi ve "her bot oynadıkça ustalaşır" beklentisiyle örtüşmediği değerlendirmede işaretlendi.
- Maç başına süre dakikalar; 8v8 değerlendirme kapasitesi sınırlı (R-10). Karakter başına veri az ve rakibe aşırı uyum riski yüksek (docs/14 §6, §7.3).

## Karar

Öğrenme **üç ayrı düzeyde** tanımlanır ve ayrı ölçülür (docs/14 §6.1):

1. **Oturum içi kestirim (L0.5)**: tek bot/takım, kısa pencere; maç sonunda silinir; "öğrenme" sayılmaz.
2. **Rol profili politikası (L1/L2)**: aynı rolü oynayan **tüm** botlar aynı sürümlü politikayı paylaşır; asıl öğrenme budur (F9/F10).
3. **Karakter düzeyi kalıcı politika: planlanmaz.** Yalnızca telemetri/teşhis.

"Oynadıkça ustalaşma" beklentisi filo düzeyinde (toplam deneyim) karşılanır; bireysel kalıcı öğrenme proje sahibinin ayrı onayını gerektirir.

## Değerlendirilen alternatifler

| Alternatif | Artılar | Eksiler | Neden seçilmedi |
|---|---|---|---|
| A. Yalnız rol profili (seçilen) | Veri çoğalır, genelleme, açıklanabilir, tekrarlanabilir | Bireysel "karakter gelişimi" yok | — (seçildi) |
| B. Rol politikasına çekilmiş sınırlı karakter sapması (shrinkage; karakter başına ±δ, `PolicyStore`'da karakter anahtarlı) | Bireysel farklılık, "kişilik" | Veri az → gürültü, rakibe aşırı uyum, test matrisi 12–20 karakterle çarpılır, geri alma karmaşık | F9 sonucu görülmeden erken; F10 sonrası ADR ile açılabilir |
| C. Karakter başına bağımsız politika | Tam bireysellik | Veri çok az, ezberleme (docs/14 §13 yasaklar) | Hedefle ve §6 gerekçesiyle çelişir |

## Sonuçlar

- Olumlu: F9 hedefi net; AC-LRN-08 ile başarı ayrı ölçülür; kapsam şişmez.
- Olumsuz: Kullanıcı bireysel kalıcı öğrenme bekliyorsa beklenti karşılanmaz (doküman bunu açıkça söyler).
- Geri alma: B seçeneği ayrı ADR ile F10'da eklenebilir; mevcut politika dosyası biçimi karakter anahtarını sonradan alabilir.

## Doğrulama

`docs/14` AC-LRN-01..08; F9 raporunda "iyileşme var/yok" kanıtı; proje sahibinin bu ADR'yi onaylaması veya B'yi seçmesi.
