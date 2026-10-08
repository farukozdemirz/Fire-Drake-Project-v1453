# U3-02: Yeni Moradon DB betiği (`db/015_u3_moradon_1534.sql`): ZONE_INFO, START_POSITION, K_OBJECTPOS, K_NPCPOS (zone 21)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | U3 — Sürüm yükseltme 1534, yeni Moradon (`docs/17` §2 U, ADR-0068 Ek 2) |
| Branch | `bot/U3-02` (taban: `main`) |
| Bağımlı olduğu planlar | U2-02 (KAPANDI; zone 21 NPC/canavar kimlikleri), U3-01 (SMD dosya adı `moradon_1534.smd`; paralel yazılabilir) |
| İlgili gereksinim / kabul | T-UPG-02; `docs/reports/u0-1534/G-yeni-moradon-smd.md` §5, §7 "DB / scripts", `E-veri-farki.md` §4.3 |
| Tahmini büyüklük | S–M (1 üretici, 2 SQL, README) |
| Hazırlayan / tarih | Claude / 2026-10-08 |

---

## 1. Amaç

Yeni Moradon'un DB tarafını tek, geri alınabilir betikte toplamak: zone 21'in harita dosyası adı ve başlangıç noktaları, kapı/örs nesneleri ve NPC/canavar yerleşimi. Betik **ayrı 1534 DB kopyasına** uygulanacak (ADR-0068 Ek 2 madde 4; canlı `FDP_kn_online`'a değil — eski 1453 istemcisinin Moradon'u bozulmasın). Uygulama Claude'un işidir.

## 2. Bağlam (okunması zorunlu)

- G raporu §5 (warp grupları, nesneler `K_OBJECTPOS` 4013/4014/5001, ALPHA futbol nesneleri 1019–1022 **alınmaz**), §7 sonundaki "DB / scripts" listesi, §8 denetimleri.
- E raporu §4.3 (zone 21 yerleşimi ALPHA vs bizim; istemci minimap kanıtı).
- Betik kalıbı: `db/013_u2_items_1534.sql`, `db/014_u2_npc_monster_1534.sql` (+ rollback), `tools/u2-gen-alpha.py` (ALPHA'yı `SQLCMD` ile okuyan deterministik üretici; `--check`).
- Sunucu yükleyicileri: `shared/database/ZoneInfoSet.h`, `StartPositionSet.h`, `ObjectPosSet.h`, `NpcPosSet.h`; AIServer NPC yerleşimi.
- ALPHA DB: `.\SQL2019` → `FDP_alpha1534` (`SELECT`). Bizim DB: `.\SQLEXPRESS` → `FDP_kn_online` (`SELECT`; test için kopya tablolar).

## 3. Kapsam

**Var:**
1. `tools/u3-gen-moradon-db.py` (veya `tools/u2-gen-alpha.py`'ye `moradon` alt komutu — hangisi daha az kopya üretiyorsa; raporda gerekçe): ALPHA'dan zone 21 `K_NPCPOS` satırları (kimlikleri bizim `K_NPC`/`K_MONSTER`'da **olmayan** satır varsa betik hatayla durur — U2-02 sonrası hepsi olmalı), `K_OBJECTPOS` zone 21 (futbol 1019–1022 hariç; nesne türü 50 efektler raporla), ve sabitler.
2. `db/015_u3_moradon_1534.sql` (+ `_rollback.sql`), `SET XACT_ABORT ON`, tek işlem:
   - Yedek tabloları: `ZONE_INFO_Z21_U3_BACKUP`, `START_POSITION_Z21_U3_BACKUP`, `K_OBJECTPOS_Z21_U3_BACKUP`, `K_NPCPOS_Z21_U3_BACKUP` (yalnız ilk çalıştırmada doldurulur; ikinci çalıştırma yedeğe dokunmaz).
   - `ZONE_INFO` 21: `strZoneName='moradon_1534.smd'`, `InitX/InitZ/InitY = 81590/53079/469`, `RoomEvent = 0`.
   - `START_POSITION` 21: her iki ulus 817/530, `bRangeX = bRangeZ = 10`, kapı kolonları G §7'ye göre (bugünkü yanlış yerleşim düzeltilir).
   - `K_OBJECTPOS` 21: bizim zone 21 satırları silinir, ALPHA'dan seçilenler eklenir.
   - `K_NPCPOS` 21: bizim zone 21 satırları (138) silinir, ALPHA'nın satırları (≈123) eklenir.
   - Sonunda sayılar (`zone_info=1 start=1 objpos=… npcpos=…`).
   - Yeniden çalıştırılabilir (aynı son durum).
3. Rollback: yedeklerden zone 21'i geri yazar ve yedek tablolarını düşürür; tekrar çalıştırılabilir.
4. `db/README.md`: `## 015` (yalnız 1534 DB'sine uygulanır uyarısıyla).

**Yok:** Lua (U3-03), `USERDATA` konum sıfırlaması (proje sahibi kararı), warp ücretleri (SMD içinde; U3-01), canlı DB'ye uygulama.

## 4. Dokunulabilecek dosyalar

| Dosya | Değişiklik |
|---|---|
| `tools/u3-gen-moradon-db.py` **veya** `tools/u2-gen-alpha.py` | üretici |
| `db/015_u3_moradon_1534.sql`, `db/015_u3_moradon_1534_rollback.sql` | YENİ (üretilmiş) |
| `db/README.md` | `## 015` |

## 5. Uygulama adımları

1. Üreticiyi yaz; betikleri üret; `--check` 0.
2. Bizim zone 21 satırları ile ALPHA'nınkileri tabloya koy (sayı, kimlik grupları; satır içeriği kişisel veri değil — referans tablolar).
3. **Test:** `FDP_smoke1534` duman DB'sinde (`.\SQLEXPRESS`, U2 verisi uygulanmış canlı kopya) uygula → sayılar; tekrar uygula → aynı son durum; geri al → `EXCEPT` ile 4 tablonun zone 21 satırları uygulama öncesiyle aynı; yeniden uygula (duman ortamı için bırak). **`FDP_kn_online`'a yazma.**
4. Doğrulama noktaları: yeni `START_POSITION` ve tüm `K_NPCPOS` merkezleri yeni haritanın sınırları içinde (0..1024).

## 6. Kabul kriterleri

- [ ] K1: Üretici `--check` 0; betik başlığında kaynak, kapsam ve sayılar.
- [ ] K2: §5.3 testi: sayılar, tekrar uygulama, geri alma `EXCEPT` 0, yeniden uygulama; `FDP_kn_online` dokunulmadı (önce/sonra zone 21 satır sayıları aynı).
- [ ] K3: Futbol nesneleri 1019–1022 betikte yok; zone 21 `K_NPCPOS` satırlarının tüm NPC/canavar kimlikleri bizim tablolarda mevcut.
- [ ] K4: SQL ASCII, LF; yorumlar İngilizce.
- [ ] K5: `git diff --stat main...bot/U3-02` yalnız §4; `git status --short` temiz.

## 7. Doğrulama komutları

```bash
python3 -I tools/<üretici> --check
SQLCMD="/mnt/c/Program Files/Microsoft SQL Server/Client SDK/ODBC/130/Tools/Binn/SQLCMD.EXE"
"$SQLCMD" -S '.\SQLEXPRESS' -E -d FDP_smoke1534 -b -i "$(wslpath -w db/015_u3_moradon_1534.sql)"
git diff --stat main...bot/U3-02
git status --short
```

## 8. Kısıtlar ve uyarılar

- Yazma yalnız `FDP_smoke1534`'e (test); canlı `FDP_kn_online`'a ve ALPHA DB'ye yazma yok. Kişisel veri tabloları okunmaz.
- İndirilen paketteki exe/dll çalıştırılmaz.
- Git: `AGENTS.md` §2.8; commit `[U3-02] ...`.

---

## Uygulayıcı Raporu

(Uygulayıcı doldurur.)

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(Henüz yok.)
