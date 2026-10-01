# ADR-0012: Botların sıralama, ödül ve duyurulara dahil olması

Durum: KABUL · Tarih: 2026-10-01 · Karar veren: proje sahibi (oturumda tek tek soru-cevap ile)
İlgili: K-9, REQ-NEW-14 (KALDIRILDI)

## Bağlam
PK öldürmelerinde NP ve altın aktarımı, kişisel sıralama, periyodik sıralama ödülleri ve zone geneline ölüm duyurusu var `[D]`.

## Karar
Botlar **tamamen normal oyuncu gibi** dahildir; bu akışlarda bota özel istisna yapılmaz. S7 yalnızca giriş için sabit IP içerir.

## Değerlendirilen alternatifler
| Alternatif | Artılar | Eksiler | Neden seçilmedi |
|---|---|---|---|
| Sıralama/ödülden hariç + duyuruları kısmak | Gürültüsüz test | Ek kod, gerçekçilik azalır | Seçilmedi |
| Sıralama/ödülden hariç, duyurular açık | Ekonomi korunur | Ek kod | Seçilmedi |

## Sonuçlar
Sıralamalar ve ödüller botlarla dolabilir; zone duyuruları çok botlu testlerde yoğunlaşır ve performans maliyeti MET-PERF-05 ile ölçülür.

## Doğrulama
T-PERF-01..03 (duyuru maliyeti).
