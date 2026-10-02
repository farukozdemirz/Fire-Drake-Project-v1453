# ADR-0033-DEG: Arena sınırı, doğuş yolu ve geri çekilme: arena modu ile serbest Ronark modu ayrı

Durum: KABUL (proje sahibi, 2026-10-02: "kontrollü arena testlerinde geri çekilme arena içinde; serbest Ronark için güvenli konuma çekilme, yeniden gruplanma ve savaşa dönüş ayrıca planlansın")
Tarih: 2026-10-02 · Karar veren: proje sahibi (ilk taslak: Claude değerlendirme ajanı)
İlgili: docs/12 §7, §13.4; docs/11 §4.3; ADR-0004, K-6; F5-06/F5-07/F5-51; docs/17 F11; `docs/reports/degerlendirme-2026-10-02.md` DEG-20, `…-ek.md` madde 4

## Bağlam

- K-6: tek arena A (1274, 890), `P-ARENA-R` = 60 m; botlar arena dışına yol planlamaz (docs/12 §7). `AddForbidOutsideDisc` arenanın **dışını** yasaklı yapar; yasaklı hücreye dışarıdan girilemez.
- Doğan bot arena dışındadır (Karus 233 m, El Morad 640 m): doğuş yolu yasaklı bölgeden başlar ve `forbiddenPenalty` her adımı ~11× pahalılaştırır. Ölçüm (`tools/nav-measure.sh arena`): El Morad doğuşu → arena `NodeLimit` (20 000 düğüm, yol yok); arena içinden dışarıdaki doğuş noktasına hedef `InvalidGoal`.
- `docs/11` §4.3 solo geri çekilmeyi "kendi tower halkasına" ister; tower halkası arenanın dışındadır. İki ayrı kullanım var: kontrollü ölçüm (arena) ve nihai hedef (serbest Ronark).

## Karar

İki mod, iki ayrı davranış kümesi:

| | **Arena modu** (kontrollü test, F5–F8) | **Serbest Ronark modu** (F11) |
|---|---|---|
| Sınır | Arena içindeki bot dışarı yol planlamaz; dışarıdaki bot (doğuş, summon bekleme, dönüş) sınırın içine girebilir ve dışarıda serbest yürür, yasaklı-hücre cezası dışarıda uygulanmaz | Sınır yok; yalnızca düşman tower halkası yasaklı |
| Geri çekilme | **Arena içinde**: güvenli nokta arenanın içinde (party: arka hat; solo: arenanın kendi ulus yarısı) | Güvenli konuma çekilme: kendi tower halkası, dost konumu veya düşmansız bölge (`docs/11` §4.3 asıl metni) |
| Yeniden gruplanma | Arena içinde regroup noktası (`docs/09` §8) | Dağılmış party'nin arena dışında toplanması: **F11'de ayrıca planlanır** |
| Savaşa dönüş | Arenaya giriş sonrası "savaş alanında kal" | Düşmanın konumu bilinmiyor: arama + yeniden giriş kararı: **F11'de ayrıca planlanır** |
| Doğuştan dönüş | Yürüyerek veya summon; rota ulus başına önbellekte | Normal yürüyüş/summon, sınır yok |

Serbest Ronark için güvenli konuma çekilme, yeniden gruplanma ve savaşa dönüş davranışları `docs/17` F11 kapsamında ayrı alt kalemlerdir (F11-a/b/c) ve kendi kabul testlerine sahiptir (T-FREE-06..08); arena modundaki sonuçlar onların yerine geçmez.

## Değerlendirilen alternatifler

| Alternatif | Artılar | Eksiler | Neden |
|---|---|---|---|
| Arena dışını tamamen yasaklı bırak (mevcut) | Basit | Doğuş yolu `NodeLimit`/`InvalidGoal` | Ölçümle çürüdü |
| Arena sınırını yumuşak yol cezası yap | Her yer erişilebilir | Bot arenayı terk edebilir (K-6) | Sınır sert olmalı |
| İçeriden-dışarıya yasak, dışarıda serbest (arena modu, seçilen) | Sınır korunur, dönüş çalışır | Planlayıcıda iki bölgeli kural | Seçildi |

## Sonuçlar

Olumlu: ölüm → doğuş → dönüş döngüsü planlanabilir; arena modunda geri çekilme ölçülebilir; serbest mod davranışları ayrı kabulle kaybolmaz. Olumsuz: 60 m'lik arenada geri çekilme mesafesi kısıtlı (taktik etkisi F6/F7'de ölçülür); F11 kapsamı büyür. Geri alma: arena sınırı bayrağı.

## Doğrulama

F5-51 birim testi; F5-55 çalışma zamanı: **her iki ulus** için ölüm → respawn → arenaya dönüş (T-NAV-05/T-NAV-10; Karus ~52 sn, El Morad ~142 sn ± %20); F11 testleri T-FREE-06..08.
