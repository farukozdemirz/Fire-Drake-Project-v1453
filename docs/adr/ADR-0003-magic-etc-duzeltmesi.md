# ADR-0003: MAGIC.Etc = 1 düzeltmesinin kalıcılığı

Durum: KABUL · Tarih: 2026-10-01 · Karar veren: proje sahibi (oturumda tek tek soru-cevap ile)
İlgili: K-4, KI-001, F1

## Bağlam
~1300 MAGIC satırı (tüm potlar, Sprint dahil) `Etc = 1` ile görev 1'in tamamlanmış olmasını istiyor; yeni karakterlerde görev yok, skill/pot 'failed' veriyor. Yerel DB'de `UPDATE MAGIC SET Etc = 0 WHERE Etc = 1` uygulanmış ve yedeği `MAGIC_BAK_etc` olarak duruyor `[V]` (1306 satır yalnızca Etc sütununda değişti).

## Karar
Düzeltme depoda geri alma adımı olan bir SQL betiği olarak tutulur ve her temiz kurulumda uygulanır. `Etc` 510–523 satırlarına dokunulmaz.

## Değerlendirilen alternatifler
| Alternatif | Artılar | Eksiler | Neden seçilmedi |
|---|---|---|---|
| Görev 1'i karakterlere işaretlemek | Orijinal veri korunur | Yeni insan karakterlerde sorun sürer | Kalıcılık |
| Yalnızca yerel bırakmak | İş yok | Yeniden kurulumda kaybolur | Tekrarlanabilirlik |

## Sonuçlar
Kurulum belgelenir ve tekrarlanabilir olur. Değişiklik `[MECH]` veri değişikliği olarak kaydedilir.

## Doğrulama
T-DATA-06.
