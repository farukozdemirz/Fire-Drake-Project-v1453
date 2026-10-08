# U1-02: 1534 `WIZ_MYINFO` düzeni ve 32 bit ağırlık alanları (profil kapılı)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | U1 — Sürüm yükseltme 1534, protokol (`docs/17` §2 U, ADR-0068 madde 3) |
| Branch | `bot/U1-02` (taban: `yukseltme/1534`) |
| Bağımlı olduğu planlar | U1-01 (KAPANDI; `shared/ProtocolProfile.h`) |
| İlgili gereksinim / kabul | T-UPG-01 (giriş → oyuna giriş), T-UPG-05 (1453 yolu aynı) |
| Tahmini büyüklük | S–M (2 dosya, isteğe bağlı `User.h`) |
| Hazırlayan / tarih | Claude / 2026-10-08 |

---

## 1. Amaç

1534 istemcisi `WIZ_MYINFO`'yu (oyuna girişte karakterin kendi bilgisi) ve bazı paketlerdeki ağırlık alanlarını 1453'ten farklı okur. Bu düzenler **istemci exe'sinin statik çözümünden** çıkarıldı (`docs/reports/u0-1534/D-paket-duzeni-farki.md` §3.1, §5 madde 7 ve 10, §6 U1-02/U1-03). Bu plan, yalnız `ProtocolProfile::ClientVersion(__VERSION) >= 1534` iken bu düzenleri üretir; 1453 yolu **bayt bayt aynı** kalır.

Not: ALPHA bu paketlerde istemciyle **uyuşmuyor** (MyInfo'da 74 eşya gönderiyor, istemci 72 okuyor; `LevelChange`/`PointChange` ağırlığını u16'ya kırpıyor). Referans istemcinin okuma sırasıdır (D §3.1); ALPHA yalnız başlık alanları için yardımcıdır.

## 2. Bağlam (okunması zorunlu)

- `docs/reports/u0-1534/D-paket-duzeni-farki.md` §0, §3.1 (MyInfo bayt düzeni — **bağlayıcı**), §4 (envanter sabitleri), §5, §6 U1-02 ve U1-03.
- İstemci izi (salt referans, depo dışı): `/tmp/claude-1000/-mnt-c-Users-frkoz-OneDrive-Desktop-Fire-Drake-Project-v1453/fbfeef86-7e71-4d83-9948-3d869babf2db/scratchpad/k1534/d/final_cases.txt` (`0xe` = MyInfo satırı; ağırlık paketleri için D §6 U1-03'teki adresler).
- `shared/ProtocolProfile.h` (U1-01), `shared/globals.h:192-253` (yuva sabitleri: `SLOT_MAX` 14, `HAVE_MAX` 28, `CWING` 42 … `CTOP` 46, `BAG1/2` 47/48, `INVENTORY_MBAG` 49, `INVENTORY_TOTAL` 73).
- `GameServer/User.cpp`: `CUser::SendMyInfo` (`:947`), `CUser::LevelChange` (`:1829`, ağırlık yazımı ~`:1866-1872`), `CUser::PointChange` (`:1897`, ~`:1912`), `CUser::SendItemMove` (`:3366`, ~`:3378`), `CUser::AllPointChange` (`:3976`, ~`:4216-4219`).
- `GameServer/ItemHandler.cpp`: `CUser::SendItemWeight` (`:520-525`).
- `GameServer/Knights.h:62-63` (`m_sCape`, `m_bCapeR/G/B`).
- ALPHA referansı (salt okunur): `/mnt/c/dev/fdp1534/alpha/ALPHA KO 1534 PROJE/1-Game Source/GameServer/User.cpp:1074-1146` (MyInfo başlığı).
- Bot alıcısı MyInfo'yu yalnız sayar (`GameServer/Bot/BotManager.cpp` ~`:4285`); bot ayrıştırıcısı yok.

## 3. Kapsam

**Var (yalnız 1534 dalında):**
1. `SendMyInfo` 1534 düzeni, D §3.1'e birebir:
   - `m_bCity` alanı **yazılmaz**.
   - `GetClanID()` sonra `GetFame()` (u8).
   - Klan varsa: `u16 allianceId, u8 flag, str8 clanName, u8 grade, u8 ranking, u16 markVer, u16 capeId, u8 R, u8 G, u8 B, u8 0`. Klan yoksa: `u64 0, u16 0xFFFF, u32 0`. (Mevcut 1453 kodundaki ittifak pelerini davranışını koru: pelerin kimliği bugünkü `GetCapeID` ile aynı kaynaktan; RGB `m_bCapeR/G/B`'den.)
   - Ardından sabit `u8 2, u8 3, u8 4, u8 5`.
   - `u16 maxHp, u16 hp, u16 maxMp, u16 mp`, **`u32 maxWeight, u32 itemWeight`**, sonra 1453'teki stat/direnç/altın/yetki/sıralama/skill çubuğu sırası (D §3.1'deki sırayla karşılaştır; fark varsa D'ye uy ve raporla).
   - Eşyalar, her biri 19 bayt (`u32 id, u16 dur, u16 count, u8 flag, u16 rentalTime, u32 sealSerial, u32 expiry`): yuva 0..13, 14..41, **43, 44, 45, 46** (kanat 42 yazılmaz), 47, 48, 49..72. Toplam 72 kayıt. Mühür alanı 0 değilse istemci ek mühür bloğu okur: bugünkü 1453 kodu mühür bilgisi tutuyorsa D §3.1'deki bloğu yaz; tutmuyorsa `u32 0` yaz ve raporda belirt.
   - Kuyruk: `u8 accountStatus, u8 premiumType, u16 premiumTime, u8 chicken, u32 manner` (1453 kuyruğuyla karşılaştır).
2. Ağırlık alanları u32 (yalnız 1534): `LevelChange`, `PointChange`, `SendItemMove` yanıtı, `AllPointChange`, `SendItemWeight`. Diğer alanların sırası değişmez.
3. 1453 yolu: hiçbir bayt değişmez. Kod düzeni: `if (ProtocolProfile::ClientVersion(__VERSION) >= 1534) { …1534… } else { …mevcut kod aynen… }` ya da ortak kısımları paylaşan eşdeğer yapı; ama 1453 çıktısı aynı kalmalı.

**Yok:** UserInfo/NpcInfo/bölge değişimi (U1-03), klan RGB'nin başka paketlerdeki kayıtları (U1-05), pelerin satın alma isteği (U1-04), envanter sabitlerinin değiştirilmesi, DB, bot kodu.

## 4. Dokunulabilecek dosyalar

| Dosya | Değişiklik |
|---|---|
| `GameServer/User.cpp` | `SendMyInfo`, `LevelChange`, `PointChange`, `SendItemMove`, `AllPointChange` içinde 1534 dalları |
| `GameServer/ItemHandler.cpp` | `SendItemWeight` 1534 dalı |
| `GameServer/User.h` | yalnız yeni yardımcı bildirimi gerekirse (ör. `WriteMyInfoItem1534`) |

## 5. Uygulama adımları

1. `SendMyInfo`'nun bugünkü 1453 çıktısını alan alan listele (raporda tablo). D §3.1 ile yan yana koy; farkları işaretle.
2. 1534 dalını yaz; eşya döngüsünü D §3.1 sırasına göre yardımcı fonksiyonla yaz.
3. Ağırlık alanlarını u32 yap (yalnız 1534).
4. **Bayt uzunluğu kanıtı:** profil 1534 iken klansız, çantasız bir karakter için MyInfo'nun 1453'e göre uzunluk farkını hesapla (D §6 U1-02 "1453 uzunluğu + 551" tahminini kendi tablonla doğrula ya da düzelt) ve raporda göster. Sunucu çalıştırılmaz; hesap koddan yapılır.
5. Derle, testleri koş.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` ve `Debug` hatasız, yeni uyarı yok.
- [ ] K2: `./tools/run-tests.sh Release` → taban (`yukseltme/1534`, 3470) ile aynı sayı, 0 başarısız.
- [ ] K3: 1453 eşdeğerliği: `git diff yukseltme/1534...bot/U1-02` içinde mevcut 1453 satırlarının yalnız bir `else` bloğuna taşındığı/dokunulmadığı görülür; raporda her değişen fonksiyon için "1453 çıktısı aynı" gerekçesi.
- [ ] K4: §5.1 ve §5.4 tabloları raporda; MyInfo 1534 alan sırası D §3.1 ile bire bir (her satır için ✔).
- [ ] K5: Kanat (42) 1534 MyInfo'da yazılmıyor; 72 eşya kaydı (kodla göster).
- [ ] K6: Kodlama/BOM/CRLF korunur (`file` önce/sonra).
- [ ] K7: `git diff --stat` yalnız §4; `git status --short` temiz.

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release
git diff --stat yukseltme/1534...bot/U1-02
git diff yukseltme/1534...bot/U1-02 -- GameServer/User.cpp GameServer/ItemHandler.cpp
git status --short
```

## 8. Kısıtlar ve uyarılar

- Oyun mekaniği değişmez; yalnız kablo düzeni.
- Profil kontrolü her çağrıda `ProtocolProfile::ClientVersion(__VERSION) >= 1534`.
- Sunucu başlatma/dağıtım yok (Claude yapar). İndirilen paketteki exe'ler çalıştırılmaz.
- Git: `AGENTS.md` §2.8; commit `[U1-02] ...`.

---

## Uygulayıcı Raporu

(Uygulayıcı doldurur.)

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(Henüz yok.)
