# ADR-0004: Test arenası

Durum: KABUL · Tarih: 2026-10-01 · Karar veren: proje sahibi (oturumda tek tek soru-cevap ile)
İlgili: K-6, REQ-TST-01, REQ-NEW-12, F1, F8

## Bağlam
Karus kapısının hemen önü guard tower menzilinde; tower'lar El Morad oyuncularına saldırıyor, oyuncular tower'lara saldıramıyor `[V]`/`[D]`. Analizde canavar ve tower menzilinden ≥ 120 m uzak iki aday bulundu: A (1274, 890) ve B (746, 1106) ([15](../15_TEST_ARENA_SCENARIOS_AND_ACCEPTANCE_CRITERIA.md) §2).

## Karar
Kontrollü testler ve değerlendirme maçları **yalnızca arena A**'da oynanır. Her seed iki kez, taraf değiştirilerek oynanır. Ölüm sonrası dönüş ve kazanma oranı ulus bazında ayrı raporlanır. B yedek aday olarak kayıtta kalır.

## Değerlendirilen alternatifler
| Alternatif | Artılar | Eksiler | Neden seçilmedi |
|---|---|---|---|
| A ve B çifti | Eve yakınlık avantajını dengeler | İki alanın doğrulanması, daha karmaşık otomasyon | Proje sahibi tek alanı tercih etti |
| Nötr respawn test modu | En adil | Oyun normalinden sapar | Seçilmedi |
| Doğrudan Karus kapısı önü | Kullanıcının ilk adayı | Sonucu tower'lar belirler | Önerilmedi |

## Sonuçlar
Karus tarafı ölüm sonrası savaşa daha çabuk döner (~233 m'ye karşı ~640 m). Bu etki taraf değişimiyle dağıtılır ve raporda ayrı gösterilir (R-04).

## Doğrulama
T-ENV-ARENA-01..04; ulus bazlı raporlarda farkın büyüklüğü.
