# ADR-0013: Upstream PR #10

Durum: KABUL · Tarih: 2026-10-01 · Karar veren: proje sahibi (oturumda tek tek soru-cevap ile)
İlgili: K-10, R-13

## Bağlam
Upstream'de birleştirilmemiş PR #10 (2021) eksik bir `break;` düzeltmesi, yakındaki kullanıcı listesini cevap başına 10 kişiyle sınırlama ve riskli bir gönderme kodu yeniden yazımı içeriyor ([19](../19_SOURCES_AND_EVIDENCE.md) R-PR10).

## Karar
PR #10 **hiç alınmaz**, küçük düzeltme dahil.

## Değerlendirilen alternatifler
| Alternatif | Artılar | Eksiler | Neden seçilmedi |
|---|---|---|---|
| Yalnızca küçük düzeltmeyi almak | Bilinen bir kopma hatası kapanır | — | Seçilmedi |
| Tamamını almak | — | 8v8'de görünmeyen oyuncu, çökme riski | Seçilmedi |

## Sonuçlar
`WIZ_LOGOSSHOUT` durumundaki eksik `break;` hatası ([`GameServer/User.cpp:457-462`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L457-L462)) depoda kalır; bu paketi gönderen istemci koparılabilir. Botlar bu paketi kullanmaz.

## Doğrulama
—
