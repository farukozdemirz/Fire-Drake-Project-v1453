# ADR-0021: Navigasyon sunucu entegrasyonu: dört tasarım kararı (F5-61, F5-62, F5-65, F5-66)

Durum: KABUL
Tarih: 2026-10-03 · Karar verenler: proje sahibi (soru-cevap oturumu)
İlgili: F5-55 şemsiye planı ve dilimleri, `docs/12` §13, ADR-0006

## Bağlam

F5-61..F5-66 taslak planları (`plans/`) dört noktayı "HAZIR öncesi, proje sahibi kararı" olarak açık bıraktı. Varsayılanlar plan taslaklarında Claude önerisi olarak yazılıydı.

## Karar

1. **`/bot goto` geçersiz hedef (F5-62):** hedef `Walk` olmayan hücredeyse komut **reddedilir** (`invalid_goal`), en yakın yürünebilir hücreye alınmaz. Yanlış koordinat sessizce başka yere gitmesin; testler yanıltılmasın.
2. **Başlangıç hücresi yürünemezse (F5-61, D5):** kiriş denetimi, başlangıç hücresinden **çıkış noktasına kadar muaf** tutulur; çıkıştan sonraki her hücre `Walk` olmak zorundadır. Bot kilitlenmez, koruma yalnızca o tek hücre için gevşer.
3. **Konum sıçraması (F5-65):** botun sunucudaki konumu, gönderdiği son paketin konumundan **> 1,0 m** farklıysa (itme, sunucu geri çekmesi, açıklanamayan ışınlanma) rota ve takip **sonlandırılır**, yeniden planlama istenir; eski yolla yürümeye devam edilmez.
4. **T-NAV-06 hareketli hedef takibi kabul eşiği (F5-66):** önce ölçülür; ilk koşuda yalnızca dağılım raporlanır (bot-hedef mesafesi ortanca/p95, yeniden plan sayısı, `NAV_STUCK`), **eşik ölçüm görüldükten sonra proje sahibiyle belirlenir** (şimdilik sayı konmaz).

## Değerlendirilen alternatifler

| Konu | Reddedilen | Neden |
|---|---|---|
| 1 | En yakın yürünebilir hücreye al | Yanlış koordinatı gizler; ölçümleri yanıltabilir |
| 2 | Muafiyet yok, hata ver | Bir doğuş noktası sorunu botu tamamen durdurur |
| 3 | Rota korunur, takılma tespiti halleder | Bayat yolla yanlış yöne/duvara yürüme riski |
| 4 | Şimdi sabit eşik | Doğrulanmamış tahmin; koşu haksız yere başarısız olabilir |

## Sonuçlar

Olumlu: davranışlar öngörülebilir ve test edilebilir. Olumsuz: reddetme ve rota sonlandırma daha çok açık hata/yeniden plan üretir; T-NAV-06 ilk koşuda "geçti/kaldı" hükmü vermez. Geri alma: dört karar da ilgili planın parametresi/kuralıdır.

## Doğrulama

F5-61/62/65 birim testleri ve F5-66 çalışma zamanı koşusu (Claude); T-NAV-06 dağılım raporu ve sonrasında eşik kararı.
