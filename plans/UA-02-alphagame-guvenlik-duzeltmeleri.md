# UA-02: AlphaGame güvenlik düzeltmeleri (istemcinin kullanmadığı paketler, mühür, ışınlanma, parola günlüğü, kimlik bilgileri)

| Alan | Değer |
|---|---|
| Durum | HAZIR (UA-01 `yukseltme/alpha`'ya birleşince başlar) |
| Faz | UA — Sürüm yükseltme tabanı AlphaGame 1534 (ADR-0069 madde 2) |
| Branch | `bot/UA-02` (taban: UA-01 birleştikten sonraki `yukseltme/alpha`) |
| Bağımlı olduğu planlar | UA-01 |
| İlgili gereksinim / kabul | ADR-0069 madde 2; `docs/reports/u0-1534/A-kaynak-kod-karsilastirmasi.md` §6 ve Ek C (#1–#7, #13, WordGuard); `D-paket-duzeni-farki.md` (istemcinin işlemediği opcode'lar) |
| Tahmini büyüklük | M |
| Hazırlayan / tarih | Claude / 2026-10-08 |

---

## 1. Amaç

AlphaGame kodu ilk kez çalıştırılmadan önce A §6'daki istismar edilebilir açıkları kapatmak. İlke: **1534 istemcisinin hiç göndermediği ya da işlemediği paketler sunucuda kabul edilmez**; istemcinin kullandığı yollarda eksik denetimler eklenir. Meşru istemci davranışı değişmez.

## 2. Bağlam (okunması zorunlu)

- A §6 tablosu ve Ek C (#1–#26), özellikle #1 mühürle karakter ele geçirme, #2 `WIZ_MOVING_TOWER` ışınlanma, #3 parola günlüğü, #4 mühür açma sınır denetimi ve `GetItem` bir fazla, #5 `ZoneMilitaryCamp`, #6 `WIZ_CAPTURE`, #7 sabit SQL kimlik bilgileri, #13 Menissia `MerchantListSend` null; A §6 sonundaki `WordGuardSystem` notu (bizim `LoginHandler.cpp` hesap adı denetimi).
- D raporu: istemcinin işlemediği opcode'lar `0x84, 0x85, 0x8B, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0xA1` (istemci anahtarı ≤ 0x90). İstemcinin gönderdiği sabit düzenli paketler: `/tmp/claude-1000/-mnt-c-Users-frkoz-OneDrive-Desktop-Fire-Drake-Project-v1453/fbfeef86-7e71-4d83-9948-3d869babf2db/scratchpad/k1534/d/sends_new.txt` (1534 istemcisi) ve `sends_our.txt`; istemci durumları `final_cases.txt`, `client_cases.txt`.
- Bizim (yükseltme hattı) eşdeğer düzeltmeler: `yukseltme/1534` üzerindeki `LogInServer/LoginSession.cpp` (U1-06), `GameServer/LoginHandler.cpp`.

## 3. Kapsam

**Var:**
1. **Kullanılmayan opcode'lar:** `CUser` paket dağıtımında yukarıdaki dokuz opcode reddedilir (paket yok sayılır, bir kez günlüğe yazılır, bağlantı kesilmez). İşleyici kodu silinmez.
2. **#1 mühür:** mühür/mühür açma alt işlemleri. İstemcinin bu alt işlemleri gönderip göndermediğini `sends_new.txt` ve istemci durum listelerinden göster. Gönderiyorsa: hedef karakterin isteyen hesaba ait olduğu sunucuda doğrulanır (hesabın karakter listesi; kaynağı kodda göster); doğrulanamıyorsa işlem başarısız döner. Göndermiyorsa alt işlem reddedilir.
3. **#4:** mühür açma yolunda `InvSlot`/`AccSlot` sınır denetimi; `CUser::GetItem` `pos >= INVENTORY_TOTAL` için `nullptr` (bir fazla okuma düzeltilir).
4. **#5 `ZoneMilitaryCamp`** (`WIZ_ZONE_CHANGE` alt işlem 4): istemci göndermiyorsa reddedilir; gönderiyorsa yalnız geçerli kamp bölgeleri ve NPC/kapı bağlamı.
5. **#6:** `m_tBorderCapure` kurucuda başlatılır (0x85 zaten madde 1 ile kapanır).
6. **#3 parola günlüğü:** `LogInServer/LoginSession.cpp` günlük ve printf satırlarında parola hiçbir biçimde yazılmaz (`ID=<hesap> Authentication=<sonuç>`).
7. **#7:** `GameServerDlg.cpp`, `AIServer/ServerDlg.cpp`, `LogInServer/LoginServer.cpp` ini okumalarındaki sabit kullanıcı adı/parola varsayılanları boş dizgeye çevrilir.
8. **#13:** `MerchantListSend` null denetimi.
9. **WordGuard:** hesap adı karakter denetimi bizim `LoginHandler.cpp` sürümündeki gibi geri gelir.

**Yok:** oyun kuralı değişikliği (Genie süresi, Menissia ışınlanma kuralı, `+prison` ulus seçimi); DB; paket düzenleri (UA-03); bot kancaları (UA-04); AIServer bağlantı adresi.

## 4. Dokunulabilecek dosyalar

| Dosya | Değişiklik |
|---|---|
| `GameServer/User.cpp` | yalnız paket dağıtımı ve `GetItem` (UA-03 aynı dosyada MyInfo'ya dokunacak; o fonksiyonlara dokunma) |
| `GameServer/User.h` | `GetItem`, kurucu başlatma |
| `GameServer/SealHandler.cpp`, `GameServer/UpgradeHandler.cpp` | #1, #4 |
| `GameServer/CharacterMovementHandler.cpp` | #5 |
| `GameServer/MerchantHandler.cpp` | #13 |
| `GameServer/LoginHandler.cpp` | WordGuard |
| `GameServer/GameServerDlg.cpp`, `AIServer/ServerDlg.cpp`, `LogInServer/LoginServer.cpp`, `LogInServer/LoginSession.cpp` | #7, #3 |

## 5. Uygulama adımları

1. Her bulgu için: AlphaGame kod alıntısı, istemcinin bu yolu kullanıp kullanmadığı (kanıt satırı), düzeltme. Tablo raporda.
2. Derle (Release, Debug), testleri koş.

## 6. Kabul kriterleri

- [ ] K1: Release ve Debug 0 hata, yeni uyarı yok.
- [ ] K2: `./tools/run-tests.sh Release` UA-01 sonrası taban ile aynı, 0 başarısız.
- [ ] K3: §3'ün her maddesi için önce/sonra kod alıntısı ve istemci kullanım kanıtı.
- [ ] K4: `grep -a -n "password" LogInServer/LoginSession.cpp`: hiçbir günlük/printf argümanında parola yok.
- [ ] K5: #7 dosyalarında sabit kimlik bilgisi dizgesi kalmadı (grep; değer yazdırılmadan, yalnız satır numarası).
- [ ] K6: `git diff --stat` yalnız §4; kodlama korunur; `git status --short` temiz.

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release
grep -a -n "password" LogInServer/LoginSession.cpp
git diff --stat yukseltme/alpha...bot/UA-02
git status --short
```

## 8. Kısıtlar ve uyarılar

- Sunucu başlatılmaz (çalışma zamanı denetimi bu plan birleştikten sonra Claude'un işi).
- Kimlik bilgisi değerleri hiçbir dosyaya ve rapora yazılmaz.
- Git: `AGENTS.md` §2.8; commit `[UA-02] ...`; push yok.

---

## Uygulayıcı Raporu

(Uygulayıcı doldurur.)

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(Henüz yok.)
