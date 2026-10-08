# U1-07: 1534 MyInfo ve sadakat değişiminde klan fonu alanı (profil kapılı)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | U1 — Sürüm yükseltme 1534, protokol (`docs/17` §2 U, ADR-0068 madde 3) |
| Branch | `bot/U1-07` (taban: `yukseltme/1534`) |
| Bağımlı olduğu planlar | U1-02, U1-05 (DOĞRULANDI; `yukseltme/1534` @ `9bcf311d`) |
| İlgili gereksinim / kabul | T-UPG-03 (klan penceresi), T-UPG-05 |
| Tahmini büyüklük | XS (1 dosya) |
| Hazırlayan / tarih | Claude / 2026-10-08 |

---

## 1. Amaç

U1-05 istemci çözümlemesi gösterdi ki 1534 istemcisi oyuncunun **klan fonu** alanını (istemci nesnesinde `+0x960`) üç pakette doldurur: `KNIGHTS_UPDATE` (0x24, U1-05'te `m_nClanPointFund` yazıldı), **`WIZ_MYINFO`** (U1-02'de klan bloğundan sonra yazılan sabit `u8 2, 3, 4, 5`; istemci `0x6f7f61` → `+0x960`) ve **`WIZ_LOYALTY_CHANGE`** (`LOYALTY_NATIONAL_POINTS` yanıtının üçüncü u32'si, bugün `uint32(0)`; istemci `0x70554e` → `+0x960`). Sonuç: klanlı oyuncunun 1534 istemcisi girişte ≈ 84 milyon NP fon gösterir ve her NP değişiminde fon 0'a iner. Bu plan iki yerde gerçek klan fonunu gönderir. Kanıt: `plans/U1-05-…md` Uygulayıcı Raporu §5.1 ve "Açık noktalar 1" (`yukseltme/1534`'te).

## 2. Bağlam (okunması zorunlu)

- `plans/U1-05-1534-klan-paketlerinde-pelerin-rengi-ve-gorev-sayaclari.md` (yükseltme hattındaki sürüm) Uygulayıcı Raporu ve Doğrulama Raporu.
- `GameServer/User.cpp`: `CUser::WriteMyInfo1534` (U1-02; `uint8(2) << uint8(3) << uint8(4) << uint8(5)` satırı), `CUser::SendLoyaltyChange` (`~:653`; `Packet result(WIZ_LOYALTY_CHANGE, uint8(LOYALTY_NATIONAL_POINTS))` ve `uint32(0) // Clan donations(?)`).
- `GameServer/Knights.h` (`m_nClanPointFund`, `Atomic<uint32>`), `Knights.cpp` `CKnights::SendUpdate` (U1-05: aynı alanın 0x24 kaynağı).
- İstemci izleri (salt referans): `/tmp/claude-1000/-mnt-c-Users-frkoz-OneDrive-Desktop-Fire-Drake-Project-v1453/fbfeef86-7e71-4d83-9948-3d869babf2db/scratchpad/k1534/d/` (`dis/new.asm` `0x6f7f61`, `0x70554e`).

## 3. Kapsam

**Var (yalnız 1534 dalında):**
1. `WriteMyInfo1534`: sabit dört bayt yerine `uint32(klan fonu)` — klanı varsa `m_nClanPointFund` (ittifakta da **kendi** klanının fonu; 0x24 ile tutarlı olduğunu raporda göster), klansızsa `0`. Uzunluk değişmez (+551 korunur).
2. `SendLoyaltyChange` (`LOYALTY_NATIONAL_POINTS`): üçüncü u32 = oyuncunun klan fonu (klansız 0). 1453 dalı `uint32(0)` olarak kalır.

**Yok:** fon hesap mantığı, bağış akışı, diğer paketler, DB.

## 4. Dokunulabilecek dosyalar

| Dosya | Değişiklik |
|---|---|
| `GameServer/User.cpp` | `WriteMyInfo1534`, `SendLoyaltyChange` |

## 5. Uygulama adımları

1. İki noktayı değiştir; istemci okuma adreslerini (`0x6f7f61`, `0x70554e`) `dis/new.asm`'de kendin teyit et ve rapora alıntıla.
2. Derle, testleri koş.

## 6. Kabul kriterleri

- [ ] K1: Release ve Debug hatasız, yeni uyarı yok.
- [ ] K2: `./tools/run-tests.sh Release` taban ile aynı, 0 başarısız.
- [ ] K3: 1453 eşdeğerliği (diff); MyInfo 1534 uzunluğu değişmedi (alan tipi u32 → u32).
- [ ] K4: İstemci adresleri raporda alıntılı.
- [ ] K5: `git diff --stat` yalnız §4; kodlama korunur; `git status --short` temiz.

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release
git diff yukseltme/1534...bot/U1-07 -- GameServer/User.cpp
git status --short
```

## 8. Kısıtlar ve uyarılar

- Sunucu başlatma/dağıtım yok. İndirilen paketteki exe'ler çalıştırılmaz.
- Git: `AGENTS.md` §2.8; commit `[U1-07] ...`.

---

## Uygulayıcı Raporu

(Uygulayıcı doldurur.)

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(Henüz yok.)
