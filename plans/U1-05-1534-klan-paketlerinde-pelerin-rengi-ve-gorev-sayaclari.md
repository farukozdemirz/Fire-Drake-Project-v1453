# U1-05: 1534 klan paketlerinde pelerin rengi (katılım, güncelleme, ittifak, klan puanı) ve görev sayaçları (profil kapılı)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | U1 — Sürüm yükseltme 1534, protokol (`docs/17` §2 U, ADR-0068 madde 3) |
| Branch | `bot/U1-05` (taban: `yukseltme/1534`) |
| Bağımlı olduğu planlar | U1-01..U1-04 (DOĞRULANDI; `yukseltme/1534` @ `1bbe5bd5`) |
| İlgili gereksinim / kabul | T-UPG-03 (klan kademesi + pelerin uçtan uca), T-UPG-05 |
| Tahmini büyüklük | M (≤ 5 dosya) |
| Hazırlayan / tarih | Claude / 2026-10-08 |

---

## 1. Amaç

1534 istemcisi `WIZ_KNIGHTS_PROCESS` alt kodlarının bir kısmında pelerin kimliğinden sonra **u32 renk alanı** (`R, G, B, 0`) bekler ve 1453'te hiç okumadığı klan puanı alt kodlarını (0x3B–0x41, 0x4F, 0x51) okur. Görev 9/1 sayaçlarını da u16 bekler (`docs/reports/u0-1534/D-paket-duzeni-farki.md` §2.7, §6 "U1-07" önerisi; istemci karşılaştırması `…/scratchpad/k1534/d/ks_cmp.txt`). Bu plan, sunucunun ürettiği bu paketleri `ProtocolProfile::ClientVersion(__VERSION) >= 1534` arkasında istemcinin okuma sırasına getirir. 1453 yolu bayt bayt aynı kalır. Klan kuralları, ekonomi ve ittifak mantığı **değişmez**.

## 2. Bağlam (okunması zorunlu)

- D raporu §2.7 ve §6 "U1-07"; `ks_cmp.txt` (alt kod başına 1453 ve 1534 istemci okuma şablonları — **bağlayıcı**): `/tmp/claude-1000/-mnt-c-Users-frkoz-OneDrive-Desktop-Fire-Drake-Project-v1453/fbfeef86-7e71-4d83-9948-3d869babf2db/scratchpad/k1534/d/ks_cmp.txt` (ayrıca `final_cases.txt`, `dis/new.asm` gerekirse).
  - 0x02 JOIN: 1534 `{u8 u16 u16 u8 u8 u16 u16 **u32** u16 u16 STR u8 u8}` (1453'te u32 yok).
  - 0x1C/0x1E/0x1F/0x20/0x21/0x22/0x24: her klan kaydında iki yerde `u32` ve sonda `u32 u32` (ks_cmp satırlarına bak).
  - 0x3B–0x41, 0x4F, 0x51: 1534 istemci şablonları; sunucunun bugünkü yanıtlarını bunlarla karşılaştır.
- Sunucu: `GameServer/KnightsManager.cpp` (`PacketProcess` `:8-108`, `RecvUpdateKnights` `:606-644`, ittifak oluşturucuları `:983-1300`, `ListTop10Clans` `:1303`, `DonateNPReq/DonateNP/DonationList` `:1349-1420`), `GameServer/Knights.cpp` (`CKnights::SendUpdate` `:321-328`), `GameServer/DatabaseThread.cpp` (ittifak yanıtları ~`:538-750`), `GameServer/QuestHandler.cpp` (`QuestV2MonsterCountAdd` `:151-187`, `QuestV2MonsterDataRequest` `:210-232`; 18xx biçimi yorum satırında).
- Renk kaynağı kuralı (U1-02/U1-03 ile tutarlı): her pakette renk, **o pakette gönderilen pelerin kimliğinin klanından** alınır.
- ALPHA referansı (salt okunur; yalnız kablo düzeni için): `…/1-Game Source/GameServer/Knights.cpp:456-491`, `KnightsManager.cpp`, `DatabaseThread.cpp`, `QuestHandler.cpp`.

## 3. Kapsam

**Var (yalnız 1534 dalında):**
1. ks_cmp.txt'de `DIFF` olan ve sunucunun ürettiği her `WIZ_KNIGHTS_PROCESS` yanıtı için 1534 düzeni (u32 renk alanları ve ek alanlar). Sunucunun **hiç üretmediği** alt kodlar raporda listelenir, kod eklenmez.
2. 0x3B–0x41, 0x4F, 0x51: sunucu bugün üretiyorsa düzeni ks_cmp'ye göre doğrula/düzelt; üretmiyorsa listele.
3. Görev sayaçları (`QuestV2MonsterCountAdd`, `QuestV2MonsterDataRequest`): 1534'te sayaçlar u16 (yorumdaki 18xx biçimi D'ye uygunsa onu kullan; değilse D'ye uy).

**Yok:** kademe yükseltme kuralları, ittifak mantığı, renk ücreti, yeni alt kod işleyicileri, DB, bot kodu.

## 4. Dokunulabilecek dosyalar

| Dosya | Değişiklik |
|---|---|
| `GameServer/KnightsManager.cpp` | 1534 yanıt düzenleri |
| `GameServer/Knights.cpp` | `SendUpdate` 1534 |
| `GameServer/DatabaseThread.cpp` | yalnız ittifak yanıt oluşturucuları (`ReqSkillDataLoad`'a **dokunma**; U1-06'nın) |
| `GameServer/QuestHandler.cpp` | görev sayaçları 1534 |

## 5. Uygulama adımları

1. ks_cmp.txt'deki her `DIFF` alt kodu için: sunucuda üreten fonksiyon(lar), 1453 yazım sırası, 1534 istemci okuma sırası — tablo (rapor).
2. 1534 dallarını yaz.
3. Derle, testleri koş.

## 6. Kabul kriterleri

- [ ] K1: Release ve Debug hatasız, yeni uyarı yok.
- [ ] K2: `./tools/run-tests.sh Release` → taban (3484) ile aynı, 0 başarısız.
- [ ] K3: 1453 eşdeğerliği diff ile; her değişen fonksiyon için gerekçe.
- [ ] K4: §5.1 tablosu; her 1534 yazım sırası ks_cmp satırıyla bire bir.
- [ ] K5: Klan/ittifak/ekonomi mantığı değişmedi (diff).
- [ ] K6: Kodlama korunur; `git diff --stat` yalnız §4; `git status --short` temiz.

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release
git diff --stat yukseltme/1534...bot/U1-05
git status --short
```

## 8. Kısıtlar ve uyarılar

- U1-06 aynı anda `DatabaseThread.cpp` içinde **yalnız** `ReqSkillDataLoad`'a ve `LoginSession.cpp`'ye dokunuyor; o fonksiyona dokunma.
- Sunucu başlatma/dağıtım yok. İndirilen paketteki exe'ler çalıştırılmaz.
- Git: `AGENTS.md` §2.8; commit `[U1-05] ...`.

---

## Uygulayıcı Raporu

(Uygulayıcı doldurur.)

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(Henüz yok.)
