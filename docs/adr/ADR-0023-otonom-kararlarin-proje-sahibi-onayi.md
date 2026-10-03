# ADR-0023: Otonom döngüde verilen kararların proje sahibi onayı (ADR-0005, 0006, 0007, 0014..0018, 0002 Ek, 0031 Ek, 0030..0033-DEG)

Durum: KABUL
Tarih: 2026-10-03 · Karar verenler: proje sahibi (soru-cevap oturumu, `docs/reports/karar-secenekleri-2026-10-03.md` seçenekleriyle)
İlgili: ADR-0005, ADR-0006, ADR-0007, ADR-0014, ADR-0015, ADR-0016, ADR-0017, ADR-0018, ADR-0002 (Ek F8-02), ADR-0031-DEG (Ek F8-01), ADR-0030..0033-DEG

## Bağlam

Gece döngüsünde Claude bu ADR'leri (veya Ek'lerini) "gözden geçirilmeli" etiketiyle kendisi kabul etmişti (karar yetkisi `plans/OTONOM_DONGU.md` §2). Proje sahibi, her birinin sade anlatımını ve alternatifini görerek onayladı.

## Karar

| ADR | Karar |
|---|---|
| ADR-0005 (bot tick'i IOCP thread'inde) | **ONAYLANDI.** 16 bot yürürken `Tick` ölçümü F5-66'da yapılır; bütçe aşılırsa botlar gruplara bölünür |
| ADR-0006 (ızgara A*, LoS `advisory`, Ek F5-03..F5-11) | **ONAYLANDI**, T-NAV-03 kapısı arena ölçeği (≤ 64 hücre); uzun mesafe için hiyerarşik arama ayrı karar; takılma > 2/bot-saat ise navmesh yeniden düşünülür (R-09) |
| ADR-0017 (aksiyon = gerçek paket, adalet koruması saf mantık) ve Ek'leri | **ONAYLANDI** |
| ADR-0018 (F4 kapsam genişletme) ve Ek'leri | **ONAYLANDI ve kapsam DONDURULDU:** bundan sonra yeni F4 dilimi eklenmez; gerekirse öneri `docs/STATUS.md` "Blokajlar"a yazılır ve proje sahibine sorulur (otonom döngü yeni F4 dilimi yazmaz). Rogue/okçu ve Type7 kapsam dışı (ADR-0022) |
| ADR-0014 (hesap doğrulaması ve `SET_LOGIN_INFO` atlanır) | **ONAYLANDI** (bot hesapları web panelinde görünmez: bilinen yan etki) |
| ADR-0015 (komut kanalı: `/bot`, `BotCommands.txt`, `+bot`) | **ONAYLANDI;** üretim sunucusunda `[BOT] ENABLED=0` (varsayılan) kalır, proje sahibi teyit eder |
| ADR-0007, ADR-0016, ADR-0002 Ek F8-02, ADR-0031-DEG Ek F8-01 | **TOPLU ONAYLANDI** |
| ADR-0030, 0031, 0032, 0033-DEG | **TEYİT EDİLDİ** (kararları zaten proje sahibinindir) |

## Sonuçlar

Başlıklardaki "gözden geçirilmeli" etiketi bu ADR'lerde "proje sahibi onayladı: 2026-10-03, ADR-0023" olarak değiştirildi (içerik değişmedi). Bundan sonra döngünün yeni verdiği kararlar yine "gözden geçirilmeli" etiketiyle gelir. F2/F3/F4 **faz kabulü** hâlâ ayrı ve proje sahibindedir (ADR onayı faz kabulü değildir).

## Doğrulama

`git grep 'gözden geçirilmeli' docs/adr` yalnızca yeni (2026-10-03 sonrası) Ek'leri gösterir.
