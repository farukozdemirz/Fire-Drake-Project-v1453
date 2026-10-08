# U1-04: 1534 istemci isteklerindeki ek alanlar (tamir, pelerin RGB, warp listesi) ve PK sıralaması (profil kapılı)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | U1 — Sürüm yükseltme 1534, protokol (`docs/17` §2 U, ADR-0068 madde 3) |
| Branch | `bot/U1-04` (taban: U1-02 birleştikten sonraki `yukseltme/1534`) |
| Bağımlı olduğu planlar | U1-01 (KAPANDI), U1-02 (aynı dosya `User.cpp`) |
| İlgili gereksinim / kabul | T-UPG-01, T-UPG-03 (pelerin), T-UPG-05 |
| Tahmini büyüklük | S–M (3 dosya) |
| Hazırlayan / tarih | Claude / 2026-10-08 |

---

## 1. Amaç

1534 istemcisi bazı **istemci → sunucu** isteklerine 1453'te olmayan alanlar ekler ve Ronark'ta PK sıralamasını (`WIZ_RANK`) farklı ister/okur (`docs/reports/u0-1534/D-paket-duzeni-farki.md` §2.6, §2.7, §6 "U1-06" önerisi). Bu alanlar okunmazsa sonraki alanlar kayar: tamir yanlış NPC'ye gider, pelerin rengi kaybolur, warp listesi yanlış açılır, sıralama penceresi bozulur. Plan, bu dört işleyiciyi `ProtocolProfile::ClientVersion(__VERSION) >= 1534` arkasında 1534 düzenine getirir ve pelerin satın alımındaki **bilinen DB isteği hatasını** (sunucu `WIZ_CAPE` DB isteğine `r,g,b` yazmıyor, okuyucu okuyor; `docs/reports/u0-1534/A-kaynak-kod-karsilastirmasi.md` §2) düzeltir. 1453 yolu bayt bayt aynı kalır (DB isteği düzeltmesi hariç: o, iki profilde de yanlış okumayı giderir).

## 2. Bağlam (okunması zorunlu)

- D raporu §2.6, §2.7, §6 "U1-06" maddeleri; istemci izleri `/tmp/claude-1000/-mnt-c-Users-frkoz-OneDrive-Desktop-Fire-Drake-Project-v1453/fbfeef86-7e71-4d83-9948-3d869babf2db/scratchpad/k1534/d/final_cases.txt` (salt referans).
- `GameServer/NPCHandler.cpp`: `CUser::ItemRepair` (`:8-70`), `CUser::HandleCapeChange` (`:806-943`; DB isteği `:935-937`, başarı yanıtı ~`:920-923`, RGB yorum satırları `:813-816, 876-887, 911-916`).
- `GameServer/DatabaseThread.cpp:428-435` (`CUser::ReqChangeCape` — `r,g,b` okur), `GameServer/DBAgent.cpp` `UpdateCape`.
- `GameServer/User.cpp`: `CUser::SelectWarpList` (`:4303-4346`), `CUser::HandlePlayerRankings` (`:5355-5505`) — U1-02 birleştikten sonra satırlar kayabilir; fonksiyon adlarıyla bul.
- `GameServer/Knights.h:62-63` (`m_sCape`, `m_bCapeR/G/B`), `shared/database/KnightsSet.h:33-35`.
- ALPHA referansı (salt okunur): `…/1-Game Source/GameServer/NPCHandler.cpp:7-79`, `:1041-1209`, `DatabaseThread.cpp:466-473`; `User.cpp:4865-4908`, `:6217-6369`.

## 3. Kapsam

**Var:**
1. `ItemRepair` (1534): istemcinin eklediği `u16 sNpcID` alanı D'deki sırayla okunur; NPC yakınlık/varlık denetimi bugünkü mantıkla yapılır (yeni denetim **eklenmez**; yalnız alan okunur ve kayma giderilir).
2. `HandleCapeChange` (1534): istekten `u8 r, u8 g, u8 b` (+ D'deki dolgu) okunur. Pelerin seçimi ve şartlar (`byGrade`, `byRanking`, klan puanı, altın) **değişmez**. Başarıda `m_bCapeR/G/B` güncellenir; başarı yanıtına D'deki `RGB0` eklenir; klan güncelleme yayını bugünkü yolla yapılır.
   - Renk boyama izni: D'de ve ALPHA'da renk yalnız belirli kademelere (Accredited/Royal) açık olabilir; kuralı ALPHA `:1041-1209`'dan **oku**, istemcinin gönderdiği renk alanlarını yalnız izin verilen klan türünde kalıcı yaz; izin yoksa 0,0,0. Kuralın kaynağını raporda göster.
3. **DB isteği düzeltmesi (iki profil):** `HandleCapeChange`'in `WIZ_CAPE` DB isteği `clanID, capeID, r, g, b` yazar (okuyucu `ReqChangeCape` ile aynı sıra). 1453 profilinde `r,g,b` = mevcut `m_bCapeR/G/B` (değişmeden kalır).
4. `SelectWarpList` (1534): istemcinin eklediği `npcid` alanı okunur.
5. `HandlePlayerRankings` (1534): istekte tür baytı yok (tür = PK sıralaması); yanıtta tür baytı ve girdi başına premium alanı yok (D §2.7). 1453 dalı aynen.

**Yok:** Klan RGB'nin diğer paketlerdeki kayıtları (JOIN, SendUpdate, ALLY_*) ve görev sayaçları (U1-05), WIZ_BATTLE_EVENT/0x90 ölüm listesi (U1-08 açık maddeler), DB şema değişikliği, bot kodu.

## 4. Dokunulabilecek dosyalar

| Dosya | Değişiklik |
|---|---|
| `GameServer/NPCHandler.cpp` | `ItemRepair`, `HandleCapeChange` |
| `GameServer/User.cpp` | `SelectWarpList`, `HandlePlayerRankings` |
| `GameServer/DatabaseThread.cpp` | yalnız gerekirse `ReqChangeCape` yorumu/okuma doğrulaması (davranış değişmez) |

## 5. Uygulama adımları

1. Dört işleyicinin 1453 okuma/yazma sırasını ve D'deki 1534 sırasını yan yana tabloya koy (rapor).
2. 1534 dallarını yaz; DB isteği düzeltmesini yap.
3. Derle, testleri koş.

## 6. Kabul kriterleri

- [ ] K1: Release ve Debug hatasız, yeni uyarı yok.
- [ ] K2: `./tools/run-tests.sh Release` taban sayısıyla aynı, 0 başarısız.
- [ ] K3: 1453 eşdeğerliği (DB isteği düzeltmesi hariç) diff ile gösterilir.
- [ ] K4: `WIZ_CAPE` DB isteği yazma sırası `ReqChangeCape` okuma sırasıyla bire bir (kod alıntısı).
- [ ] K5: Pelerin şartları (grade/ranking/klan puanı/altın) değişmemiş (diff).
- [ ] K6: §5.1 tablosu raporda; her 1534 alanı D'deki istemci izine bağlanmış.
- [ ] K7: Kodlama korunur; `git diff --stat` yalnız §4; `git status --short` temiz.

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release
git diff --stat yukseltme/1534...bot/U1-04
git status --short
```

## 8. Kısıtlar ve uyarılar

- Oyun mekaniği ve ekonomi değişmez; yalnız kablo düzeni ve kayma giderimi.
- Sunucu başlatma/dağıtım yok. İndirilen paketteki exe'ler çalıştırılmaz.
- Git: `AGENTS.md` §2.8; commit `[U1-04] ...`.

---

## Uygulayıcı Raporu

(Uygulayıcı doldurur.)

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(Henüz yok.)
