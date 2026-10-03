# ADR-0010: Görev kapılı master skill'ler

Durum: KABUL · Tarih: 2026-10-01 · Karar veren: proje sahibi (oturumda tek tek soru-cevap ile)
İlgili: K-7, REQ-NEW-05

## Bağlam
Etc 510–523 satırları (warrior Hell blade/blooding/Berserker/Wall of Iron; mage armor ve staff'lar, Freezing Distance, Blink; priest Superior Parasite, Counter Curse, Discountis, Massive Binder, Superioris vb.) görev durumu istiyor. Görevlerin bu kurulumda tamamlanabilirliği doğrulanmadı (Q-04).

## Karar
İlk sürümde bu skill'ler kullanılmaz; profiller ([04](../04_CHARACTER_BUILDS_STATS_AND_EQUIPMENT.md) §5) bunlarsız kurulur. Görevler doğrulanınca ileri profil olarak eklenir (yeni ADR).

## Değerlendirilen alternatifler
| Alternatif | Artılar | Eksiler | Neden seçilmedi |
|---|---|---|---|
| Betikle görevleri tamamlanmış saymak | Skill'ler açık | Doğrulanmamış görev durumu | Seçilmedi |
| Görev şartını DB'den kaldırmak | Basit | Orijinal oyundan sapar | Seçilmedi |

## Sonuçlar
Profiller daha zayıf ama doğrulanabilir. MB-03 (mage armor yansıma hatası) ilk sürümde pratikte devre dışı kalır.

## Doğrulama
T-DATA-04 (ileri profil öncesi).

## Güncelleme (2026-10-03, ADR-0022, proje sahibi)

Bot karakter satırlarına görev durumu **yalnızca açık ad listesiyle ve geri alınabilir şekilde** (`db/003_bot_quests.sql` + rollback) yazılabilir; gerçek oyuncu kayıtları ve oyun mekaniği değişmez. "Betikle görevleri tamamlanmış saymak" alternatifi bot satırları için kabul edildi; "görev şartını DB'den kaldırmak" reddedilmiş kalır.
