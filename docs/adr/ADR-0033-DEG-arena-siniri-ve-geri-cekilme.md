# ADR-0033-DEG: Arena sınırı, doğuş yolu ve geri çekilme (değerlendirme ajanında Claude kararı — gözden geçirilmeli)

Durum: ÖNERİLDI (arka plan değerlendirme ajanı kabul etti; proje sahibi gözden geçirecek)
Tarih: 2026-10-02 · Karar veren: Claude (değerlendirme ajanı, soru sorulamadı)
İlgili: docs/12 §7, §13.4; docs/11 §4.3; ADR-0004, K-6; F5-06/F5-07/F5-51; DEG-20

## Bağlam

- K-6: tek arena A (1274, 890), `P-ARENA-R` = 60 m; botlar arena dışına yol planlamaz (docs/12 §7). `AddForbidOutsideDisc` arenanın **dışını** yasaklı yapar; yasaklı hücreye dışarıdan girilemez.
- Doğan bot arena dışındadır (Karus 233 m, El Morad 640 m): doğuş yolu yasaklı bölgeden başlar ve `forbiddenPenalty` her adımı ~11× pahalılaştırır. Ölçüm (WSL g++): El Morad doğuşu → arena `NodeLimit` (20 000 düğüm, yol yok); arena içinden dışarıdaki doğuş noktasına hedef `InvalidGoal` (`docs/reports/degerlendirme-2026-10-02.md` §5.3).
- `docs/11` §4.3 solo geri çekilmeyi "kendi tower halkasına" ister; tower halkası arenanın dışındadır.

## Karar

1. Arena sınırı yalnızca **arenanın içindeki** bot için "dışarı çıkış yasak"tır. Dışarıdaki bot (doğuş, summon bekleme, dönüş) sınırın içine girebilir; dışarıda yasaklı-hücre cezası uygulanmaz.
2. **Arena modunda** geri çekilme güvenli noktası **arenanın içindedir** (party: arka hat; solo: arenanın kendi ulus yarısı). "Kendi tower halkasına çekil" yalnızca arena modu kapalıyken (serbest Ronark, F11) geçerlidir.
3. Doğuştan arenaya dönüş yürüyerek veya summon ile yapılır; arenaya girişten sonra "savaş alanında kal" kuralı başlar. Dönüş rotası ulus başına kısa ömürlü yol önbelleğinde tutulur.

## Değerlendirilen alternatifler

| Alternatif | Artılar | Eksiler | Neden seçilmedi |
|---|---|---|---|
| Arena dışını tamamen yasaklı bırak (mevcut) | Basit | Doğuş yolu `NodeLimit`/`InvalidGoal` | Ölçümle çürüdü |
| Arena sınırını yol maliyetinde yumuşak ceza yap | Her yer erişilebilir | Bot arenayı terk edebilir (K-6/AC kuralı) | Sınır sert olmalı |
| İçeriden-dışarıya yasak, dışarıda serbest (seçilen) | Sınır korunur, dönüş çalışır | Planlayıcıda iki bölgeli kural | — |

## Sonuçlar

Olumlu: ölüm → doğuş → dönüş döngüsü planlanabilir; geri çekilme arena içinde kalır. Olumsuz: Geri çekilme mesafesi 60 m'lik arenada kısıtlı (taktik etkisi F6/F7'de ölçülür). Geri alma: arena sınırı bayrağı.

## Doğrulama

F5-51 birim testi (iki ulus doğuşu → arena `Found`, düğüm ≤ 6000; arena içi bot dışarı çıkamaz); F5-55/T-NAV-05 çalışma zamanı: doğuş → arena yürüme süresi 52 sn (Karus) / 142 sn (El Morad) ± %20.
