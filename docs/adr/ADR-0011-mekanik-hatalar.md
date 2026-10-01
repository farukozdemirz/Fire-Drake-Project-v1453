# ADR-0011: Sunucu mekanik hataları

Durum: KABUL · Tarih: 2026-10-01 · Karar veren: proje sahibi (oturumda tek tek soru-cevap ile)
İlgili: K-8, MB-01..14

## Bağlam
Kod incelemesinde mekanik hatalar bulundu (AC debuff'ının çift uygulanması, isabet çarpanı tamsayı bölmesi, mage armor tam yansıma, ters çalışan item bonusları, Boldness) ([03](../03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md) §15).

## Karar
İlk sürümde hatalar **düzeltilmeden** oynanır; insan ve bot aynı kurallara tabidir. Hatalar KNOWN_ISSUES ve 03 §15'te tutulur; her düzeltme ayrı ADR ve `[MECH]` commit'iyle yapılır.

## Değerlendirilen alternatifler
| Alternatif | Artılar | Eksiler | Neden seçilmedi |
|---|---|---|---|
| Yalnızca bizi etkileyenleri düzeltmek | Daha anlamlı denge | Bot işi gecikir | Seçilmedi |
| Tümünü önce düzeltmek | Temiz mekanik | Kapsam büyür, denge değişir | Seçilmedi |

## Sonuçlar
Bot kararları mevcut (hatalı) kurallara göre ayarlanır. İleride bir hata düzeltilirse parametre yeniden ayarı gerekebilir.

## Doğrulama
T-MECH-DMG-02, T-MECH-12 ile etkilerin ölçülmesi.
