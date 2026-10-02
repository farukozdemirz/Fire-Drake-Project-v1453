# ADR-0032-DEG: Senaryo başlangıç yerleşimi ve durum sıfırlama (değerlendirme ajanında Claude kararı — gözden geçirilmeli)

Durum: ÖNERİLDI (arka plan değerlendirme ajanı kabul etti; proje sahibi gözden geçirecek)
Tarih: 2026-10-02 · Karar veren: Claude (değerlendirme ajanı, soru sorulamadı)
İlgili: docs/15 §6a; CON-03 (gizli teleport yok), ARENA-05, MET-NAV-05; KI-013; F8; DEG-23

## Bağlam

- Bot durumunun bir kısmı DB'de kalıcıdır (despawn kaydı, AC-ARCH-05): konum, HP/MP/NP, envanter. Önceki maçın ölümleri NP'yi düşürür (KI-013: NP 0 → `Regene` yok), pot tüketimi envanterde kalır, MP respawn'da dolmaz.
- Başlangıç noktaları arena merkezinden ±35 m; doğuştan yürüyerek varmak 233 m (Karus) / 640 m (El Morad) sürer (taraf asimetrisi, K-6).
- CON-03: normal PK sırasında gizli teleport yok; eval modunda `TEST_TELEPORT` maçı geçersiz kılar.

## Karar

1. **Kurulum yerleşimi** (MATCH_START **öncesi**, `ScenarioRunner` Prepare'de bot satırına başlangıç konumunun yazılması/spawn sonrası tek yerleşim) izinlidir ve `TEST_TELEPORT` sayılmaz; maç **içinde** yapılan her ışınlama kurtarma teleportudur ve maçı geçersiz kılar.
2. Her maçın başında `docs/15` §6a tablosundaki durumlar sıfırlanır ve doğrulanır; doğrulama başarısızsa maç `SETUP_FAIL` ile başlamaz ve geçersiz sayılır.
3. Sıfırlama yalnızca **bot karakter satırlarına** yazar (kişisel veri tabloları okunmaz/yazılmaz, CLAUDE.md DB kuralı).

## Değerlendirilen alternatifler

| Alternatif | Artılar | Eksiler | Neden seçilmedi |
|---|---|---|---|
| Doğuştan yürüyerek başla | Gizli teleport yok | Her tekrar 4–10 dk; taraf asimetrisi başlangıcı kirletir; R-10 kapasite | Pratik değil |
| Maç içi `testtp` ile yerleşim | Basit | Eval'de maçı geçersiz kılar | Kural çelişkisi |
| Kurulum yerleşimi + doğrulama (seçilen) | Tekrarlanabilir, başlangıç ölçülebilir | Prepare'in DB yazma yetkisi ve doğrulama kodu gerekir | — |

## Sonuçlar

Olumlu: Her maç aynı koşuldan başlar; durum sızıntısı (NP/pot/MP) engellenir. Olumsuz: `ScenarioRunner` kapsamı büyür (F8 planı: envanter doldurma, DB yazımı, doğrulama). Geri alma: `ScenarioReset` bayrağı kapalıysa mevcut davranış (yalnız despawn/spawn).

## Doğrulama

T-IGT-EVAL-01: başlangıç doğrulaması %100, `SETUP_FAIL` oranı raporlanır; iki ardışık maçta `snap` değerleri eşit.
