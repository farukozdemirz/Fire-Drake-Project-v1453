# ADR-0030-DEG: Öğrenme düzeyi: önce rol profili, sonra karakter bazlı kalıcı öğrenme (F12)

Durum: KABUL (proje sahibi, 2026-10-02: "ilk aşamada rol profili öğrenmesi ve oturum içi kestirim; karakter başına kalıcı öğrenme nihai hedefin parçası, kapsamdan çıkarma, sonraki faz olarak kaydet")
Tarih: 2026-10-02 · Karar veren: proje sahibi (ilk taslak: Claude değerlendirme ajanı)
İlgili: docs/14 §6, §6.1; docs/17 F9, F10, **F12**; `docs/reports/degerlendirme-2026-10-02.md` DEG-25, `docs/reports/degerlendirme-2026-10-02-ek.md` madde 1

## Bağlam

- Proje hedefi "deneyimlerinden gelişen botlar"dır; nihai hedefte **her karakterin kendi maç geçmişinden kalıcı olarak gelişmesi** vardır.
- `docs/14` §6 politikayı rol profili düzeyinde tutar (veri miktarı, aşırı uyum, tekrarlanabilirlik). Karakter başına veri az ve rakibe aşırı uyum riski yüksektir; ayrıca rol politikası kararlı olmadan karakter sapmasının neyin üstüne eklendiği belli değildir.

## Karar

Öğrenme **iki aşamada** ve üç düzeyde yürür; karakter bazlı kalıcı öğrenme kapsamdan **çıkarılmaz**, ayrı faz olarak planlanır:

| Aşama | Düzey | İçerik | Faz | Durum |
|---|---|---|---|---|
| 1 | Oturum içi kestirim (L0.5) | Tek botun/takımın rakip kestirimi (heal hızı, burst); maç sonunda silinir; öğrenme sayılmaz | F6–F7 | planlı |
| 1 | Rol profili politikası (L1/L2) | Aynı rolü oynayan **tüm** botların ortak sürümlü politikası | F9/F10 | planlı |
| 2 | **Karakter bazlı kalıcı öğrenme** | Karakter başına kalıcı, sınırlı sapma (`δ_c`) rol politikasının üstünde; kendi maç geçmişinden | **F12** (TASLAK, ADR kapılı) | kapsamda, faz taslağı `docs/17` |

F12 şu ilkelere bağlıdır (ayrıntı ve kabul kriterleri `docs/17` F12, `docs/14` §6.1):

1. **Rol politikası zemindir:** karakter politikası = rol politikası + sınırlı sapma; sapma izinli aralıkların bir alt bandındadır ve rol politikasına çekilir (shrinkage). `δ_c = 0` her zaman geçerli geri alma konumudur.
2. **Kabul kilitli sette:** karakter politikası, rol politikasını kilitli `evalset-v1`'de (B rakip grubu) SPRT ile geçmedikçe etkin olmaz; geçemeyen karakterde `δ_c = 0` kalır.
3. **Kalıcılık:** sürümlü, değişmez karakter politika dosyası (`PolicyStore`, karakter anahtarlı); DB şeması değişikliği ve insan verisi içermez (yalnız bot telemetrisi).
4. **Bağımlılık:** F9 (politika altyapısı, canary, otomatik geri alma, değerlendirme seti) kabul edilmiş olmalı; F9 "iyileşme yok" sonucuyla bitse bile F12 ayrı ADR ile değerlendirilir (rol düzeyinde iyileşme bulunmayan bir alanda karakter düzeyi iyileşmesi ayrıca kanıt ister).

## Değerlendirilen alternatifler

| Alternatif | Artılar | Eksiler | Sonuç |
|---|---|---|---|
| A. Yalnız rol profili, karakter düzeyi hiç yok | Basit | Nihai hedefin bir parçasını siler | Reddedildi (proje sahibi) |
| B. Rol politikasına çekilmiş sınırlı karakter sapması | Bireysellik + güvenlik; geri alma basit | Veri az → gürültü; test matrisi karakter sayısıyla çarpılır | **F12'de uygulanır** (aşama 2) |
| C. Karakter başına bağımsız politika | Tam bireysellik | Aşırı uyum, tekrarlanamaz | Reddedildi (`docs/14` §13) |

## Sonuçlar

- Olumlu: Hedef kaybolmaz; aşama 1 hemen yürür, aşama 2 temel üzerine kurulur; risk (aşırı uyum) kabul kapısıyla sınırlı.
- Olumsuz: F12 kabulü F9 sonrasına bağlıdır; karakter başına yeterli maç sayısı (kapasite R-10) ayrı sorun.
- Geri alma: tüm karakterlerde `δ_c = 0` (dosya işaretçisi değişikliği).

## Doğrulama

`docs/14` AC-LRN-01..08 (aşama 1); `docs/17` F12 AC-CHR-01..06 (aşama 2).
