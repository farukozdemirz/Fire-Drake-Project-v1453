# U3-01: Yeni Moradon SMD üreteci (`tools/u3-moradon-smd.py`) ve Moradon'a giren warp yaması

| Alan | Değer |
|---|---|
| Durum | UYGULANIYOR |
| Faz | U3 — Sürüm yükseltme 1534, yeni Moradon (`docs/17` §2 U, ADR-0068 Ek 2) |
| Branch | `bot/U3-01` (taban: `main`) |
| Bağımlı olduğu planlar | — |
| İlgili gereksinim / kabul | T-UPG-02 hazırlığı; `docs/reports/u0-1534/G-yeni-moradon-smd.md` §7 (V1–V10) |
| Tahmini büyüklük | M (1 araç + `docs/appendix/tools` kullanımı) |
| Hazırlayan / tarih | Claude / 2026-10-08 |

---

## 1. Amaç

Yeni Moradon (zone 21) için sunucu harita dosyasını (SMD) **1534 istemcisinin kendi dosyalarından** üreten, Moradon'a giren warp'ları diğer haritalarda yamalayan ve çıktıyı çevrimdışı doğrulayan deterministik bir araç. Yöntem G raporunda kanıtlı bir prototiple doğrulandı (`/tmp/claude-1000/-mnt-c-Users-frkoz-OneDrive-Desktop-Fire-Drake-Project-v1453/fbfeef86-7e71-4d83-9948-3d869babf2db/scratchpad/k1534/g/u3_moradon_smd.py`; prototip referanstır, depo kurallarına göre yeniden yaz). Üretilen SMD'ler **depoya girmez** (istemciden türetilmiş tescilli veri); dağıtım Claude'un işidir.

## 2. Bağlam (okunması zorunlu)

- `docs/reports/u0-1534/G-yeni-moradon-smd.md` tamamı (özellikle §1 düzenler, §2 yükseklik, §3 çarpışma, §4 yürünebilirlik kuralı R ve doğrulamalar, §5 warp'lar, §7 araç tasarımı ve V1–V10, §8 çalışma zamanı denetimleri).
- `docs/adr/ADR-0068-*.md` Ek 2.
- Sunucu okuyucu: `shared/SMDFile.cpp:16-180`, `N3BASE/N3ShapeMgr.cpp:52-118`, `shared/database/structs.h:212-240` (`_WARP_INFO`).
- Depodaki ayrıştırıcılar: `docs/appendix/tools/smd_parse.py`, `smd_analyze.py`, `tools/nav-export.py` (zone 71 parmak izi/crc32).
- Girdiler (salt okunur): istemci `/mnt/c/dev/fdp1534/client/Knight Online/Zones/moradon.gtd`, `.opd`, `.opdext`; bağışçı `/mnt/c/dev/fdp1534/alpha/ALPHA KO 1534 PROJE/Server-Files/Map/moradon_0826.smd`; bizim haritalar `/mnt/c/dev/fdp/server/Map/*.smd`; ALPHA DB `.\SQL2019` → `FDP_alpha1534` (yalnız `SELECT`, `K_NPCPOS`/`START_POSITION` doğrulama noktaları için).

## 3. Kapsam

**Var:** `tools/u3-moradon-smd.py` (yalnız standart kütüphane; `python3 -I` ile çalışır):
- `build --client-zones DIR --donor SMD [--warps SPEC.json] --out SMD`: G §7 adımları 1–8 (yükseklik `.gtd`'den transpozsuz; çarpışma `.opd` bloğu bayt bayt; kural R ile olay ızgarası; ALPHA nesne olayı bloğu; regene 0; warp'lar ALPHA'dan zone 73 hariç; yan JSON: girdi/çıktı md5 ve sayılar). Warp ücretleri: **bizim** değerler (ADR-0068 Ek 2 madde 3) — bizim eski Moradon SMD'sindeki aynı kimlikli warp'ların ücreti kullanılır; karşılığı yoksa ALPHA ücreti ve raporda listelenir.
- `patch-inbound --map-dir DIR --files LIST --x 817 --z 530 --out-dir DIR`: `sZone == 21` warp kayıtlarında yalnız `fX/fZ` (8 bayt) değişir; diğer her bayt aynı.
- `verify --smd SMD [--points JSON] [--spawns FILE]`: V1–V10 (G §7 tablosu); başarısızlıkta çıkış ≠ 0.
- `--selftest`: kural R'yi depodaki/yerel referans SMD'lerde (eski Moradon, 71, 72) %100; istemci→ızgara uçtan uca ≥ %99,99 (71, 72); dosyalar yoksa ilgili alt test `SKIP` yazar ama sentetik birim testleri (başlık ayrıştırma, kural R, yama baytları) her zaman koşar.

**Yok:** DB değişiklikleri (U3-02), Lua (U3-03), dağıtım, sunucu kodu, depoya SMD eklemek.

## 4. Dokunulabilecek dosyalar

| Dosya | Değişiklik |
|---|---|
| `tools/u3-moradon-smd.py` | YENİ |
| `docs/appendix/tools/smd_parse.py` | yalnız hata düzeltmesi gerekirse (raporla) |

## 5. Uygulama adımları

1. Aracı yaz; `--selftest`.
2. `build` → çıktıyı depo **dışına**, `/tmp/claude-1000/-mnt-c-Users-frkoz-OneDrive-Desktop-Fire-Drake-Project-v1453/fbfeef86-7e71-4d83-9948-3d869babf2db/scratchpad/u3/out/moradon_1534.smd` yaz.
3. `patch-inbound` → 6 dosya (`karus_051221.smd`, `elmo_051221.smd`, `siege_0722.smd`, `freezone_a_20050718.smd`, `freezone_b_20050718.smd`, `In_dungeon_20050718.smd`; adları `/mnt/c/dev/fdp/server/Map`'te doğrula) → aynı çıktı dizini. Kaynak harita dizinine **yazma**.
4. `verify` → V1–V10 tablosu (beklenen değerler G §7).
5. Zone 71 parmak izi: yamalı `freezone_a_20050718.smd` için `tools/nav-export.py` crc32'si yamasız ile aynı.

## 6. Kabul kriterleri

- [ ] K1: `python3 -I tools/u3-moradon-smd.py --selftest` → OK.
- [ ] K2: `build` çıktısı 2.708.222 bayt ± (320 × warp sayısı farkı); yan JSON üretildi; aynı girdilerle ikinci `build` bayt bayt aynı (determinizm).
- [ ] K3: V1–V10 hepsi geçer (çıktı tablosu raporda, G §7 beklenenleriyle yan yana).
- [ ] K4: `patch-inbound` farkı yalnız `sZone==21` kayıtlarında 8 bayt (bayt düzeyinde fark dökümü); zone 71 crc32 aynı.
- [ ] K5: Depoya SMD/ikili eklenmedi; `git diff --stat main...bot/U3-01` yalnız §4.
- [ ] K6: Python LF, ASCII; `git status --short` temiz.

## 7. Doğrulama komutları

```bash
python3 -I tools/u3-moradon-smd.py --selftest
python3 -I tools/u3-moradon-smd.py build --client-zones "/mnt/c/dev/fdp1534/client/Knight Online/Zones" --donor "/mnt/c/dev/fdp1534/alpha/ALPHA KO 1534 PROJE/Server-Files/Map/moradon_0826.smd" --out <scratchpad>/u3/out/moradon_1534.smd
python3 -I tools/u3-moradon-smd.py verify --smd <scratchpad>/u3/out/moradon_1534.smd
git diff --stat main...bot/U3-01
git status --short
```

## 8. Kısıtlar ve uyarılar

- İndirilen paketteki exe/dll çalıştırılmaz; istemci ve ALPHA dosyaları yalnız okunur.
- `/mnt/c/dev/fdp/server/Map` ve `/mnt/c/dev/fdp1534*/server/Map` dizinlerine **yazma** (dağıtım Claude'un).
- DB yalnız `SELECT`; kişisel veri tabloları okunmaz.
- Git: `AGENTS.md` §2.8; commit `[U3-01] ...`.

---

## Uygulayıcı Raporu

(Uygulayıcı doldurur.)

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(Henüz yok.)
