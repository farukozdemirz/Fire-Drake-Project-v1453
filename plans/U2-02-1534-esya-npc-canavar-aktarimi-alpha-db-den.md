# U2-02: 1534 eşya, NPC ve canavar aktarımı (`db/013`, `db/014`; kaynak ALPHA DB, kapsam istemcinin tanıdığı kimlikler)

| Alan | Değer |
|---|---|
| Durum | TASLAK (U2-01 `main`'e birleşince HAZIR) |
| Faz | U2 — Sürüm yükseltme 1534, veri (`docs/17` §2 U, ADR-0068 madde 4 ve Ek 1) |
| Branch | `bot/U2-02` (taban: U2-01 birleştikten sonraki `main`) |
| Bağımlı olduğu planlar | U2-01 (`tools/kotbl.py`) |
| İlgili gereksinim / kabul | T-UPG-01/04 hazırlığı; `docs/reports/u0-1534/E-veri-farki.md` §3, §4, §8 |
| Tahmini büyüklük | M (1 üretici, 4 SQL, README bölümü) |
| Hazırlayan / tarih | Claude / 2026-10-08 |

---

## 1. Amaç

1534 istemcisinin tanıdığı ama bizim DB'de olmayan eşya, NPC ve canavar satırlarını, kişisel veri içermeyen referans tablolara (**ITEM, K_NPC, K_MONSTER**) **eklemeli ve geri alınabilir** biçimde aktarmak. Kaynak satırlar ALPHA DB'sidir (`.\SQL2019` → `FDP_alpha1534`); kapsam **yalnız istemcinin tanıdığı kimlikler**. Mevcut satırların hiçbiri değişmez; botların kullandığı 83 eşya ve tüm MAGIC verisi aynen kalır.

Kararlar (ADR-0068 Ek 1, geri alınabilir varsayılanlar):
- Eşya kapsamı: istemcinin çözebildiği ve bizde olmayan **tüm** kimlikler (E §3.2: 35.860; hepsi ALPHA'da var).
- Yeni eşyaların seviye/stat şartları **ALPHA değerleriyle** kalır (istemcinin gösterdiğiyle tutarlı); bizim mevcut satırlarımızdaki düşürülmüş şartlara dokunulmaz.
- NPC kimlikleri 24438, 24439, 24440 **aktarılmaz** (bizim zone 64 bekçileri; E §4.1 çakışma).
- Yeni canavarların düşürme tablosu (`K_MONSTER_ITEM`) bu planda yok.

## 2. Bağlam (okunması zorunlu)

- `docs/reports/u0-1534/E-veri-farki.md` §3 (eşya modeli, sayılar, ItemClass kuralı §3.4), §4.1 (NPC/canavar farkları, çakışmalar), §8 (betik kuralları).
- `docs/reports/u0-1534/C-db-semasi.md` (ITEM/K_NPC/K_MONSTER şema farkları; ALPHA `ITEM.ItemClass` tamamen NULL).
- `tools/kotbl.py` (U2-01): `item_org_us.tbl` (kolon 36 = eşya derecesi), `Item_Ext_<n>_us.tbl`, `Npc_us.tbl`, `Mob_us.tbl`.
- Betik kalıbı: `db/012_u2_capes_1534.sql` (+ rollback), `db/README.md`.
- Sunucu yükleyicileri (kolon listeleri): `shared/database/ItemTableSet.h`, `shared/database/NpcTableSet.h` (K_NPC ve K_MONSTER aynı set ile yüklenir; kontrol et), `GameServer/LoadServerData.cpp`, `AIServer/` NPC yüklemesi.

## 3. Kapsam

**Var:**
1. `tools/u2-gen-alpha.py` (standart kütüphane; ALPHA ve bizim DB'yi `SQLCMD.EXE` ile **salt okur**, istemci tablolarını `kotbl` ile okur):
   - Alt komut `items`: bizim `ITEM` kolon listesini `INFORMATION_SCHEMA`'dan alır; ALPHA `ITEM`'den aynı adlı kolonları seçer; kapsam = istemcinin çözebildiği (`item_org` tabanı + `Item_Ext` varyantı; E §3.1) **ve** bizde olmayan kimlikler. `ItemClass` ve aksesuar `ItemExt` değerlerini E §3.4 kuralıyla **üreticide** hesaplar (kural betik başlığına yazılır). Bizim tabloda olup ALPHA'da olmayan NOT NULL kolon varsa açık hata.
   - Alt komut `npcs`: K_NPC (istemci `Npc_us` kimlikleri, eksi 24438/24439/24440) ve K_MONSTER (istemci `Mob_us` kimlikleri) için aynı yaklaşım; `strName` istemci tablosundan (ASCII'ye çevrilemeyen karakter varsa ALPHA adı; o da değilse `Npc <id>`).
   - `--check`: betikleri yeniden üretip depodakiyle bayt bayt karşılaştırır.
   - Çıktı deterministik (kimliğe göre sıralı, sabit sayı biçimi).
2. `db/013_u2_items_1534.sql` (+ `_rollback.sql`): `sqlcmd` değişkeni `Target` (gerçek kullanımda `ITEM`); 1000'lik `INSERT … VALUES` blokları yerine **önce `#u2_items` geçici tablosuna** blok blok yükle, sonra tek `INSERT … SELECT … WHERE NOT EXISTS (Num)`; eklenen kimlikleri `dbo.$(Target)_U2_ADDED (Num int PRIMARY KEY)` tablosuna yaz; `SET XACT_ABORT ON`; sonunda `inserted=… already_present=…`. Rollback yalnız kayıt tablosundakileri siler ve kayıt tablosunu düşürür.
3. `db/014_u2_npc_monster_1534.sql` (+ `_rollback.sql`): değişkenler `NpcTarget` (gerçekte `K_NPC`) ve `MonTarget` (gerçekte `K_MONSTER`); aynı desen, kayıt tabloları `$(NpcTarget)_U2_ADDED`, `$(MonTarget)_U2_ADDED`.
4. `db/README.md`: `## 013`, `## 014` bölümleri (sunucu kapalıyken çalıştırma şartı yok; tablolar açılışta yüklenir — etki için yeniden başlatma).

**Yok:** canlı tablolara uygulama (Claude yapar), `K_NPCPOS`/Moradon yerleşimi (U3), görevler (U3), `K_MONSTER_ITEM`, `ITEM_EXCHANGE`, mevcut satır güncellemesi.

## 4. Dokunulabilecek dosyalar

| Dosya | Değişiklik |
|---|---|
| `tools/u2-gen-alpha.py` | YENİ |
| `db/013_u2_items_1534.sql`, `db/013_u2_items_1534_rollback.sql` | YENİ (üretilmiş) |
| `db/014_u2_npc_monster_1534.sql`, `db/014_u2_npc_monster_1534_rollback.sql` | YENİ (üretilmiş) |
| `db/README.md` | `## 013`, `## 014` |
| `tools/kotbl.py` | yalnız hata düzeltmesi gerekirse (raporla) |

## 5. Uygulama adımları

1. Üreticiyi yaz; `items` ve `npcs` çıktılarını üret; `--check` 0.
2. Sayıları E raporuyla karşılaştır: eşya ≈ 35.860 (fark varsa nedenini açıkla); K_NPC 79; K_MONSTER 60. `ItemClass` dağılımı E §3.4 tahminiyle (0: 15.644; 1: 2.558; 2: 4.305; 3: 8.292; 4: 4.742; 8: 320) karşılaştırılır.
3. Kural doğrulaması: §3.4 ItemClass kuralını **bizim mevcut ITEM satırlarımıza** uygulayıp eşleşme oranını yeniden ölç (E: %99,53); rapora yaz.
4. **Geçici kopya tablolarda test** (`.\SQLEXPRESS`, `FDP_kn_online`, `-E`): `ITEM_U202TEST`, `K_NPC_U202TEST`, `K_MONSTER_U202TEST` (`SELECT * INTO`); uygula → beklenen `inserted`; ikinci uygulama `inserted=0`; geri al → `EXCEPT` ile orijinalle aynı (0 satır); kopyaları ve kayıt tablolarını düşür. Bot eşyaları (`db/007_bot_gear.sql` kimlikleri) uygulama öncesi/sonrası kopyada değişmemiş (`EXCEPT` 0).
5. Bellek etkisi notu: kopyada uygulama sonrası `ITEM` satır sayısı ve tahmini artış rapora.

## 6. Kabul kriterleri

- [ ] K1: `python3 tools/u2-gen-alpha.py --check` → 0.
- [ ] K2: §5.2 sayıları raporda; E raporundan sapma açıklanmış.
- [ ] K3: §5.3 kural eşleşme oranı ≥ %99,5.
- [ ] K4: §5.4 geçici kopya testi: ilk uygulama beklenen sayılar, ikinci `inserted=0`, geri alma `EXCEPT` 0, bot eşyaları değişmedi, geçici tablo kalmadı.
- [ ] K5: 24438, 24439, 24440 betikte **yok** (`grep` ile göster); ALPHA'nın istemcide olmayan K_NPC/K_MONSTER kimlikleri betikte yok.
- [ ] K6: SQL ASCII, LF; yorumlar İngilizce; betik başlığında kaynak, kural ve kapsam yazılı.
- [ ] K7: `git diff --stat main...bot/U2-02` yalnız §4 dosyaları; `git status --short` temiz.

## 7. Doğrulama komutları

```bash
python3 tools/u2-gen-alpha.py --check
grep -c "24438\|24439\|24440" db/014_u2_npc_monster_1534.sql
ls -la db/013_u2_items_1534.sql db/014_u2_npc_monster_1534.sql
git diff --stat main...bot/U2-02
git status --short
```

## 8. Kısıtlar ve uyarılar

- ALPHA DB ve bizim DB yalnız `SELECT` ile okunur; canlı `ITEM`/`K_NPC`/`K_MONSTER`'a yazma yok.
- Kişisel veri tabloları okunmaz (`AGENTS.md` §2.7).
- İndirilen paketteki exe/dll çalıştırılmaz.
- `db/013` büyük olacak (~15–20 MB); tek dosya olarak commit edilir (üretici deterministik, `--check` ile yeniden üretilebilir).
- Git: `AGENTS.md` §2.8; commit `[U2-02] ...`.

---

## Uygulayıcı Raporu

(Uygulayıcı doldurur.)

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(Henüz yok.)
