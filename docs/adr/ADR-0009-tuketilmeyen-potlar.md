# ADR-0009: Tüketilmeyen pot verisi

Durum: KABUL · Tarih: 2026-10-01 · Karar veren: proje sahibi (oturumda tek tek soru-cevap ile)
İlgili: K-5, MB-01, KI-006, REQ-NEW-09

## Bağlam
360/720 HP ve 960/1920 MP potlarının NPC sürümlerinde `MAGIC.UseItem = 0`; sunucu potu ne kontrol ediyor ne tüketiyor `[V]`/`[D]`. Durum özgün veride de aynı.

## Karar
**Veri olduğu gibi kalır.** Kural insan ve bot için aynıdır. Bot kuralı (CLI-06): bot, envanterinde en az bir adet bulunan potu kullanır; pot cooldown'una (grup başına 2 sn) uyar. Tüketilen potlar (1440 HP vb.) normal azalır.

## Değerlendirilen alternatifler
| Alternatif | Artılar | Eksiler | Neden seçilmedi |
|---|---|---|---|
| DB'de `UseItem` düzeltmesi + bot sayacı | Gerçek kaynak yönetimi | Veri değişikliği | Seçilmedi |
| Yalnızca bot sayacı | Veri değişmez | İnsanlar sınırsız, botlar sınırlı; karşılaştırma bozulur | Seçilmedi |

## Sonuçlar
"Sınırsız kaynak yok" gereksinimi bu potlar için sunucu kuralı olarak kabul edildi; bota özel avantaj oluşmaz. Kaynak yönetimi davranışlarının etkisi bu potlarda pot cooldown'uyla sınırlıdır.

## Doğrulama
T-POT-01, T-MECH-POT-05 (istemcinin envanterde olmayan potu kullandırıp kullandırmadığı).
