# ADR-0001: Botların temsil katmanı

Durum: KABUL · Tarih: 2026-10-01 · Karar veren: proje sahibi (oturumda tek tek soru-cevap ile)
İlgili: K-1, REQ-ARC-01, F2

## Bağlam
Botların oyunda oyuncu gibi görünmesi, party/chat kullanması ve PvP kurallarına tabi olması gerekiyor. Üç seçenek değerlendirildi ([02](../02_REPOSITORY_ANALYSIS_AND_INTEGRATION_MAP.md) §10): sunucu içi soketsiz oyuncu, harici sahte istemci, NPC tabanlı bot. NPC görünüm paketi ırk/sınıf/zırh taşımıyor; party üyeleri oturum kimliğiyle tutuluyor; PvP formülleri yalnızca oyuncu hedefe uygulanıyor `[D]`.

## Karar
Botlar GameServer içinde, ayrılmış oturum slotlarında (en üst kimlik aralığı) soketsiz `CUser` olarak çalışır. `CUser`'a bot alıcısı (`m_botSink`) eklenir; aksiyonlar gerçek paketler olarak IOCP thread'inde mevcut handler'lardan geçer ([13](../13_BOT_ARCHITECTURE_AND_DATA_MODEL.md) §2–4).

## Değerlendirilen alternatifler
| Alternatif | Artılar | Eksiler | Neden seçilmedi |
|---|---|---|---|
| Harici sahte istemci | Sunucu değişmez, en yüksek sadakat | JvCryption/CRC/sıra, dünya durumu ayrıştırma, süreç yönetimi; çok büyük iş | Maliyet |
| NPC tabanlı | AI altyapısı hazır | Oyuncu gibi görünmez, party yok, PvP formülü yok | Gereksinimi karşılamıyor |

## Sonuçlar
Sunucu kodunda küçük ama birden çok noktada değişiklik (S1–S12). Thread güvenliği aksiyonların IOCP'ye taşınmasıyla yönetilir. Gerçek oyuncu kapasitesi ayrılan slot kadar azalır.

## Doğrulama
F2 kabulü: AC-ARCH-01, AC-ARCH-04, AC-ARCH-05; Q-14.
