# ADR-0068: Sürüm yükseltme — hedef istemci 1534 (AlphaGame paketi), taban bizim kaynak kod

Durum: KABUL
Tarih: 2026-10-08 · Karar verenler: proje sahibi (hedef paket: AlphaGame 1534; "sen incele, en uygun hali seç"), Claude (taban seçimi, bu ADR)
İlgili: K-11 (hedef istemci), `docs/reports/surum-yukseltme-analizi-moradon-resurrection-2026-10-08.md` §1–§12, faz U (`docs/17` §2 U)

## Bağlam

Proje sahibi Moradon: The Resurrection dönemine (yeni Moradon, yeni klan sistemi, uzun pelerin) yükseltme istedi ve üç arşiv indirdi (`C:\Users\frkoz\Downloads\1534\`). İnceleme (2026-10-08, üç bağımsız ajan + Claude) sonuçları:

- `1534 SRC.rar` = GitHub `ForcePower/KODevelopers-1534` deposunun birebir kopyası (336/336 dosya aynı ad ve boyut) `[V]`. Yeni içerik yok.
- `ALPHA KO 1534 PROJE.rar` (şifre `ko-yardim.com`, SHA-256 `1e8b5e64…0af4`) → `C:\dev\fdp1534\alpha`: AlphaGame kaynak kodu (`__VERSION 1534`), `KN_online.bak` (SQL Server 2019), derlenmiş sunucu, 61 SMD, 599 Lua `[V]`.
- `Knight Online.rar` (aynı şifre, SHA-256 `5eb64794…ac`) → `C:\dev\fdp1534\client`: istemci, veri Eylül 2007'ye kadar (yeni Moradon `moradon.gtd` 2007-02, `Cloak.tbl` 2007-07, uzun pelerin modelleri `cloak_101..113`) `[V]`.
- İstemci exe'si **1534 protokolü** kullanır: `cmp ecx,1534` (ofset `0x3C03C4`), `mov eax,1534; ret` (`0x3C1580`); kripto özel anahtarı `0x1257091582190465` (32 bitlik iki parça halinde) `[V]`. Bizim sunucu bugün 1453 ve `0x7412580096385200` kullanır (`shared/version.h:3`, `shared/JvCryption.cpp:6-13`) `[D]`.
- İstemcide anti-cheat yok; `d3d8.dll` açık kaynaklı d3d8to9 sarmalayıcısı; Windows Defender taraması temiz `[V]`.
- **Ronark Land korunuyor:** yeni istemcinin zone 71 arazisi bizim `freezone_a_20050718.smd` ile 263.169 hücrenin hepsinde aynı; değişen yalnız bowl içindeki birkaç nesne `[V]`. AlphaGame'in kendi zone 71 SMD'si (`freezone_b.smd`) istemciyle uyuşmuyor (yüksekliklerin %73'ü aynı, 52 m'ye varan fark, bowl'da 1.217 yürünebilirlik hücresi farklı) `[V]`.
- AlphaGame kaynağı KODevelopers'tan değil, 1453 ailesinin ayrı bir çatalı (UP→ALPHA 121 dosya değişik, +11.294/−3.526 satır) `[V]`. Doğrulanmış mekaniklerimizden 11'ini bozan değişiklikler içerir (AP bonusu iki kez, ayakta MP yenilenmesi yok, tüm sınıflara hız sınırı 90, R aralık/menzil denetimi kaldırılmış, Type1 yeniden yazılmış, stun/slow direnci, Mage Armor yansıtma, Berserker %20, alan debuff'ı gizlenme) `[A]`; iki YÜKSEK güvenlik açığı (mühür sistemiyle ada göre karakter ele geçirme, `WIZ_MOVING_TOWER` ile GM'siz ışınlanma) `[A]`; AIServer protokolü de değişik.
- AlphaGame DB'si: botların kullandığı 124 skill'in tümünde değerler farklı (menzil ≈ 0,45×, cooldown farklı, buff tipleri farklı); `ITEM.ItemClass` 123.720 satırın hepsinde NULL; kendi koduyla da uyumsuz prosedürler var; bot betiklerimizden db/002, 005, 006, 007 bu DB'de bozulur `[V]`.
- AlphaGame paket düzenleri (sunucu → istemci) bazı paketlerde 1453'ten farklı: kullanıcı bilgisi (+19 bayt: pelerin RGB ve bayrak; 12 ekipman yuvası), NPC bilgisi (ad yok), bölge değişimi (üç paket), envanter (`COSP_MAX` 5→8, 73→74 yuva). İstemcinin hangisini beklediği **çalışma zamanında doğrulanmadı**; istemci AlphaGame sunucusuyla birlikte dağıtıldığı için AlphaGame düzenleri güçlü adaydır `[I]`.

## Karar

1. **Hedef istemci:** AlphaGame paketiyle gelen 1534 istemcisi (`C:\dev\fdp1534\client\Knight Online`). (K-11 yanıtı.)
2. **Taban kaynak kod: bizim depo kalır** (seçenek A). AlphaGame kaynağı yalnız **referans**tır; özellikler tek tek, ayrı planlarla ve mekanik doğrulamayla alınır. AlphaGame sunucusu ve AIServer'ı olduğu gibi alınmaz.
3. **Protokol profili:** sunucu, istemci sürümünü ve kripto anahtarını çalışma zamanında ini'den okur (`[PROTOCOL] CLIENT_VERSION`, varsayılan 1453 = bugünkü davranış). 1534 profilinde istemcinin beklediği paket düzenleri ayrı planlarla eklenir; 1453 profili her zaman geri dönüş yoludur. Bot katmanı (BotCore ayrıştırıcıları) aynı profile göre çalışır.
4. **Veri tabanı:** `FDP_kn_online` korunur ve 1534 için **eklemeli, geri alınabilir** `db/0xx` betikleriyle taşınır (VERSION, pelerin tablosu, yeni eşya/NPC/Moradon satırları). AlphaGame DB'si (`.\SQL2019` → `FDP_alpha1534`) yalnız referans veri kaynağıdır. **MAGIC/MAGIC_TYPE* değerleri değiştirilmez** (botların doğrulanmış kuralları bu veriye dayanır; değişiklik ayrı karar ister).
5. **Haritalar:** zone 71/72 SMD'lerimiz ve bot gezinme verisi aynen kalır. Yeni Moradon için AlphaGame `moradon_0826.smd` yalnız onarılarak (yükseklik ızgarası transpoze, yürünebilirlik ızgarası yeniden üretim) ayrı planla alınır.
6. **Ortam:** SQL Server 2019 Express ayrı örnek (`.\SQL2019`) olarak kuruldu; yalnız referans DB için kullanılır. Oyun sunucusu `.\SQLEXPRESS` (2017) üzerinde kalır.

## Ek 1 (2026-10-08): U2 veri kapsamı — geri alınabilir varsayılanlar

Proje sahibi "en uygun hali sen seç" dediği için Claude aşağıdaki varsayılanları seçti; her biri betik kayıt tablosuyla geri alınabilir ve proje sahibi değiştirebilir (`docs/reports/u0-1534/E-veri-farki.md` §8, §9):

1. **Pelerin değerleri istemci `Cloak.tbl`'dan** (ALPHA'dan değil): ALPHA'da 144 satırda klan puanı 0, 143 satırda kademe şartı yanlış. 168 yeni pelerin, 84'ü uzun (royal, `byRanking` 8–12). Mevcut 56 satıra dokunulmaz.
2. **Eşya kapsamı:** istemcinin çözebildiği ve bizde olmayan tüm kimlikler (≈ 35.860; kaynak ALPHA `ITEM`, `ItemClass`/`ItemExt` E §3.4 kuralıyla üretilir). Yeni satırların seviye/stat şartları ALPHA (dönem) değerleriyle kalır; mevcut satırlardaki düşürülmüş şartlar değişmez.
3. **NPC/canavar:** yalnız istemcinin tanıdığı kimlikler (K_NPC 79, K_MONSTER 60); **24438–24440 aktarılmaz** (bizim zone 64 bekçileri). Yeni canavarların düşürme tablosu yok.
4. **VERSION satırı eklenmez:** U1-01 ile giriş sunucusu 1534 profilinde sürümü ini'den bildirir; satır eklense eski 1453 istemcisinin launcher'ı yama isterdi.
5. **Yeni Moradon (zone 21) yerleşimi ve görevler U3'e** (SMD onarımı ve 552 görev kimliği çakışması nedeniyle).

## Ek 2 (2026-10-08): Yeni Moradon (zone 21) — SMD istemci dosyalarından üretilir; geçişte ayrı 1534 DB'si

`docs/reports/u0-1534/G-yeni-moradon-smd.md` sonucu karar maddesi 5 şöyle güncellenir:

1. **SMD, ALPHA'nınki onarılarak değil, 1534 istemcisinin kendi `Zones/moradon.gtd` (yükseklik, transpozsuz) ve `.opd` (çarpışma bloğu, bayt bayt) dosyalarından üretilir;** yürünebilirlik ızgarası, resmî haritaları %100 yeniden üreten düzenleyici kuralıyla (4 m karede çarpışma poligonu veya köşe yükseklik farkı ≥ 10 m → 0; kenarlar 1) hesaplanır. ALPHA `moradon_0826.smd` yalnız warp ve nesne olayı bağışçısıdır (zone 73 warp'ları çıkarılır). Sebep: ALPHA dosyası istemciden farklı, daha yeni bir harita sürümü (transpozdan sonra bile hücrelerin %11'i 60 m'ye kadar farklı).
2. Zone 21'e giren warp'lar 6 SMD'de (1, 2, 30, 71, 72, 81) yalnız hedef koordinat (8 bayt/kayıt) değiştirilerek (817,530)'a yönlendirilir; zone 71 gezinme parmak izi (`NavService` crc32) değişmemelidir.
3. **Varsayılanlar (proje sahibi değiştirebilir):** warp ücretleri bizimki; Moradon'a varış tek nokta (817,530); oyuncu konum sıfırlaması yapılmaz (karar proje sahibinde); ALPHA futbol nesneleri/görevi alınmaz.
4. **Eski istemci uyumu:** zone 21 verisi DB'de seçildiği için canlı `FDP_kn_online`'a uygulanırsa 1453 istemcisinin Moradon'u bozulur. Geçiş süresince Moradon DB değişiklikleri **ayrı bir 1534 DB kopyasına** (`FDP_kn1534`, ODBC `KO_1534_GAME/MAIN`, çalışma dizini `C:\dev\fdp1534\server`) uygulanır; kesin geçiş (tek DB) proje sahibinin kararıdır.

## Değerlendirilen alternatifler

| Alternatif | Artılar | Eksiler | Neden seçilmedi |
|---|---|---|---|
| B — AlphaGame kaynağını taban alıp bot katmanını üstüne taşımak | Genie, VIP depo, balıkçılık/madencilik, mühür, BDW/Juraid/Chaos yeniden yazımları hazır gelir; kanca taşıma ucuz (42 parçadan 34'ü doğrudan uygulanıyor, 10 çakışma bölgesi) | 11 doğrulanmış mekanik bozulur (bot ayarları ve `docs/03` geçersizleşir); iki YÜKSEK güvenlik açığı; AIServer de değişmek zorunda; DB'si kendi koduyla uyumsuz ve skill verisi farklı; `docs/02`/`docs/03` satır referanslarının tamamı geçersizleşir | Maliyet kanca taşımada değil, yeniden doğrulamada; botların "insanla aynı kurallar" ilkesini riske atar |
| C — AlphaGame DB'sini aynen almak | Daha zengin içerik (123.720 eşya, 224 pelerin, 1.384 NPC) | Skill değerleri farklı; ItemClass boş; bot betiklerinin dördü bozulur; bowl düzenlemesi kaybolur; SQL 2019 gerektirir | Seçici satır aktarımı aynı içeriği risksiz verir |
| D — KODevelopers-1534 kaynağı | Aynı soy, küçük fark | DB/harita/istemci yok; giriş parolasını konsola yazar | İstemciyle eşleşen düzen kanıtı yok; AlphaGame daha güçlü referans |

## Sonuçlar

- Olumlu: bot katmanı, testler (2752+), doğrulanmış mekanikler ve Ronark gezinme verisi korunur; her adım ini profiliyle geri alınabilir.
- Olumsuz: AlphaGame'deki ek sistemler (Genie, VIP depo, balıkçılık/madencilik, mühür, kral sistemi ekleri, olay yeniden yazımları) ancak tek tek taşınırsa gelir.
- Risk: istemcinin beklediği paket düzenleri çalışma zamanında doğrulanana kadar belirsiz (R-UPG-05). Azaltma: U1 planları her düzeni profile bağlar; insan testi `FDP_PACKET_TRACE` ile yapılır.
- Geri alma: `[PROTOCOL] CLIENT_VERSION=1453` ve eski istemci; DB betiklerinin rollback'leri.

## Doğrulama

- T-UPG-01: 1534 istemcisiyle giriş → karakter listesi → oyuna giriş; paket izleyicide bilinmeyen opcode 0.
- T-UPG-04: `tools/run-tests.sh` sonuçları ve 8v8/dolaşım senaryoları U öncesi ile aynı.
- T-UPG-05: `CLIENT_VERSION=1453` ile eski istemci aynı derlemeyle oynanır.
