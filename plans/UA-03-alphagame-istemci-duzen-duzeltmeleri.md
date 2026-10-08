# UA-03: AlphaGame paket düzenlerini 1534 istemcisine uydurma (73 yuvalı envanter, MyInfo, ağırlıklar, görev sayaçları, klan fonu)

| Alan | Değer |
|---|---|
| Durum | HAZIR (UA-01 `yukseltme/alpha`'ya birleşince başlar) |
| Faz | UA — Sürüm yükseltme tabanı AlphaGame 1534 (ADR-0069 madde 3) |
| Branch | `bot/UA-03` (taban: UA-01 birleştikten sonraki `yukseltme/alpha`) |
| Bağımlı olduğu planlar | UA-01 |
| İlgili gereksinim / kabul | ADR-0069 madde 3; T-UPG-01; `docs/reports/u0-1534/D-paket-duzeni-farki.md` §0, §2; `C-db-semasi.md` §5 |
| Tahmini büyüklük | L |
| Hazırlayan / tarih | Claude / 2026-10-08 |

---

## 1. Amaç

AlphaGame sunucusunun bu istemciyle uyuşmayan düzenlerini istemcinin okuma sırasına getirmek. İstemci çözümlemesi (D raporu, `[C]` kanıtı) bağlayıcıdır. Bizim yükseltme hattında (`yukseltme/1534`) U1-02..U1-07 ile istemciden doğrulanmış düzenler referanstır.

## 2. Bağlam (okunması zorunlu)

- D §0 madde 3–4 ve 6: AlphaGame'in uymadığı noktalar: MyInfo eşya listesi (74 gönderir, istemci 72 okur), ağırlık alanları u16 (istemci u32: `LEVEL_CHANGE`, `POINT_CHANGE`, `ITEM_MOVE` yanıtı, `WEIGHT_CHANGE`, `CLASS_CHANGE`/ALL_POINT), görev 9/1 sayaçları u8 (istemci u16); AlphaGame'in 8 cospre / `CFAIRY` / 74 yuva modeli bu istemcide yok, istemci 73 yuva (`COSP_MAX 5`, `BAG1 47`, `BAG2 48`, `MBAG 49..72`).
- C §5: AlphaGame DB sütunları zaten 73 yuvaya göre (`strItem`/`strSerial` 584, `strItemTime`/`strUserSeal` 292); AlphaGame kodu 592/296 yazıyor (taşma). Düzeltme sonrası kod ve DB aynı boyda olmalı.
- Klan fonu: U1-07 (MyInfo'da klan bloğundan sonraki u32 ve `WIZ_LOYALTY_CHANGE` `LOYALTY_NATIONAL_POINTS` üçüncü u32 = oyuncunun klan fonu; AlphaGame sabit `u8 2,3,4,5` gönderiyor).
- Klan paketleri: `/tmp/claude-1000/-mnt-c-Users-frkoz-OneDrive-Desktop-Fire-Drake-Project-v1453/fbfeef86-7e71-4d83-9948-3d869babf2db/scratchpad/k1534/d/ks_cmp.txt` (alt kod başına istemci okuma şablonu); AlphaGame'in yazım sırası her `DIFF` için karşılaştırılır.
- İstemci kanıt dosyaları: `/tmp/claude-1000/-mnt-c-Users-frkoz-OneDrive-Desktop-Fire-Drake-Project-v1453/fbfeef86-7e71-4d83-9948-3d869babf2db/scratchpad/k1534/d/` (`final_cases.txt`, `client_cases.txt`, `dis/new.asm`).
- Referans uygulama (doğrulanmış): `git show yukseltme/1534:GameServer/User.cpp` (`WriteMyInfo1534`, `WriteMyInfoItem1534`, ağırlıklar, `SendLoyaltyChange`), `ItemHandler.cpp`, `QuestHandler.cpp`, `Knights.cpp`, `KnightsManager.cpp`.

## 3. Kapsam

**Var:**
1. **Envanter modeli 73 yuva:** `shared/globals.h` sabitleri istemci modeline (`INVENTORY_TOTAL 73`, `COSP_MAX 5`, `CFAIRY` yok, `BAG1 47`, `BAG2 48`, `MBAG1 49-60`, `MBAG2 61-72`); bu sabitleri kullanan her yer (eşya taşıma, cospre, DB yükleme/kaydetme arabellekleri, mühür bloğu) tutarlı. `strItem`/`strSerial` 584, `strItemTime`/`strUserSeal` 292 bayt yazılır.
2. **MyInfo:** eşya listesi 72 kayıt, istemcinin okuma sırası; klan bloğundan sonra u32 klan fonu (klansız 0).
3. **Ağırlıklar u32:** D §0.6 listesindeki her paket.
4. **Görev 9/1 sayaçları u16.**
5. **`WIZ_LOYALTY_CHANGE`** `LOYALTY_NATIONAL_POINTS` üçüncü u32 = klan fonu.
6. **Klan paketleri:** `ks_cmp.txt` `DIFF` satırlarında AlphaGame'in yazım sırası istemciye uymuyorsa düzeltilir; uyuyorsa raporda "uyuyor".
7. Raporda opcode başına tablo: AlphaGame yazım sırası (önce), yeni sıra, istemci okuma kanıtı (dosya/satır).

**Yok:** güvenlik (UA-02), bot kancaları (UA-04), DB, oyun kuralı değişikliği.

## 4. Dokunulabilecek dosyalar

`shared/globals.h`; `GameServer/User.cpp` (UA-02 paket dağıtımı ve `GetItem`'a dokunuyor; onlara dokunma), `GameServer/User.h` (`GetItem` hariç), `GameServer/ItemHandler.cpp`, `GameServer/DBAgent.cpp`, `GameServer/QuestHandler.cpp`, `GameServer/Knights.cpp`, `GameServer/KnightsManager.cpp`, `GameServer/DatabaseThread.cpp`, `GameServer/SealHandler.cpp` ve `GameServer/UpgradeHandler.cpp` (yalnız yuva sabitleri; UA-02 güvenlik satırlarına dokunma), envanter sabitini kullanan diğer GameServer dosyaları (raporda gerekçeyle).

## 5. Uygulama adımları

1. Sabitleri kullanan yerlerin tam listesi (`grep`), her birinin yeni modelde doğruluğu.
2. §3 maddeleri; her opcode için istemci kanıtı.
3. Derle (Release, Debug), testleri koş.

## 6. Kabul kriterleri

- [ ] K1: Release ve Debug 0 hata, yeni uyarı yok.
- [ ] K2: `./tools/run-tests.sh Release` UA-01 sonrası taban ile aynı, 0 başarısız.
- [ ] K3: §3.7 tablosu; her satır istemci kanıtıyla.
- [ ] K4: `INVENTORY_TOTAL` 73; DB'ye yazılan blob boyları 584/292 (kod alıntısı).
- [ ] K5: MyInfo toplam uzunluğu ve alan sırası, `yukseltme/1534` `WriteMyInfo1534` ile alan alan karşılaştırma tablosu (fark varsa gerekçe).
- [ ] K6: `git diff --stat` yalnız §4; kodlama korunur; `git status --short` temiz.

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release
git diff --stat yukseltme/alpha...bot/UA-03
git status --short
```

## 8. Kısıtlar ve uyarılar

- Sunucu başlatılmaz.
- UA-02 aynı anda `User.cpp`/`User.h`/`SealHandler.cpp`/`UpgradeHandler.cpp`'ye dokunuyor; çakışmayı en aza indirmek için yalnız §3 kapsamındaki fonksiyonları değiştir.
- Git: `AGENTS.md` §2.8; commit `[UA-03] ...`; push yok.

---

## Uygulayıcı Raporu

(Uygulayıcı doldurur.)

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(Henüz yok.)
