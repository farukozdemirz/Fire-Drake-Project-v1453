# Sürüm Yükseltme Analizi — "Moradon: The Resurrection" (Yeni Moradon) dönemi

> Durum: Taslak v1.2 · Tarih: 2026-10-08 · Hazırlayan: Claude (planlayıcı/denetçi) · **Karar bekliyor, uygulama başlamadı.**
> Soru (proje sahibi): mevcut geliştirmeleri koruyarak projeyi uzun klan pelerinlerinin ve yeni klan sisteminin bulunduğu Yeni Moradon dönemine yükseltmek; hedefin 1505–1507 olduğu söyleniyor, doğrulanacak.
> Etiketler: `[D]` depo kodu · `[V]` yerel veri (DB, dosya, istemci) · `[S]` dış kaynak · `[B]` başka sürüm/doğrulanmamış · `[Ö]` öneri · `[A]` açık / çalışma zamanı testi gerekli · `[I]` çıkarım.
> Bu rapor **doğrulanan bulgular** ile **tahminleri** ayırır. Satır numaraları `main` @ `5aa3e76c` ve üst kaynak `0f52027`'de kontrol edildi. Hiçbir üretim kodu, DB satırı veya istemci dosyası değiştirilmedi; dış kaynak (1534 sunucu kodu) yalnız geçici çalışma dizinine indirildi, depoya alınmadı.

---

## 0. Yönetici özeti

1. **Hedef sürüm iddiası doğru.** "Moradon: The Resurrection" USKO'da 13–14 Eylül 2007'de yayına girdi ve yaması **1505** numaralıdır (`Moradon_AutoPatch_1505`), ardından 1506 ve 1507 geldi `[S]` (§1). Topluluk aynı dönemi daha çok **1534** istemcisiyle dağıtır; 1534 için aynı soydan açık sunucu kaynağı vardır `[S]`.
2. **Elimizdeki istemci 1453 protokolüdür ve verisi 2006 ortasıdır.** Sürüm sabiti exe içinde (`cmp ecx,1453`), yalnız eski Moradon haritası var, yeni Moradon/uzun pelerin varlıkları yok `[V]` (§2.3).
3. **Sunucu kodu zaten "yeni klan sistemi"nin veri modelini taşıyor** (kademe bayrakları Training→Royal1, pelerin tablosu, klan NP fonu, bağış, duyuru, ittifak) ve kullanıcı paketlerinde kademe/pelerin kimliğini gönderiyor `[D]` (§2.1). Eksik olan kurallar (otomatik kademe yükseltme, pelerin RGB, birkaç gizli hata) ve **istemci**dir.
4. **1453 → 1534 sunucu kaynak farkı küçüktür:** 232 ortak dosyadan 159'u birebir aynı, 73'ü farklı; `packets.h` 5 satır, kripto anahtarı 1 satır; farkın büyük kısmı özel sunucu etkinlikleridir `[D]` (§3). Dolayısıyla protokol tarafında **yeniden yazma yok**; sürüm/kripto/VERSION uyarlaması ve dönem verisi gerekir.
5. **Asıl eksikler dosyadır, kod değil:** hedef istemci (1534 veya 1505–1507), yeni Moradon sunucu haritası (`.smd`), yeni Moradon DB içeriği (NPC konumları, başlangıç noktası, görevler), hedef istemciyle eşleşen tablo/DB kimlikleri. **Bunların hiçbiri elimizde yok** `[V]` (§5).
6. **Öneri:** mevcut sunucu + bot katmanı korunur (seçenek A, §6); 1534 kaynağı yalnız referanstır. Beş aşamalı plan (§7) ve doğrulama yöntemi (§8). İlk karar: hedef istemci (§10, K-11 adayı).

---

## 1. Sürüm kimliği doğrulaması

| # | Bulgu | Kaynak | Etiket |
|---|---|---|---|
| 1.1 | "Moradon: The Resurrection" genişlemesi 13 Eylül 2007'de duyuruldu, 14 Eylül 2007'de yama yayında | GamingNexus 2007-09-13; mmorpg.com 2007-09-14 (§11 W-10, W-11) | `[S]` |
| 1.2 | Yama numarası **1505** (`Moradon_AutoPatch_1505`), sonrasında 1506 ve 1507; başlık tarihi 13 Eylül 2007 | DonanımHaber konu başlığı "Moradon_AutoPatch_1505, Patch 1506, Patch 1507…" (W-12) | `[S]` |
| 1.3 | 1453 ≈ Ekim 2006; ~1470 ≈ Ocak 2007 | `docs/03` §1.1; `docs/19` W-05 | `[S]` |
| 1.4 | Zaman çizgisi tutarlı: 1453 (Ekim 2006) → ~1470 (Ocak 2007) → 1505 (Eylül 2007) | 1.2 + 1.3 | `[I]` |
| 1.5 | 1534 = aynı dönem, Forgotten Frontiers (Ekim 2008, seviye 83) **öncesi**; açık 1534 sunucu kaynağında `MAX_LEVEL 80`, `__VERSION 1534` | KODevelopers-1534 `shared/version.h`, `GameServer/Define.h:21` (W-13); Kalais arşivi (`docs/19` W-01) | `[S]` `[D]` |
| 1.6 | 1534'ün tarihi: 1505'ten ~29 yama sonra, 2008'in ilk yarısı | 1.3–1.5 aritmetiği | `[I]` |
| 1.7 | Topluluk bu istemciyi "USKO 1534 Moradon: The Resurrection" adıyla dağıtıyor | ko-yardim, kodevelopers.net arama özetleri (W-14) | `[S]`/`[B]` |
| 1.8 | **1505–1507 tam istemcisinin bugün dolaşımda olduğu doğrulanamadı**; yalnız 2007 yama dosyalarına atıf var | W-12 | `[A]` |

**Dönemin içeriği (resmî duyuru + Kalais rehberi, W-10/W-11/W-15) `[S]`:**

- Yeni Moradon şehri ve çevresi: çok daha büyük av alanı, 45'e kadar oyuncuyu taşıyan canavarlar; Moradon'dan çıkış için seviye 35 şartı.
- **Klan sistemi tamamen değişti:** 3 sınıf × 5 kademe = 15 kademe (Squire / Knight / Royal Knight). 3. sınıf kısa düz pelerin; 2. sınıf çok renkli kısa pelerin; **1. sınıf (Royal) aynı pelerinlerin uzun, yere değen hali.** Pelerinler klan NP fonundan alınır (36k–2,9M NP). Üyeler NP bağışlar; PK'de klana onur puanı yazılır. Eşikler: 252.000 klan puanı Knight, 1.080.000 Royal Knight (resmî); 2. sınıf için Isiloon, 1. sınıf için Fire Drake görevi (Kalais). Klan duyurusu ("Clan Notice"), ilk 5 klan bayrağı, klan salonu.
- Diğer: Border Defense War, Juraid Mountain, Moradon arena, antrenman kuklaları (30/60/80), falcı, pet sistemi, yeni unique eşyalar, mage/rogue skill düzeltmeleri.

**Sonuç:** "1505–1507 civarı" iddiası doğrudur. Pratikte iki hedef adayı vardır: **1505–1507** (dönemin ilk hali) ve **1534** (aynı dönemin son hali; tam istemci ve aynı soydan açık sunucu kaynağı mevcut). Ayrıntı §1.1.

### 1.1 Ek: 1534 araştırması (2026-10-08, proje sahibinin isteğiyle)

| # | Bulgu | Kaynak | Etiket |
|---|---|---|---|
| 1.9 | **Resmî README "Knight Online Expansion Version 1505 — Moradon: The Resurrection"**: Moradon şehri ve av alanı büyüdü; çıkış için seviye 35; Knight sistemi 3 seviye — Squire (144.000 klan puanı + Caitharos görevi), Knight (252.000 + Isiloon), Royal Knight (1.080.000 + Felankor), her seviyede 5 rütbe; "rütbe yükseldikçe pelerin uzar"; klan duyurusu (`/clannotice`); klan puanı katkısı (manuel/otomatik, ayrılan üyeye %30 iade); pet; Juraid (70–80); BDW (35–45 ve 60–69); Moradon arena; kuklalar (30/60/80); falcı; ilk 5 klan bayrağı; `/Return` | Wayback 2008-01-15, `knightonlineworld.com/moradon_patchtxt.php` (W-17) | `[S]` |
| 1.10 | **Resmî yama listesi (zaman çizgisi):** `patch1506.zip`…`patch1532.zip` (anlık görüntüler 2008-07-17, 2008-08-04, 2008-09-01); `patch1533.zip` eklenmiş (2008-10-04, 2008-11-07, 2008-12-17); 2009-02-07'den itibaren liste `patch1706`…`patch1717` | Wayback `knightonlineworld.gamersfirst.com/patches.php` (W-18) | `[S]` |
| 1.11 | 1534 resmî listede hiç görünmez. Topluluk kayıtları: frmtr "Patch 1534 Çıktı" ve "USKO 1534 KoXP" (Ekim 2008 civarı; sayfalar bot erişimine kapalı, yalnız arama özeti). Çıkarım: **1534 ≈ Ekim 2008, Forgotten Frontiers (21 Ekim 2008; 17xx numaralama, seviye 83) öncesi son yama** | W-19; 1.10 | `[S]`/`[B]` → `[I]` |
| 1.12 | Topluluk özetine göre 1534 yaması premium süresini saatliğe çevirdi ve XTrap korumasını exe'den çıkarıp ayrı `kol.exe` modülüne taşıdı | W-19 | `[B]` |
| 1.13 | **Internet Archive'da resmî kurulum adlı tam istemciler:** `KnightOnLineSetup_1506.exe` (529.402.260 B, md5 `416d9b2293fda1b693931acdbeded171`); ayrıca 1098, 1264, 1268, 1298, 1453, 1705, 1861 (yükleyen: bireysel kullanıcı, 2023-02-05). Resmî Moradon indirme sayfası tam istemciyi aynı adla (`KnightOnLineSetup_1506.exe`, "Moradon_AutoPatch_1505") dağıtıyordu | archive.org `knightonline2003` (W-20); Wayback 2008-01-01 `moradon_download.php` (W-21) | `[S]` (içerik doğrulanmadı `[A]`) |
| 1.14 | 1534'ün topluluktaki yeri: KODevelopers-1534 tek açık sunucu kaynağı (DB/harita/istemci yok, W-13/W-16); "1534 server files + database + client" paketleri forumlarda gizli, şifreli veya ücretli (ko-cuce 2023, elfdaily 2018, knightlobby, GNY Soft satış ilanı, r10 kiralık 400 TL/ay, MykoSoft v1534 "tonla bug" uyarısı); kodevelopers.net "Orjınal 1534 Client Sorunsuz" (Nisan 2026, giriş gerekli); MYKO/SEA v1534 projesi (2011, seviye 80, "yeni anti-cheat") | W-14, W-22 | `[B]` |
| 1.15 | 1534 çalıştıran özel sunucular var (OhaGaming "Reign of the Fire Drake v1534", seviye 80, 2022–2026; MemoryKO v1534); dönemi "2008 Fire Drake" diye adlandırıyorlar | ko-pserver, mmtop200 (W-22) | `[B]` |
| 1.16 | 1534 sunucu kaynağı resmî 1534 istemcisiyle kripto anahtarı `0x1257091582190465` kullanıyor ve yorumda "1453 & 1534" diyor; bizim kodda aynı değer "1453" yorumuyla kapalı, etkin anahtar `0x7412580096385200` yerel 2014 exe'siyle çalışıyor. Çıkarım: resmî 1453–1534 istemcileri aynı anahtarı paylaşır; yerel exe değiştirilmiş anahtar taşır | 1534 `shared/JvCryption.cpp`; `shared/JvCryption.cpp:6-13` | `[I]` |

**1534 nedir (özet):** 1505 genişlemesinin (Eylül 2007) tüm içeriği + Ekim 2007–Ekim 2008 yamaları; seviye sınırı 80; Forgotten Frontiers'tan (17xx) hemen önceki son "Fire Drake / Yeni Moradon" istemcisi. "Uzun pelerin + yeni klan sistemi" 1505'ten itibaren vardır; 1534 bunun en olgun hali ve topluluğun standart dağıtımıdır. Resmî 1534 tam kurulum paketi bulunamadı; 1506 tam kurulum paketi arşivde var.

### 1.2 1534 dosya kaynakları (2026-10-08 taraması; proje sahibi inceleyip indirecek)

Hiçbir dosya indirilmedi. "Görülemedi" = içerik üyelik/giriş duvarı arkasında veya site bot erişimini engelliyor (403); proje sahibi tarayıcıdan bakacak. Tüm paketler tescilli MGame varlıkları ve topluluk yeniden paketlemeleridir: ayrı boş dizine indir, hash al, kötücül yazılım taraması yap, exe'yi ana makinede çalıştırmadan önce incele (`docs/18` R-11).

**İki sunucu soyu vardır; bu ayrım indirmeden önce bilinmeli:**
- **snoxd/GameServer soyu** (bizim kod; `GameServer`+`AIServer`+`LogInServer`, Lua görevler, `.smd` haritalar): DB şeması ve görev dosyaları doğrudan uyumlu adaydır.
- **Ebenezer/Aujard soyu** (MGame mimarisi; `Ebenezer`, `Aujard`, `AIServer`, `LoginServer`, `.evt` görevler, iki DB `KN_online`+`KO_MAIN`): sunucu kodu bize uymaz; **istemci, harita ve referans tabloları** (ITEM/MAGIC/K_NPC/K_NPCPOS/KNIGHTS_CAPE) için kaynak olarak değerlidir.

| # | Kaynak | İçerik (iddia) | Erişim | Not |
|---|---|---|---|---|
| S-01 | Internet Archive `knightonline2003` — https://archive.org/details/knightonline2003 · doğrudan: https://archive.org/download/knightonline2003/KnightOnLineSetup_1506.exe | Resmî kurulum adıyla tam istemci 1506 (529.402.260 B, md5 `416d9b2293fda1b693931acdbeded171`); ayrıca 1453, 1705, 1861 | **Açık** (HEAD 200 doğrulandı) | Yeni Moradon'un ilk resmî hali; HackShield/XTrap ve resmî IP içerir → exe yaması gerekir; 1506→1533 resmî yamaları arşivde yok (`download.knightonlineworld.com` CDN test sayfası) `[A]` |
| S-02 | GitHub `ForcePower/KODevelopers-1534` — https://github.com/ForcePower/KODevelopers-1534 | 1534 sunucu kaynağı (snoxd soyu, bizimle 159/232 dosya aynı) | **Açık** | DB/harita/görev/istemci yok (W-16) |
| S-03 | GitHub `yfz912/KoProjectCSharp` — https://github.com/yfz912/KoProjectCSharp | "Açık kaynak ko 1534": C# EF projeleri; 1534 DB şemasının 103 tablo adı (`PET_*`, `USER_GENIE`, `MONSTER_STONE`, `QUEST_*` …) | **Açık** | Dosya yok; şema karşılaştırması için referans |
| S-04 | ko-yardim.com — "ALPHAGAME v1534 - Source - DB - LUA-MAP - Client - Pet Sistemi" (30 Nis 2023, 603 yanıt) — https://ko-yardim.com/konu/alphagame-v1534-source-db-lua-map-client-pet-sistemi.1365/ | Source + DB + Lua + harita + istemci (snoxd soyu; Lua ve `.smd`) | **Görülemedi** (giriş) | Bizim yapıya en yakın tam paket adayı; şifre ve link gizli |
| S-05 | pvpers.gg — aynı AlphaGame paketi (29 Oca 2025, 8 sayfa) — https://pvpers.gg/konu/alphagame-v1534-sorunsuz-pet-sistemi-tum-source-db-ve-client-dosyalari.404/ | S-04 ile aynı | **Görülemedi** (giriş; RAR şifresi spoiler'da) | Yedek kaynak |
| S-06 | ko-yardim.com — "V1534 - source release x64, anti-d3d8to9, dll source fix PUS address" (14 Şub 2026, 127 yanıt) — https://ko-yardim.com/konu/v1534-source-release-x64-anti-d3d8to9-dll-source-fix-pus-address.10500/ | Onarılmış/derlenmiş sunucu kaynağı; "Client and DB use alpha 1534"; mediafire | **Görülemedi** (giriş) | AlphaGame istemci+DB ile eşleşen güncel kaynak; d3d8.dll sargısı istemci tarafı |
| S-07 | kodevelopers.net — "Knight Online Orjınal 1534 Client Sorunsuz" (6 Nis 2026, 51 yanıt) — https://www.kodevelopers.net/threads/knight-online-orjinal-1534-client-sorunsuz.35/ | "Orjinal temiz" USKO 1534 istemcisi | **Görülemedi** (giriş) | Bir yanıt "data" dosyalarının ayrı gerektiğini söylüyor; HackShield durumu belirsiz |
| S-08 | kodevelopers.net — "KO Server Project v1534 Sorunsuz" (5 Nis 2026) — https://www.kodevelopers.net/threads/ko-server-project-v1534-sorunsuz.26/ | Sunucu paketi; "JR, BDW, CSW, upgrade, savaşlar, klan sistemi çalışıyor" | **Görülemedi** (giriş) | Soy belirsiz |
| S-09 | kodevelopers.net — "1534 Lion Files Pus+Daily Pet" (9 Nis 2026) — https://www.kodevelopers.net/threads/yildiz1534-lion-files-pus-daily-pet-yildizguncel-yeni.91/ | Özel sunucu paketi (XingCode anti-cheat, PUS) | **Görülemedi** (giriş) | "Orijinal" değil; düşük öncelik |
| S-10 | r10dev.net — "Purge ACS System v1.534 — 1534 Orijinal Database, Server Files ve ACS" (14 Ağu 2026) — https://r10dev.net/konular/purge-acs-system-v1-534-knight-online-1534-orijinal-database-server-files-ve-acs-sistem.11574/ | "Orijinal 1534 database yapısı", slot/NPC/drop/quest | **Görülemedi** (giriş) | DB adayı |
| S-11 | ko-cuce.net — "KoCuce Orjinal 1534 Server Files + Database + Client + ODBC 2024" (27 Kas 2023) — https://www.ko-cuce.net/konular/kocuce-orjinal-1534-server-files-database-client-odbc-2024.80380/ | Sunucu + DB + istemci + ODBC (KO-FOX kaynaklı) | **Görülemedi** (gizli içerik) | Soy belirsiz |
| S-12 | ko-cuce.net — "Usko Client + Data FTP LİNK (2038-1977-1960-1886-1534-1453-1299)" (11 Kas 2015, 1K yanıt, 143K görüntüleme) — https://www.ko-cuce.net/konular/usko-client-data-ftp-link-2038-1977-1960-1886-1534-1453-1299-2024.33601/ | USKO istemci arşivi, 1534 dahil | **Görülemedi** (giriş) | "Orijinal" olma olasılığı yüksek; FTP linkleri 2024'te yenilenmiş |
| S-13 | ko-cuce.net — "1534 Client" (9 Nis 2012, HiFi) — https://www.ko-cuce.net/konular/1534-client.310/ | 1534 istemci | **Görülemedi** (giriş) | Eski; link durumu belirsiz |
| S-14 | ko-yardim.com — "Sexyko Orjinal Client v1534" (2 Oca 2021, 221 yanıt) — https://ko-yardim.com/konu/sexyko-orjinal-client-v1534-serinin-en-iyi-clienti.225/ | SexyKO sunucusunun 1534 istemcisi | **Görülemedi** (giriş) | Sunucuya özel düzenlenmiş olabilir; resmî "orijinal" değil |
| S-15 | kocuce.com.tr — "1534 Server Files ve Sourceler + Client + Database 2019~2020" (25 Eyl 2019, 137 yanıt) — https://www.kocuce.com.tr/konu/1534-server-files-ve-sourceler-client-database-2019-2020-kocuce.243/ | **Ebenezer/Aujard soyu:** kaynak (Ebenezer, Aujard), sunucu (LoginServer, AIServer, DLL), istemci (`Data`, `KnightOnLine.exe`), DB `k-27.08.2016.bak`; RAR şifresi `www.kocuce.com.tr` | **Kısmen görüldü** (liste açık, link giriş gerektirir) | İstemci + harita + referans tablolar için en belgeli paket |
| S-16 | elfdaily.com — "1534 Server Files ve Sourceler" (3 Mar 2018, 18 sayfa) — https://www.elfdaily.com/konu/1534-server-files-ve-sourceler.3213/ | S-15 ile aynı paket (İstanbulGaming.Online); şifre `elfdaily.com` | **Görülemedi** (giriş) | Yedek |
| S-17 | elfdaily.com — "[1534] server files ! Avci %97 fix !" (28 Ara 2015, 19 sayfa) — https://www.elfdaily.com/konu/1534-server-files-avci-97-fix.156/ · kocuce.com.tr kopyası (25 Eyl 2019) — https://www.kocuce.com.tr/konu/1534-server-files-avci-97-fix-kocuce.241/ | 1534 sunucu dosyaları (Ebenezer soyu olasılığı) | **Görülemedi** (giriş) | "Linkler çalışıyor mu" yanıtları var |
| S-18 | ko-cuce.net — "v1534 Server Files ve Database" (10 Şub 2014, LipitoS4) — https://www.ko-cuce.net/konular/v1534-server-files-ve-database.11475/ | SQL 2008 R2 DB (düzenlenmiş), "haritalar eksiksiz"; şifre `ko-fox.com` | **Görülemedi** (giriş) | Bilinen hata: party'de NP gelmiyor |
| S-19 | ko-cuce.net — "v1534 Server Files - Database" (4 Şub 2014, LipitoS4) — https://www.ko-cuce.net/konular/v1534-server-files-database.11225/ | SQL 2008 R2 DB; klan kurma, ittifak çalışıyor; şifre `ko-fox.com` | **Görülemedi** (giriş) | |
| S-20 | ko-cuce.net — "v1534 S.Files & Database By Ogzhn ODBS" (9 May 2014) — https://www.ko-cuce.net/konular/v1534-s-files-database-by-ogzhn-odbs.14842/ | Derlenmiş sunucu + DB (SQL 2008 R2); "klan sistemi ve pelerin değiştirme sorunsuz" | **Görülemedi** (giriş) | Bir yanıt "server files eksik" diyor |
| S-21 | ko-master.com — "1534 database" — https://ko-master.com/konular/1534-database.423/ | 1534 DB; şifre `ko-master.com` (arama özeti) | **Görülemedi** (403) | |
| S-22 | knightlobby.com — "v1534 Server Files ve Database" — https://knightlobby.com/konu/v1534-server-files-ve-database.32510/ | Sunucu + DB | **Görülemedi** (403) | |
| S-23 | vsro.org — "Knight Online 1534 Server Files, Full Client, Başlangıç Karakter" (22 Tem 2026) — https://www.vsro.org/konular/knight-online-1534-server-files-full-baslangic-karakter-banka-sistemi.18417/ | Sunucu + tam istemci + SQL betikleri | **Görülemedi** (gizli metin) | Yanıtlar "link kırık" diyor |
| S-24 | ko4life.net Market — "Original version 1534 Files!" (Aether, 1 Haz 2018) — https://ko4life.net/forum/45-market/ | Satış ilanı (1453/19xx/v2 karışımı; 1534'e ek özellikler) | **Görülemedi** (403) | "Orijinal" iddiası şüpheli |
| S-25 | elfdaily.com — "MykoSoft Istrap Full Source v1098/v1534/v2368" (20 Eyl 2023) — https://www.elfdaily.com/konu/mykosoft-istrap-full-source-files-v-1098-v-1534-v-2368.4887/ | Kaynak + anti-cheat + istemci + DB | **Görülemedi** (giriş) | Yanıt: "tonla bug, eksik ve açık" |
| S-26 | ragezone — KO bölümü https://forum.ragezone.com/community/knight-online.107/ · Releases https://forum.ragezone.com/community/knight-releases.465/ | Olası 15xx dosya/exe paylaşımları | **Görülemedi** (403) | Tarayıcıdan arama: "1534" |
| S-27 | Blog (8 Şub 2012) — https://knightonlineprivateserverlar.blogspot.com/2012/02/1534-puf-noktalar-guncel.html | Ebenezer soyu 1534 kurulum notları (`KN_online`/`KO_MAIN`, bilinen hatalar) | **Açık** | Kurulum referansı |

**İndirme sonrası ilk kontrol listesi (U0):** `KnightOnline.exe` içinde sürüm sabiti (`cmp ecx,0x5FE`=1534 veya `0x5E2`=1506), `Zones/moradon.*` tarihi (≥ 2007) ve boyutu, `Data/Cloak.tbl` ve `Zones.tbl` tarihi, HackShield/XTrap/`kol.exe` varlığı, `Launcher.exe` ve `Server.ini`; sunucu paketinde `Map/*.smd` adları (yeni Moradon SMD'si), `Quests/*.lua` sayısı, DB `.bak` adı/boyutu/SQL sürümü, `KNIGHTS_CAPE` satır sayısı ve `K_NPCPOS` zone 21 satırları.

---

## 2. Mevcut durum envanteri

### 2.1 Sunucu kodu (depo, `main` @ `5aa3e76c`)

**Sürüm ve el sıkışma `[D]`**

- `shared/version.h:3` → `#define __VERSION 1453`. Sürüme bağlı kod yalnız şu noktalarda: `shared/JvCryption.cpp:6-13` (özel anahtar seçimi; 1453 dalı `0x7412580096385200`, yorumda alternatif `0x1257091582190465`), `LogInServer/LoginServer.cpp:93,102,106` (sunucu listesi düzeni: ≥1888 LAN IP, ≥1453 ek alanlar, <1600 bilinmeyen bayt = 1), `LogInServer/LoginSession.cpp:166` (**≥1500: sunucu listesi isteğinde `u16 echo` okunup geri yazılır; 1453 derlemesinde bu dal kapalı**), `GameServer/DatabaseThread.cpp:190` (≥1920), `GameServer/User.cpp:5070,5074` (≥1900).
- Sunucu istemci sürümünü **karşılaştırmaz**: `LoginSession::HandleVersion` (`LoginSession.cpp:32-37`) yalnız DB'deki en yüksek `VERSION.sVersion` değerini döner (`DBProcess.cpp:14-46`); `CUser::VersionCheck` (`GameServer/LoginHandler.cpp:3-19`) istemci verisini okumaz, `uint16(__VERSION)` + kripto anahtarını gönderir ve şifrelemeyi açar. Uyuşmazlık kararı istemci tarafındadır.
- Launcher yaması: `HandlePatches` (`LoginSession.cpp:39-60`) istemcinin bildirdiği sürümden büyük her `VERSION` satırını FTP adresiyle birlikte döner. Yerel `VERSION` tablosu 1454..1473 (20 satır), istemci `Server.ini` `Files=1473`/`Last=1473` `[V]`.

**Klan sistemi (zaten var olanlar) `[D]`**

- Kademe modeli: `ClanTypeFlag` (`GameServer/Knights.h:26-41`) = Training(1), Promoted(2), Accredited5..1 (3–7), Royal5..1 (8–12). Bu, resmî "Squire/Knight/Royal" üçlüsünün sunucu karşılığıdır; eğitim+terfi iki ön kademe ile 12 bayrak.
- `CKnights` alanları (`Knights.h:46-71`): `m_byGrade`, `m_byRanking`, `m_nClanPointFund`, `m_sCape`, `m_bCapeR/G/B`, `m_sAlliance`, `m_sClanPointMethod`, `m_strClanNotice`, `m_arKnightsUser[36]`.
- Puandan derece: `CGameServerDlg::GetKnightsGrade` (`GameServer/GameServerDlg.cpp:2934-2948`) eşikleri 720k/340k/50k/25k → derece 1–5.
- Kademe yükseltme yalnız Lua `PromoteClan` (`GameServer/lua_bindings.cpp:129,322` → `QuestHandler.cpp:424-430` → `KnightsManager::UpdateKnightsGrade` `KnightsManager.cpp:738-761`). **Puan eşiğinden otomatik kademe yükseltme yok.**
- Pelerin: `KNIGHTS_CAPE` tablosu `LoadKnightsCapeTable` (`LoadServerData.cpp:261-264`), yapı `GameDefine.h:394-401`; satın alma `CUser::HandleCapeChange` (`NPCHandler.cpp:806-939`), kademe/derece kontrolü `:852-858`; RGB boyama yorum satırında (`:813-816, 876-887, 911-916`).
- NP bağışı ve fon: `DonateNPReq/DonateNP/DonationList` (`KnightsManager.cpp:1349-1420`), PK onur puanı `AddUserDonatedNP(..., bIsKillReward)` (`:789`). Klan duyurusu, ittifak (`:983-1300`), ilk 10 klan (`:1303`).
- Kullanıcı paketleri kademe ve pelerini **zaten gönderiyor**: `GetUserInfo` (`CharacterMovementHandler.cpp:99-120`: ittifak, klan adı, `m_byGrade`, `m_byRanking`, sembol sürümü, pelerin kimliği) ve `SendMyInfo` (`User.cpp:988-1016`).

**Gizli hatalar / eksikler `[D]`**

- `WIZ_CAPE` DB isteği yalnız `(clanID, capeID)` yazar (`NPCHandler.cpp:935-937`), okuyan `ReqChangeCape` ise `r,g,b` da bekler (`DatabaseThread.cpp:428-435`) → pelerin kaydı bozuk/boş değer okur.
- Pelerin RGB istemciye hiç gönderilmiyor; `m_bCapeR/G/B` yalnız yükleniyor (`shared/database/KnightsSet.h:33-35`).
- `CKnightsManager::PacketProcess` (`KnightsManager.cpp:8-108`) içinde `KNIGHTS_STASH`, `KNIGHTS_WAR_*`, `KNIGHTS_CAPE_NPC`, istemci kaynaklı `KNIGHTS_UPDATE_GRADE` yok.
- Opcode tablosu `shared/packets.h` 0x01–0x91; 17 adsız yer tutucu (`WIZ_PACKET1..17`, biri "new clan" notlu, `:102`) hiçbir yerde işlenmiyor.
- `MIN_LEVEL_NATION_BASE 1`, `MIN_LEVEL_ESLANT 40` (`GameServer/Define.h`); dönemde Moradon çıkışı 35 `[S]`.

**Harita yükleme `[D]`**

- Yalnız `.smd` okunur: `SMDFile::Load` (`shared/SMDFile.cpp:16-59`, düzen `:75-180`: yükseklik, N3ShapeMgr çarpışma, olay ızgarası, regene, warp). `.gtd/.opd` sunucuda kullanılmaz. `ZONE_INFO.strZoneName` dosya adını verir; zone 21 = `moradon_20060124.smd` (512 m; dosya başlığı `0x81`) `[V]`. AIServer aynı SMD + `Map/21.aievt` `[V]`.
- 1534 kaynağında `shared/SMDFile.*`, `N3BASE/*` **birebir aynı** → yeni Moradon SMD'si aynı biçimde olmalı; biçim değişikliği yok `[D]`.

### 2.2 Veri tabanı (`FDP_kn_online`, yalnız şema ve referans tabloları; kişisel veri tabloları okunmadı) `[V]`

- `KNIGHTS` kolonları: `… Points, Mark, sMarkVersion, sMarkLen, sCape, bCapeR, bCapeG, bCapeB, sAllianceKnights, ClanPointFund, strClanNotice, bySiegeFlag, nLose, nVictory, ClanPointMethod, …` (yalnız şema okundu).
- `KNIGHTS_CAPE` (56 satır): `sCapeIndex, strName, nBuyPrice, nDuration, byGrade, nBuyLoyalty, byRanking`. Sunucu okuyucu `KnightsCapeSet.h` bu kolonlarla uyumlu `[D]`.
- `KNIGHTS_RATING`, `USER_KNIGHTS_RANK`, `KNIGHTS_ALLIANCE`, `KNIGHTS_SIEGE_WARFARE`, `KING_*` tabloları ve prosedürler mevcut: `UPDATE_KNIGHTS_CAPE`, `UPDATE_KNIGHTS_RATING`, `KNIGHTS_RATING_UPDATE`, `DONATE_CLAN_POINTS`, `UPDATE_KNIGHTS_ALLIANCE`, `UPDATE_RANKS`, `CHANGE_KNIGHTS_LEADER` … (toplam ~150 prosedür).
- Harita/NPC içeriği: `K_NPCPOS` zone 21 = 138 satır (eski Moradon NPC'leri: Hera, Hesta, Heppa, Kaishan, Menissiah, Billbor, Moira, Charon, Patrick, Zarta, Gargameth, Chaotic generator, Hero Statue'ler, Guard, Safety Zone Gate1 …). `K_NPC` (521 satır) içinde `24432 [Sentinel]Cape`, `16048–16050 "1st/2nd/3rd place clan"`, `11610/21610 [Knight Clerk]` var; antrenman kuklası, falcı, pet NPC **yok**.
- `ITEM` 85.920 satır, 59 kolon (`… ItemClass, ItemExt`); 1534 `ItemTableSet.h` **aynı şema** `[D]`. `MAGIC` 1.839, `QUEST_HELPER` 3.461, `ZONE_INFO` 31 (zone 21 Type 1 RoomEvent 21; zone 71 Type 2).
- `VERSION`: 1454..1473.

### 2.3 İstemci (`/mnt/c/dev/fdp/Client`, 1,1 GB, 19.622 dosya) `[V]`

- **Protokol sürümü 1453, exe içinde sabit:** `KnightOnline.exe` ofset `0x365004` → `81 F9 AD 05 00 00` (`cmp ecx,1453`, giriş sürüm kontrolü), `0x3660F0` → `B8 AD 05 00 00 C3` (`mov eax,1453; ret`). Kendim bayt düzeyinde doğruladım. Görüntülenen metin biçimi `"Ver. %.3f"` → "Ver. 1.453".
- Exe değiştirilmiş özel sunucu derlemesi: dosya tarihi 2014-09-10, PE zaman damgası sıfırlanmış, ek bölüm; `Launcher.exe` 2015 (özel sunucu MFC launcher'ı, `Server.ini` `Version/Files` ve `IncludeExe/Last` anahtarlarını okur). **`Files=1473` yalnız yama sayacıdır, protokol sürümü değildir.**
- Veri seti 2006 ortası: en yeni veri dosyası 2006-07-25; ana tablolar (`item_org_us.tbl` 2006-07-18, `Skill_Magic_Main_us.tbl` 2006-07-11, `Zones.tbl` 2006-06-30) **şifreli** (`tools/client-tbl-quests.py` yalnız skill tablosunu çözer).
- **Moradon: yalnız eski harita.** `Zones/moradon.gtd` (214.335 B) ve `moradon.opd` (2,70 MB), ikisi de 2006-03-15, 512 m; `moradon_xmas.*` (2005-12). "Yeni Moradon" adlı/ tarihli hiçbir harita yok. 32 harita seti, hepsi 2002–2006; `73.mob` (Ronark Land Base) yok.
- **Pelerin varlıkları eski:** `Data/Cloak.tbl` 1.740 B (2004-10-05, şifreli); `Item/cloak_001–004, 011–013.n3cplug`, `Cloak_C_00..09`, `cloak_M_01..05.dxt`, `clanaddon_*` (2002–2004). Exe bunları `Item\Cloak_%.3d.n3cplug` kalıbıyla yükler. UI: `co_KnightsMantleShop_us.uif`, `co_knights_crest_us.uif`, `co_page_knights_Union_*` (2004–2006). Uzun pelerin modeli/kademe arayüzü bulunmadı (adlandırmadan çıkarım) `[I]`.
- İstemci kaynak kodu **yok** (hiçbir sürüm için). `N3BASE/` depoda yalnız 4 sunucu tarafı dosya.
- `/mnt/c/dev/fdp/_extract/maps_quests`: 24 SMD (hepsi 2005–2006 adlı), 8 `.aievt`, 114 Lua görev (2014); `server/Map` ve `server/Quests` ile aynı içerik.

### 2.4 Korunacak özel geliştirmeler (`git diff 0f52027..main`) `[D]`

- Toplam 416 dosya, +128.898 satır. Sunucu tarafında **yeni** dosyalar: `GameServer/Bot/*` (BotManager, ActionExecutor, BotSession, ScenarioRunner, ScriptRunner, NavService, Telemetry), `GameServer/PacketTrace.*`, `GameServer/DamageTrace.*`, `BotCore/` (yalnız başlık, sunucu başlığı içermez), `Tests/`, `tools/`, `db/001–008`, `bots/`, `plans/`, `docs/`.
- **Üst kaynak dosyalarına dokunan değişiklikler 16 dosyayla sınırlı ve küçük:** `GameServer/User.cpp` (+69/−1: `m_botSink`, `Send/SendCompressed` geçersiz kılma, izleme kancaları), `ChatHandler.cpp` (+87: `/bot` komutları), `GameServerDlg.cpp` (+20: BotManager/NavService başlat-durdur, zaman aşımı muafiyeti), `GameServerDlg.h` (+1), `User.h` (+9), `CharacterSelectionHandler.cpp` (+3/−1), `DatabaseThread.cpp` (+2/−1), `AttackHandler.cpp` (+5), `MagicInstance.cpp` (+5), `shared/KOSocketMgr.h` (+72/−2: ayrılmış oturum havuzu), `shared/SocketMgr.cpp/.h` (+33, +13/−1: `BOT_TICK` olayı), `shared/SocketDefines.h` (+2/−1), `shared/SMDFile.h` (+1: `GetHeights()`), proje dosyaları.
- **Paket düzenine bağımlılık:** `BotCore/Perception.h` `ParseUserInfo` (`:251-298`) `GetUserInfo` bayt düzenini birebir yansıtır (klan/pelerin bloğu dahil; `:289` "10 ekipman × 7 bayt"). Diğer ayrıştırıcılar `ParseMove :339`, `ParseTargetHp :398`, `ParseNpcInfo :838`, `ParsePartyEvent :1237`, `ParseSkillEvent :1826`. `ActionExecutor` istemci paketlerini (`WIZ_MOVE`, `WIZ_ATTACK`, `WIZ_MAGIC_PROCESS`, `WIZ_STATE_CHANGE`, `WIZ_PARTY`, `WIZ_CHAT`, `WIZ_REGENE`) `docs/03` §14'teki düzenle üretir. **Hedef istemci bu düzenleri değiştirirse sunucu işleyicisi ve BotCore ayrıştırıcısı birlikte değişmelidir.**
- `db/002..008` `USERDATA.strItem` bloblarını 73 yuvalı düzende yazar (`INVENTORY_TOTAL`); 1534'te aynı `[D]`.
- `NavService` zone 71 SMD'sinden yükseklik ve olay ızgarası okur (`NavService.cpp:191-202`); `BotCore/RoamRoutes`, F11 dolaşım verisi ve `tools/nav-*` bu haritaya bağlıdır.

---

## 3. 1453 ↔ 1534 sunucu kaynak karşılaştırması `[D]`

Karşılaştırma: üst kaynak `0f52027` (git archive) ile `ForcePower/KODevelopers-1534` `master` (GitHub, son itme 2017-12-09; W-13). Yalnız `.cpp/.h/.c`, boşluk ve satır sonu farkları yok sayıldı.

| Ölçü | Değer |
|---|---|
| Ortak dosya | 232 (1453: 232, 1534: 234; yalnız 1534'te: `GameServer/Header.h`, `shared/database/ItemExpirationSet.h`) |
| Birebir aynı | **159** |
| Farklı | 73 (+5.946 / −1.464 satır) |
| `shared/packets.h` | +5/−1 (yalnız `TEMPLE_*` enum ve `AG_CSW_OP_CL`; **opcode kümesi aynı**) |
| `shared/JvCryption.cpp` | tek anahtar `0x1257091582190465` ("1453 & 1534 crypto" yorumu); bizde 1453 dalı `0x7412580096385200` |
| `shared/SMDFile.*`, `N3BASE/*`, `shared/database/ItemTableSet.h`, `KnightsSet.h`, `KnightsCapeSet.h`, `ZoneInfoSet.h` | aynı |
| `GameServer/Define.h` | +18/−4: `MIN_LEVEL_NATION_BASE 1→30`, `MIN_LEVEL_ESLANT 40→60`, `MAX_LEVEL_ARDREAM 59→62`, kar savaşı/BDW sabitleri |
| En büyük farklar | `GameServerDlg.cpp` +1604 (bowl etkinliği, tapınak, kar savaşı, çevrim içi hediye, "lunar gold shells"), `ChatHandler.cpp` +1049 (GM komutları), `User.cpp` +587/−147 (`OnDeath`, `Initialize`, `ItemGet`, hız denetimi), `AIServer/Npc.cpp` +343/−181, `KnightsManager.cpp` +196/−180 (ittifak düzeltmeleri; **NP bağışı kaldırılmış**), `MagicInstance.cpp` +252/−96 |
| Protokol düzeni | `GetUserInfo`, `MoveProcess`, `Attack` yük düzeni, `SendMyInfo` değişmemiş (farklar etkinlik/ileti ekleri) |
| Kalite notu | 1534 kaynağı giriş şifresini konsola yazar (`LoginSession.cpp`), çok sayıda hata ayıklama çıktısı ve "visualdev" etkinlik yamaları içerir |

**Sonuç `[D]`/`[I]`:** 1534 kaynağı **yükseltme kaynağı değil, uyumluluk referansıdır.** 1534 istemcisi bu sunucu ailesinde çalıştığına göre 1453→1534 arasında istemci tarafı paket düzeni değişikliği ya yoktur ya da sunucunun görmezden geldiği alanlardadır. 1505–1507 için aynı çıkarım geçerlidir (aradaki sürüm). Kesinleşmesi paket kaydıyla olur (§8, T-UPG-01).

---

## 4. Tam yükseltme için neler değişmeli

### 4.1 Protokol / paketler

| Değişiklik | Kanıt | Etiket |
|---|---|---|
| `__VERSION` hedefe çekilir (1534 veya 1505/1507). ≥1500 dalı açılır: sunucu listesi yanıtına `u16 echo` eklenir (`LoginSession.cpp:166`); `<1600` bilinmeyen bayt 1 kalır (`LoginServer.cpp:106`) | §2.1 | `[D]` |
| Kripto özel anahtarı hedef exe ile eşleşmeli. Aday: `0x1257091582190465` (1534 kaynağı); mevcut `0x7412580096385200` yerel 1453 exe'siyle çalışıyor (insan girişleri yapıldı) | `JvCryption.cpp:6-13`; 1534 `JvCryption.cpp` | `[A]` |
| `GameServer VersionCheck` istemciye `__VERSION` gönderir; hedef exe kendi sabitiyle karşılaştıracaktır (bugünkü exe'de `cmp ecx,1453`). Değer eşleşmezse istemci giriş yapmaz | `LoginHandler.cpp:14`; exe `0x365004` | `[V]`/`[I]` |
| **Öneri:** sürüm ve anahtarı derleme sabiti yerine `.ini`'den okunur yap (`LogInServer.ini`/`GameServer.ini`), böylece geçiş boyunca 1453 ve hedef istemci aynı derlemeyle sırayla test edilir. Dokunulan yerler 3–4 `#if` noktasıdır | §2.1 | `[Ö]` |
| Oyun içi paket düzenlerinde değişiklik **beklenmiyor** (1534 kaynağı aynı düzeni işler); kesinleştirme: `FDP_PACKET_TRACE` ile hedef istemciden kayıt ve 1453 kaydıyla karşılaştırma | §3 | `[I]`→`[A]` |
| `WIZ_PACKETn` yer tutucuları: hedef istemci yeni bir opcode gönderirse izleyici "bilinmeyen opcode" olarak yakalar; gerekirse işleyici eklenir | `packets.h:94-148`, `User.cpp:277-530` | `[A]` |

### 4.2 Veri tabanı

| Değişiklik | Neden | Etiket |
|---|---|---|
| `VERSION` satırları hedef istemcinin `Server.ini Files=` sayacına kadar olmalı; yoksa launcher FTP'den yama ister (`HandlePatches`) | `LoginSession.cpp:39-60` | `[D]` |
| `ZONE_INFO` zone 21 → yeni Moradon SMD adı; `START_POSITION` zone 21; `K_NPCPOS` zone 21 (138 eski satır) yeni konumlarla; `K_OBJECTPOS/K_OBJECTEVENT`; warp'lar SMD içinde | §2.1, §2.2 | `[D]` `[V]` |
| Yeni NPC'ler `K_NPC` (kukla, falcı, pet, rehber, arena, pelerin satıcısı konumu) ve bunların istemci `Npc_us.tbl`/`NPC_Looks.tbl` kimlikleriyle eşleşmesi | §1 içerik; §2.2 | `[A]` |
| `KNIGHTS_CAPE` satırları hedef istemcinin `Cloak.tbl` kimlikleriyle eşleşmeli (uzun pelerin kimlikleri dahil); `byGrade/byRanking` değerleri kademe modeline göre | `NPCHandler.cpp:852-858` | `[A]` |
| Kademe kuralı verisi: resmî eşikler (252k / 1,08M) ile koddaki derece eşikleri (720k/340k/50k/25k) farklı kavramlardır (derece vs kademe); kural kararı gerekir | §2.1, §1 | `[S]` `[D]` |
| `ITEM`/`MAGIC`/`K_NPC`/`QUEST_*` kimlikleri hedef istemci tablolarıyla uyumlu olmalı. Mevcut `ITEM` 85.920 satır büyük olasılıkla daha geç dönem dökümü; doğrulanmadı. 1534 paketinde DB **yok** | §2.2; r10dev (W-16) | `[A]` |
| `db/0xx` betikleri: tüm değişiklikler geri alınabilir SQL + rollback kalıbıyla (`db/README.md`) | mevcut kalıp | `[Ö]` |

### 4.3 Haritalar

| Değişiklik | Kanıt | Etiket |
|---|---|---|
| Sunucu: yeni Moradon `.smd` (aynı SMD biçimi). Kaynağı: dönem sunucu dosyası paketi **veya** hedef istemcinin `moradon.gtd/.opd`'sinden üretim. Depoda SMD **ayrıştırıcı** var (`docs/appendix/tools/smd_parse.py`, `tools/nav-export.py`), **üretici yok** | §2.1; `WarpGateEditor.exe` yalnız warp düzenler | `[D]` `[A]` |
| AIServer: aynı SMD + `Map/21.aievt` yenilenmeli | `AIServer/ServerDlg.cpp:430-458` | `[D]` |
| İstemci: `Zones/moradon.*` hedef istemciden gelir | §2.3 | `[V]` |
| **Ronark Land (zone 71):** hedef istemcinin `freezone_a` (veya adı değişmişse) `.gtd`'si bizim `freezone_a_20050718.smd` ile aynı araziyi tanımlamalı; değilse sunucu SMD'si, `NavService` ızgarası, `RoamRoutes` ve F11 dolaşım verisi yeniden türetilir. **Bot projesi için en yüksek etkili bilinmeyen** | §2.4 | `[A]` |

### 4.4 Varlıklar (assetler)

- Sunucu tarafında varlık yoktur: SMD, `.aievt`, Lua görevleri dışında her şey istemcidedir `[D]`.
- İstemci tarafı uzun pelerin modelleri, yeni Moradon zone dosyaları, pelerin dükkânı/kademe arayüzü, `Cloak.tbl`, `Zones.tbl`, NPC/eşya tabloları **hedef istemciyle birlikte gelir**; ayrı tedarik gerekmez `[I]`.
- Klan sembol önbelleği (`Client/symbol_us/`) sunucu tarafından yazılıyor (2026-10-06/07 dosyaları) → sembol akışı bugün çalışıyor `[V]`.

### 4.5 Oyun sistemleri

| Sistem | Bugün | Gerekli | Etiket |
|---|---|---|---|
| Klan kademesi (Squire/Knight/Royal × 5) | Bayraklar var; yükseltme yalnız Lua `PromoteClan` | Puan eşiğinden otomatik kademe **veya** görev zinciri (Isiloon/Fire Drake odaları) — kapsam kararı | `[D]` `[S]` |
| Pelerin satın alma | Var; DB isteği `r,g,b` uyumsuz; RGB gönderilmiyor | `WIZ_CAPE` isteğini düzelt; RGB'nin dönemde kullanılıp kullanılmadığı istemci testiyle belirlenir | `[D]` `[A]` |
| Uzun pelerin | Sunucu yalnız `sCape` kimliği gönderir; istemci uzunluğu kademe/kimlikten çizer | `KNIGHTS_CAPE.byRanking` ile Royal kademeye sınırlama (kod var) | `[D]` `[I]` |
| NP bağışı / klan fonu / onur puanı | Var | Doğrulama | `[D]` |
| Klan duyurusu, ittifak, ilk 10 | Var | Doğrulama; ittifak pelerin yayılımı | `[D]` |
| Moradon çıkış seviyesi 35 | `MIN_LEVEL_NATION_BASE 1` | Sabit veya ini ile 35 (1534 kaynağı 30 kullanıyor) | `[D]` `[S]` |
| BDW, Juraid, arena, kuklalar, falcı, pet | Zone sabitleri ve kısmi etkinlik kodu var; pet yok | **Önerilen: ilk kapsam dışı** (PK bot hedefiyle ilgisiz) | `[Ö]` |

---

## 5. Elimizdeki dosyalar yeterli mi? Eksik listesi

| Bileşen | Elimizde | Kanıt | Durum |
|---|---|---|---|
| Hedef istemci **1505–1507** | **Yok** (yerelde) | §2.3, §1.1 | Internet Archive'da `KnightOnLineSetup_1506.exe` (resmî kurulum adı ve boyutuyla) var (W-20); indirilip exe sabiti ve içerik doğrulanmalı; resmî IP/anti-cheat yaması gerekir `[A]` |
| Hedef istemci **1534** | **Yok** (yerelde) | §2.3, §1.1 | Toplulukta "orijinal/temiz" paketler dağıtılıyor (W-14, W-22); kaynağı belirsiz; bütünlük, kötücül yazılım ve lisans doğrulanmadı `[B]` |
| Yeni Moradon sunucu haritası (`.smd`) | **Yok** | `_extract/maps_quests/Map`, `server/Map` | Üretici araç da yok `[V]` |
| Yeni Moradon DB içeriği (NPC konumları, başlangıç, warp, görevler) | **Yok** | §2.2 | `[V]` |
| Hedef dönem DB (ITEM/MAGIC/NPC/QUEST istemciyle uyumlu) | Belirsiz | §2.2 | Mevcut DB topluluk derlemesi; eşleşme doğrulanmadı `[A]` |
| Klan pelerin verisi | Kısmen (`KNIGHTS_CAPE` 56 satır; istemci `Cloak.tbl` 2004) | §2.2, §2.3 | Hedef istemciyle eşleşme doğrulanmadı `[A]` |
| Sunucu kaynak kodu | **Var** (bizim) + 1534 referans (GitHub, lisans dosyası yok) | §3 | Yeterli `[D]` |
| İstemci kaynak kodu | **Yok** | §2.3 | Gerekli değil; ancak IP/anahtar için exe yaması gerekebilir `[I]` |
| Lua görevleri (yeni Moradon) | **Yok** (114 dosya 2014, eski içerik) | §2.3 | `[V]` |
| Araçlar | SMD ayrıştırıcı, paket izleyici, test çatısı **var**; GTD→SMD üretici, TBL çözücü (genel) **yok** | `tools/`, `docs/appendix/tools/` | `[D]` |

**Net cevap:** Mevcut dosyalar yükseltme için **yeterli değildir**. Kod tarafı hazırdır; dosya tarafında hedef istemci, dönem haritası ve dönem DB içeriği proje sahibince tedarik edilmelidir. Bunların varlığı varsayılmamalı; tedarik edilince hash'leri ve kaynakları `docs/19`'a yazılmalıdır.

---

## 6. Yaklaşım seçenekleri ve öneri

| Seçenek | Açıklama | Artı | Eksi |
|---|---|---|---|
| **A — Mevcut sunucu kalır, istemci ve veri yükselir (ÖNERİLEN)** | `__VERSION`/anahtar/VERSION uyarlaması; yeni Moradon SMD + DB; klan kuralları tamamlanır; 1534 kaynağı yalnız referans | Bot katmanı ve 128k satır özel iş olduğu gibi kalır; kod farkı küçük (§3); her adım `FDP_PACKET_TRACE` ile ölçülür | Dönem haritası/DB tedariki dışa bağımlı; bazı 1534 düzeltmeleri elle taşınır |
| B — Bot katmanını 1534 kaynağına taşımak | 16 dokunulan dosya + `Bot/` klasörü 1534 ağacına uygulanır | Dönem düzeltmeleri hazır gelir | 1534 kaynağı düşük kaliteli (şifre loglama, hata ayıklama kalıntıları); kazanç küçük; `docs/02-03` satır referansları (0f52027) geçersizleşir; yüksek regresyon riski |
| C — Çok daha yeni istemciye (1.8xx+) sıçramak | Yeni Moradon'u da içerir | Modern varlıklar | Kripto, paket düzeni, DB şeması (ItemExpiration, cospre…) baştan; bot ayrıştırıcıları ve `docs/03` mekanikleri geçersiz; kapsam patlaması. **Önerilmez** |

**Hedef istemci önerisi (2026-10-08, §1.1 sonrası güncellendi):** iki aday aynı dönemdir ve aynı plan geçerlidir; yalnız `__VERSION`, anahtar ve `VERSION` tablosu değişir.

| Aday | Artı | Eksi |
|---|---|---|
| **1506 (archive.org resmî kurulum paketi)** | Kaynağı tek ve izlenebilir (resmî dosya adı/boyutu, md5 kayıtlı); yeni Moradon + yeni klan sisteminin ilk resmî hali; değiştirilmemiş exe | HackShield/XTrap ve resmî sunucu adresi içerir → IP, anahtar ve koruma için exe yaması gerekir (araç: `xmkg/ko-executable-editor`, eski); 2007 verisi, 2008 düzeltmeleri yok; md5 resmî bir kaynakla doğrulanamaz |
| **1534 (topluluk "temiz" istemcisi)** | Topluluk standardı; aynı soydan sunucu kaynağı (KODevelopers) ve çalışan özel sunucular var; genelde koruma kaldırılmış ve IP düzenlenebilir | Kaynağı belirsiz (giriş/ücret duvarı), bütünlük ve kötücül yazılım riski; resmî 1534 kurulum paketi arşivde yok; hangi exe'nin "orijinal" olduğu doğrulanamaz |

**Öneri:** U0'da **ikisini de** edinip envanterlemek (exe sabiti, `Zones/moradon.*` tarihi, `Cloak.tbl`, koruma modülleri, hash). Teknik tercih: doğrulanabilir bir 1534 istemcisi çıkarsa 1534 (daha az yama, daha olgun veri); çıkmazsa 1506 arşiv paketi üzerine exe yaması. Karar K-11 proje sahibindedir.

---

## 7. Aşamalı plan (öneri; kabul edilince `docs/17`'ye "U" faz ailesi olarak eklenir)

Plan kimliği önerisi: `U<N>-<NN>-<ad>.md`, dal `bot/U<N>-<NN>`, her plan küçük ve geri alınabilir. Faz sırası: el sıkışma olmadan hiçbir şey test edilemez; klan görselleri Moradon'da doğrulanır; bu yüzden **protokol → harita → klan**.

### U0 — Karar, tedarik ve envanter (proje sahibi + Claude; kod yok)
- Kararlar: K-11 hedef istemci ve kaynağı; K-12 kapsam (yalnız Moradon + klan mı, ek etkinlikler mi); K-13 sürüm/anahtar ini'den mi; K-14 Ronark haritası değişirse nav verisi yeniden mi (§10).
- Tedarik: hedef istemci (ayrı, boş dizine; hash listesi; kötücül yazılım taraması), dönem SMD'leri veya `.gtd/.opd`, dönem DB içeriği (varsa). Lisans/fikri mülkiyet notu (`docs/18` R-11): istemci ve veri dağıtılmaz.
- Kayıt: ADR (K-11..), `docs/19` W-10..W-16, `docs/18` yeni Q/R kimlikleri, `docs/03` §1'e "hedef dönem" alt bölümü.
- **Kapı:** hedef istemci yerelde, exe sürüm sabiti bayt düzeyinde doğrulanmış (1453'teki gibi), `Zones/moradon.*` tarihi ≥ 2007.

### U1 — Protokol el sıkışması (sunucu kodu, küçük)
- `U1-01` Sürüm ve kripto anahtarını `.ini`'den okunur yap (`__VERSION` varsayılan kalır); `LoginSession ≥1500 echo` ve `LoginServer <1600` dallarını çalışma zamanı değerine bağla; birim test.
- `U1-02` `db/0xx_version_<hedef>.sql` (+rollback): `VERSION` satırları hedef sayaca kadar.
- `U1-03` Paket izleyiciye "bilinmeyen opcode" ve "uzunluk uyuşmazlığı" sayaçları (varsa genişlet), `tools/packet-trace-summary.py` karşılaştırma modu (1453 kaydı ↔ hedef kaydı).
- **Doğrulama (T-UPG-01):** hedef istemciyle giriş → karakter listesi → karakter seçimi → oyun başlangıcı (eski haritalarda); insan oyuncu hareket/R/skill/pot/party/chat; izleyici kaydında bilinmeyen opcode 0, ayrıştırma hatası 0; 1453 kaydıyla fark raporu. Bot regresyonu: `tools/run-tests.sh` (son: 2752 test, 0 başarısız), `tools/check-perception-contract.py` PASS, 8v8 demo kısa koşu.
- **Kapı:** hedef istemci eski Moradon ve Ronark'ta oynanabiliyor; botlar etkilenmedi.

### U2 — Yeni Moradon haritası ve DB içeriği
- `U2-01` SMD tedariki/üretimi: dönem SMD varsa doğrudan; yoksa `.gtd/.opd` → SMD üretici (`docs/appendix/tools/smd_parse.py` tersine; N3ShapeMgr çarpışma bloğu dahil) — bu iş büyük ve riskli, ayrı plan.
- `U2-02` `db/0xx_moradon_<hedef>.sql` (+rollback): `ZONE_INFO` 21, `START_POSITION`, `K_NPCPOS`, `K_NPC` yeni NPC'ler, `K_OBJECTPOS/EVENT`; `Map/21.aievt`; Moradon çıkış seviyesi.
- `U2-03` Lua görevleri (varsa dönem dosyaları; yoksa yalnız zorunlu olanlar).
- **Doğrulama (T-UPG-02):** GameServer ve AIServer haritayı hatasız yükler (log); insan oyuncu şehirde yürür, NPC'ler yerinde ve konuşulabilir, kapılar ve Ronark warp'ı çalışır, düşme/duvara girme yok (`DamageTrace`/konum logu); `tools/nav-regress.sh` zone 71 değişmediyse aynı sonuç.
- **Kapı:** yeni Moradon'da 30 dk insan oturumu, çökme 0, konum anomalisi 0.

### U3 — Klan sistemi ve uzun pelerin
- `U3-01` `WIZ_CAPE` DB isteği düzeltmesi (`r,g,b`) + birim test; RGB'nin gönderilip gönderilmeyeceği istemci testine göre.
- `U3-02` `KNIGHTS_CAPE` ↔ hedef `Cloak.tbl` eşleme betiği (+rollback); pelerin NPC konumu Moradon'da.
- `U3-03` Kademe kuralı: K-12'ye göre (a) klan puanı eşiklerinden otomatik `ClanTypeFlag` güncelleme (`KnightsManager::UpdateKnightsGrade` çağrısı), (b) görev zinciri (Lua `PromoteClan`), veya (c) GM komutu ile elle. Mevcut 4 bot klanı (db/009) test verisidir.
- **Doğrulama (T-UPG-03):** bir klan Training→Promoted→Accredited→Royal; her kademede pelerin dükkânı listesi, satın alma, pelerinin kendi ve başka istemcide görünümü (uzun pelerin yalnız Royal); ittifak pelerini; ilk 10 listesi; NP bağışı ve fon; DB'de `KNIGHTS.sCape/Flag` doğru.
- **Kapı:** insan onayı (ekran görüntüsü kanıtı) + izleyici kaydında klan paketlerinde hata 0.

### U4 — (Opsiyonel, K-12'ye bağlı) ek dönem sistemleri
- Border Defense War, Moradon arena, antrenman kuklaları, Juraid; pet sistemi sunucuda yok (büyük iş, önerilmez). Her biri ayrı plan ve ayrı ADR.

### U5 — Regresyon, dokümantasyon ve kapanış
- Tam test: `run-tests.sh`, `nav-regress.sh`, `bot-scenario-diff.py` ile U0 öncesi kayıt ↔ sonrası (8v8 ve dolaşım), `bot-telemetry-report.py` bütçe hükmü (MET-PERF-02).
- İnsan testleri: 1453 ve hedef istemciyle aynı senaryo (sürüm ini ile), fark listesi.
- Dokümanlar: `docs/03` §1 (sürüm kimliği), `docs/02` (yeni ini anahtarları), `docs/12` (Ronark haritası değiştiyse), `docs/15` T-UPG-*, `docs/18` Q/R kapanışı, `docs/19` W-10+, `docs/20` eşleme, ADR'ler, faz raporu `docs/phase-reports/U-taslak.md`, `STATUS.md`.

### Sıralama ve koruma ilkeleri `[Ö]`
- Her aşama kendi `bot/U*-NN` dalında; `main`'e yalnız `/plan-dogrula` ile; otonom döngü çalışırken ortak dosyalar (`docs/18/19`, `STATUS.md`) döngü dışı bekleme noktasında güncellenir.
- Çalışma zamanı anahtarları (`[UPGRADE] CLIENT_VERSION`, `CRYPTO_KEY`) varsayılanda bugünkü değerlerdir; 1453 istemci her an geri dönüş yoludur.
- Bot katmanına **hiçbir aşamada** mantık değişikliği yapılmaz; yalnız paket düzeni gerçekten değişirse ayrıştırıcı/üretici (Perception.h, ActionExecutor) güncellenir ve bu ayrı plan olur.

---

## 8. Doğrulama yöntemi (çalıştığını nasıl anlarız)

| Kimlik (öneri) | Ne | Araç | Geçme ölçütü |
|---|---|---|---|
| T-UPG-01 | El sıkışma ve temel oyun (hedef istemci) | `FDP_PACKET_TRACE`, `tools/trace-session.sh`, `packet-trace-summary.py` | Bilinmeyen opcode 0, uzunluk hatası 0; giriş-seçim-başlangıç < 10 sn; hareket/R/skill/pot/party/chat paketleri 1453 kaydıyla aynı düzende |
| T-UPG-02 | Yeni Moradon yükleme ve yürüyüş | Sunucu logları, konum/hasar izi, insan oturumu | Yükleme hatası 0, 30 dk oturumda çökme 0, konum anomalisi 0, warp'lar çalışır |
| T-UPG-03 | Klan kademe + pelerin uçtan uca | İnsan testi + izleyici + DB SELECT (yalnız `KNIGHTS` şema dışı okuma yasak; bot klanı satırları test verisi) | Her kademede doğru pelerin listesi; uzun pelerin yalnız Royal; ittifak yayılımı; fon/bağış tutarlı |
| T-UPG-04 | Bot regresyonu | `run-tests.sh`, `check-perception-contract.py`, 8v8 ve dolaşım demoları, `bot-scenario-diff.py` | Test sayısı/sonuç değişmez; senaryo farkı yalnız harita kaynaklıysa açıklanır |
| T-UPG-05 | Geri dönüş | Sürüm ini'si 1453'e çekilir | 1453 istemci aynı derlemeyle oynanır |
| T-UPG-06 | Performans | `PERF_SAMPLE`, `bot-telemetry-report.py` | MET-PERF-02 bütçesi aşılmaz |

Çalıştırılmayan test "geçti" yazılmaz; her kayıt `docs/templates/TEST_EVIDENCE.md` ile.

---

## 9. Temel riskler

| Kimlik (öneri) | Risk | Olasılık | Etki | Azaltma |
|---|---|---|---|---|
| R-UPG-01 | Hedef istemci bulunamaz, bütünlüğü/lisansı şüpheli, kötücül içerik | Orta | Yüksek | Ayrı dizin, hash, tarama; dağıtım yok (R-11); 1505–1507 yoksa 1534 |
| R-UPG-02 | Kripto anahtarı/sürüm sabiti exe ile uyuşmaz, exe yaması gerekir | Orta | Orta | Anahtar ini'den; 1534 kaynağı adayı; paket izleyici el sıkışma kaydı |
| R-UPG-03 | İstemci `.tbl` kimlikleri ile DB (ITEM/MAGIC/NPC/QUEST) uyuşmaz → yanlış görünüm/çökme | **Yüksek** | Yüksek | Dönem DB tedariki; TBL çözücü genişletme (`client-tbl-quests.py` kalıbı); kimlik karşılaştırma betiği |
| R-UPG-04 | Ronark Land haritası dönemde değişmiş → nav ızgarası, dolaşım rotaları, F11 verisi geçersiz | Orta | **Yüksek** | U0'da `.gtd` karşılaştırması; değiştiyse K-14 ve ayrı faz |
| R-UPG-05 | Gizli paket düzeni değişikliği (1453→hedef) | Düşük | Yüksek | T-UPG-01 fark raporu; BotCore ayrıştırıcıları tek planla güncellenir |
| R-UPG-06 | Klan kuralı anlambilimi (derece vs kademe, eşikler) yanlış kurgulanır | Orta | Orta | K-12 kararı, Kalais/resmî eşikler `[S]`, insan testi |
| R-UPG-07 | SMD üretimi (gtd→smd) çarpışma/olay ızgarasında hata | Orta | Yüksek | Dönem SMD tedariki öncelikli; üretim gerekiyorsa `smd_parse.py` ile çift yönlü doğrulama |
| R-UPG-08 | Paralel otonom döngülerle doküman/STATUS çakışması | Yüksek | Düşük | Ayrı dallar; ortak dosyalar bekleme noktasında |
| R-UPG-09 | Hedef istemcide hile koruması (HackShield vb.) sunucu akışını engeller | Bilinmiyor | Yüksek | U0 envanterinde kontrol; topluluk dağıtımları genelde kaldırılmış `[B]` |
| R-UPG-10 | Kapsam genişlemesi (BDW, pet, etkinlikler) | Yüksek | Orta | K-12 ile kapsam; U4 opsiyonel ve ADR kapılı |

---

## 10. Karar soruları (tek tek sorulacak; `docs/adr/` ile kayıt)

1. **K-11 (ilk soru) — Hedef istemci:** elinizde doğrulanmış bir 1505–1507 istemcisi var mı? Yoksa 1534 hedeflenecek mi? Kaynağı ve dosya hash'i U0'da kaydedilecek.
2. **K-12 — Kapsam:** yalnız yeni Moradon + klan kademesi/uzun pelerin mi; yoksa BDW/arena/kuklalar da mı?
3. **K-13 — Sürüm/anahtar çalışma zamanı anahtarı:** ini'den okunsun mu (önerilen), yoksa derleme sabiti mi?
4. **K-14 — Ronark haritası değişirse:** nav/dolaşım verisi yeniden mi türetilir, yoksa eski Ronark SMD'si korunup yalnız Moradon mu değişir (istemci arazisi farklıysa botlar insanla farklı yüzeyde yürür; bu kabul edilemez olabilir)?

---

## 11. Kaynaklar (kabul edilince `docs/19`'a W-10..W-16 olarak eklenecek)

| Kimlik | Kaynak | Ne için |
|---|---|---|
| W-10 | GamingNexus, "New Expansion now out for Knights Online", 2007-09-13 — https://www.gamingnexus.com/News/5811/New-Expansion-now-out-for-Knights-Online | Çıkış tarihi, içerik listesi |
| W-11 | MMORPG.com, "Moradon Expansion Patch Goes Live", 2007-09-14 — http://www.mmorpg.com/knight-online/news/moradon-expansion-patch-goes-live-1000008727 | Çıkış tarihi, kademe eşikleri (252k / 1,08M) |
| W-12 | DonanımHaber, "Moradon_AutoPatch_1505, Patch 1506, Patch 1507 ve Auto Patch 1505 Sorunu Çözümü…", 2007-09-13 — https://forum.donanimhaber.com/moradon-autopatch-1505-patch-1506-patch-1507-ve-auto-patch-1505-sorunu-cozumu--17553443-18 | Yama numaraları 1505/1506/1507 |
| W-13 | GitHub `ForcePower/KODevelopers-1534` (son itme 2017-12-09) — https://github.com/ForcePower/KODevelopers-1534 | 1534 sunucu kaynağı; `version.h`, `Define.h`, kripto anahtarı; karşılaştırma tabanı |
| W-14 | ko-yardim.com ve kodevelopers.net arama özetleri ("USKO 1534 Moradon: The Resurrection") | Topluluk adlandırması `[B]` |
| W-15 | Kalais' Library, "KO Evolution / Moradon – The Resurrection" — http://ko.kalais.net/evolution.php (sertifika hatası; düz HTTP ile alındı) | 15 kademe, uzun pelerin, NP maliyetleri, Isiloon/Fire Drake görevleri, K2 ön izlemeleri (Temmuz–Ağustos 2007) |
| W-16 | r10dev, "Knight Online Server Files 1534 (KODevelopers 1534 Open Source Server Source)" — https://r10dev.net/konular/knight-online-server-files-1534-kodevelopers-1534-open-source-server-source.15359/ | Paketin yalnız kaynak kod olduğu; istemci/DB/harita/görev içermediği |
| W-17 | Wayback Machine 2008-01-15 — http://web.archive.org/web/20080115093835/http://www.knightonlineworld.com/moradon_patchtxt.php | Resmî "Expansion Version 1505" README (içerik listesi, klan puanı eşikleri 144k/252k/1,08M) |
| W-18 | Wayback Machine, `knightonlineworld.gamersfirst.com/patches.php` anlık görüntüleri 20080717023940, 20080804233825, 20080901025059, 20081004093524, 20081107095557, 20081217062108, 20090207032914 | Resmî yama listesi: 1506–1532 (Tem–Eyl 2008), 1533 (Eki–Ara 2008), 1706–1717 (Şub 2009) |
| W-19 | frmtr, "Patch 1534 Çıktı" — https://www.frmtr.com/oreads/2185415-patch-1534-cikti.html ; "USKO 1534 KoXP" — https://www.frmtr.com/knight-online/2197047-usko-1534-koxp-broksin-koxp.html (bot erişimi engelli; yalnız arama motoru özeti) | 1534 tarihi (Ekim 2008), premium saatlik, XTrap → `kol.exe` `[B]` |
| W-20 | Internet Archive, "Knight Online Client" (`knightonline2003`) — https://archive.org/details/knightonline2003 (meta veri: https://archive.org/metadata/knightonline2003) | `KnightOnLineSetup_1506.exe` ve diğer tam kurulum paketleri (boyut, md5) |
| W-21 | Wayback Machine 2008-01-01 — http://web.archive.org/web/20080101065526/http://www.knightonlineworld.com/moradon_download.php | Resmî dağıtımda tam istemci adı `KnightOnLineSetup_1506.exe`, oto-yama `Moradon_AutoPatch_1505` |
| W-22 | kodevelopers.net konu 35 "Orjınal 1534 Client Sorunsuz" (2026-04-06); ko-cuce.net konu 80380 (2023-11-27); elfdaily.com konu 3213 (2018-03-03) ve 4887 (2023-09-20); knightlobby.com konu 32510; r10.net 2853336 (2021-06); ko4life.net topic 26 (MYKO v1534, 2011); ko-pserver.com ve mmtop200.com listeleri; ragezone OhaGaming v1534 konuları (403) | 1534 dağıtım ve sunucu manzarası `[B]` |

Yerel kanıtlar: `shared/version.h`, `LogInServer/LoginSession.cpp`, `GameServer/LoginHandler.cpp`, `GameServer/Knights.h`, `GameServer/KnightsManager.cpp`, `GameServer/NPCHandler.cpp`, `GameServer/DatabaseThread.cpp`, `shared/SMDFile.cpp`, `BotCore/Perception.h`; `/mnt/c/dev/fdp/Client/KnightOnline.exe` (`0x365004`, `0x3660F0`), `Client/Zones/moradon.*`, `Client/Data/Cloak.tbl`, `Client/Server.ini`; `FDP_kn_online` şema ve referans tabloları.

---

## Değişiklik günlüğü

| Tarih | Sürüm | Değişiklik |
|---|---|---|
| 2026-10-08 | v1.0 | İlk sürüm (analiz ve plan önerisi; karar bekliyor) |
| 2026-10-08 | v1.1 | §1.1 eklendi (1534 araştırması: resmî 1505 README, resmî yama listesi zaman çizgisi, 1534 ≈ Ekim 2008, archive.org 1506 kurulum paketi, dağıtım manzarası, anahtar çıkarımı); §5 ve §6 hedef istemci satırları güncellendi; W-17..W-22 |
| 2026-10-08 | v1.2 | §1.2 eklendi: 1534 dosya kaynakları listesi (S-01..S-27; erişim durumu, soy ayrımı, indirme sonrası kontrol listesi) |
