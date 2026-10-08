# U1-03: 1534 UserInfo, adsız NpcInfo, üç parçalı bölge değişimi ve bot ayrıştırıcıları (profil kapılı)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | U1 — Sürüm yükseltme 1534, protokol (`docs/17` §2 U, ADR-0068 madde 3) |
| Branch | `bot/U1-03` (taban: `yukseltme/1534`) |
| Bağımlı olduğu planlar | U1-01 (KAPANDI) |
| İlgili gereksinim / kabul | T-UPG-01 (oyuncu/NPC görme), T-UPG-04 (bot regresyonu), T-UPG-05 |
| Tahmini büyüklük | M (6 dosya) |
| Hazırlayan / tarih | Claude / 2026-10-08 |

---

## 1. Amaç

1534 istemcisi oyuncu bilgisini (`GetUserInfo` → `WIZ_USER_INOUT`, `WIZ_REQ_USERIN`), NPC bilgisini (`GetNpcInfo` → `WIZ_NPC_INOUT`, `WIZ_REQ_NPCIN`) ve bölge değişimini (`WIZ_REGIONCHANGE`) 1453'ten farklı okur (`docs/reports/u0-1534/D-paket-duzeni-farki.md` §3.2, §3.3, §6 U1-04/U1-05; istemci exe statik çözümü). Botlar bu paketleri sunucudan süreç içinde aldığı için **bot ayrıştırıcıları da aynı profile göre** okumalı; aksi halde 1534 profilinde botlar oyuncuları/NPC'leri yanlış görür ve bölge değişiminde oyuncu tablosu silinir (D §6 U1-05).

Bu plan, sunucu tarafını `ProtocolProfile::ClientVersion(__VERSION) >= 1534` arkasına, bot tarafını açık bir **düzen parametresine** bağlar. 1453 yolu ve mevcut bot davranışı bayt bayt aynı kalır.

## 2. Bağlam (okunması zorunlu)

- D raporu §3.2 (UserInfo kaydı 1534 — **bağlayıcı**), §3.3 (NpcInfo, REGIONCHANGE), §6 U1-04, U1-05 ve ortak kural (BotCore'a `WireLayout` parametresi; BotCore sunucu global'ine bağlanmaz).
- Sunucu: `GameServer/CharacterMovementHandler.cpp:99-161` (`CUser::GetUserInfo`), `GameServer/Npc.cpp:138-155` (`CNpc::GetNpcInfo`), `GameServer/GameServerDlg.cpp:1362-1380` (`RegionUserInOutForMe`).
- Bot: `BotCore/Perception.h` `ParseUserInfo` (`:251-299`), `ParseNpcInfo` (`:838-870`), `ParseRegionList` (`:500`), toplu ayrıştırıcılar (`:311`, `:331`, `:883`, `:905`); `GameServer/Bot/BotSession.cpp:397-545` (USER_INOUT, REQ_USERIN, REGIONCHANGE, NPC_INOUT, REQ_NPCIN dalları); `Tests/BotCoreTests/PerceptionTests.cpp` (`AddUserInfo` fikstürü ~`:42`).
- Gözlem sözleşmesi: `tools/check-perception-contract.py` (R4: BotCore yalnız standart ve kardeş başlıkları içerir — `shared/ProtocolProfile.h` BotCore'a **dahil edilmez**).
- ALPHA referansı (salt okunur): `…/1-Game Source/GameServer/CharacterMovementHandler.cpp:103-283`, `Npc.cpp:144-163`, `GameServerDlg.cpp:1520-1544`.

## 3. Kapsam

**Var:**
1. **Sunucu (yalnız 1534 dalında):**
   - `GetUserInfo`: klan varsa pelerin kimliğinden sonra `u8 R, u8 G, u8 B, u8 0, u8 2`; klan yoksa 14 baytlık blok `u16 0, u8 0, u32 0, u16 0xFFFF, u16 0, u8 0, u16 0` (D §3.2'yi bire bir uygula). Eşya listesi 8 ana parça (BREAST, LEG, HEAD, GLOVE, FOOT, SHOULDER, RIGHTHAND, LEFTHAND) + 4 cospre **`CTOP, CHELMET, CLEFT, CRIGHT`** sırasıyla (12 × 7 bayt). Diğer alanlar 1453 ile aynı sırada (D §3.2 ile karşılaştır, fark varsa D'ye uy ve raporla).
   - `GetNpcInfo`: `str8 name` yazılmaz. `GetType() == 15` ise (istemci bu türde iki dize okur; DB'de şu an yok) tek satır uyarı logu (`printf` değil, mevcut log yardımcısıyla; yoksa raporda sor) ve 1453 davranışı değil, D §3.3'teki iki dizeyi yaz (bilinmeyen dize için boş dize).
   - `RegionUserInOutForMe`: üç paket — `[u8 0]`, `[u8 1, u16 n, u16 × n]`, `[u8 2]`; her biri bugünkü gönderim yoluyla (`SendCompressed` ya da mevcut eşdeğer).
2. **BotCore (saf, test edilebilir):**
   - `struct WireLayout { bool v1534 = false; };` (ya da eşdeğer, varsayılan eski düzen) `Perception.h` içinde.
   - `ParseUserInfo`, toplu UserInfo ayrıştırıcıları ve `ParseNpcInfo`, toplu NPC ayrıştırıcıları düzen parametresi alır (varsayılan eski düzen → mevcut çağrılar ve testler değişmeden derlenir).
   - 1534: UserInfo'da pelerin kimliğinden sonra 5 bayt atlanır (klansız blok 14 bayt — ayrıştırıcı klan dalını doğru yürütmeli), eşya bloğu 84 bayt; NpcInfo'da ad okunmaz (`out.name` boş).
   - Bölge değişimi için saf yardımcı: `ParseRegionChange1534(data, len, ids, cap, int & part)` → `part` 0/1/2; 1'de liste döner.
3. **BotSession:** düzeni `ProtocolProfile::ClientVersion(__VERSION) >= 1534` ile belirler (yalnız `GameServer/Bot/` içinde `shared/ProtocolProfile.h` dahil edilir) ve ayrıştırıcılara geçirir. `WIZ_REGIONCHANGE` 1534'te: alt kod 0 ve 2 **hiçbir şey silmez**; alt kod 1 bugünkü liste işlemini (Retain + bekleyen istek) yapar. 1453 dalı aynen kalır.
4. **Testler (`PerceptionTests.cpp`):** 1534 UserInfo fikstürü (klanlı, klansız, iki kayıtlı liste), 1534 NpcInfo (adsız), bölge değişimi 0/1/2 dizisinde tablonun silinmediği; mevcut 1453 testleri aynen geçmeli.

**Yok:** MyInfo/ağırlıklar (U1-02), klan RGB'nin diğer paketleri (U1-05), `RoamMonSense.h` ve `ActionExecutor.cpp` (değişmez; D §6 U1-05), envanter sabitleri, DB.

## 4. Dokunulabilecek dosyalar

| Dosya | Değişiklik |
|---|---|
| `GameServer/CharacterMovementHandler.cpp` | `GetUserInfo` 1534 dalı |
| `GameServer/Npc.cpp` | `GetNpcInfo` 1534 dalı |
| `GameServer/GameServerDlg.cpp` | `RegionUserInOutForMe` 1534 dalı |
| `BotCore/Perception.h` | `WireLayout`, düzen parametreli ayrıştırıcılar, `ParseRegionChange1534` |
| `GameServer/Bot/BotSession.cpp` | düzen seçimi, REGIONCHANGE 1534 dalı |
| `Tests/BotCoreTests/PerceptionTests.cpp` | 1534 testleri |

## 5. Uygulama adımları

1. Bugünkü `GetUserInfo`/`GetNpcInfo`/`RegionUserInOutForMe` çıktılarını alan alan listele; D §3.2/§3.3 ile yan yana tabloya koy (rapor).
2. Sunucu 1534 dallarını yaz.
3. BotCore ayrıştırıcılarını düzen parametresiyle genişlet; 1453 varsayılan.
4. BotSession'ı bağla.
5. Testleri yaz; derle; tüm testleri koş; `python3 tools/check-perception-contract.py` (veya mevcut çağrı biçimi) PASS.
6. **Çapraz kontrol:** test fikstüründe 1534 UserInfo baytlarını **sunucunun ürettiği düzenle aynı sırada** kur (alanları D §3.2'den al) ve ayrıştırıcının sid, konum, sınıf, seviye, klan, ölü/canlı alanlarını doğru okuduğunu göster.

## 6. Kabul kriterleri

- [ ] K1: Release ve Debug hatasız, yeni uyarı yok.
- [ ] K2: `./tools/run-tests.sh Release` → taban (3470) + yeni testler, 0 başarısız; mevcut Perception testleri değişmeden geçer.
- [ ] K3: `tools/check-perception-contract.py` PASS (BotCore'da `shared/` dahil edilmemiş).
- [ ] K4: 1453 eşdeğerliği: sunucu ve BotSession'da 1453 dalı satır satır aynı (diff ile göster); BotCore'da varsayılan düzen eski sonuçları verir (mevcut testler).
- [ ] K5: §5.1 tablosu raporda; UserInfo 1534 alan sırası D §3.2 ile bire bir; cospre sırası `CTOP, CHELMET, CLEFT, CRIGHT`.
- [ ] K6: REGIONCHANGE 1534 testinde 0 → 1 → 2 dizisi sonunda oyuncu tablosu yalnız listedeki kimlikleri tutar ve 0/2 paketleri tabloyu boşaltmaz.
- [ ] K7: Kodlama/BOM/CRLF korunur; `git diff --stat yukseltme/1534...bot/U1-03` yalnız §4; `git status --short` temiz.

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release
python3 tools/check-perception-contract.py
git diff --stat yukseltme/1534...bot/U1-03
git status --short
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §2.3–2.6: mekanik yok, bot avantajı yok, thread kuralı. Bot yalnız kendine gelen paketi ayrıştırır.
- BotCore saf kalır; profil bilgisi yalnız `GameServer/Bot/` üzerinden gelir.
- Sunucu başlatma/dağıtım yok. İndirilen paketteki exe'ler çalıştırılmaz.
- U1-02 aynı anda başka dalda `User.cpp`/`ItemHandler.cpp` üzerinde çalışıyor; bu dosyalara dokunma.
- Git: `AGENTS.md` §2.8; commit `[U1-03] ...`.

---

## Uygulayıcı Raporu

(Uygulayıcı doldurur.)

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(Henüz yok.)
