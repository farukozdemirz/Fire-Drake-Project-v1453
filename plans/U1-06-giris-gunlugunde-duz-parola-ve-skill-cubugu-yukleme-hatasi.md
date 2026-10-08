# U1-06: Giriş günlüğünde düz parola (KI-043) ve skill çubuğu yükleme hatası

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | U1 — Sürüm yükseltme 1534 (yan bulgular; iki profili de etkiler) |
| Branch | `bot/U1-06` (taban: `yukseltme/1534`) |
| Bağımlı olduğu planlar | — |
| İlgili gereksinim / kabul | KI-043; T-UPG-01 (skill çubuğu girişte yüklenir) |
| Tahmini büyüklük | XS (2 dosya, birkaç satır) |
| Hazırlayan / tarih | Claude / 2026-10-08 |

---

## 1. Amaç

İki 1453 hatası (profilden bağımsız, iki profilde de düzeltilir):

1. **KI-043:** `LogInServer/LoginSession.cpp` giriş günlüğü satırı (`:140`, `WriteUserLogFile`) biçim dizesi `ID=%s Authentication=%s` iki `%s` alırken argüman listesi `account, password, sAuthMessage`; bu yüzden `Authentication=` alanına **parola** yazılıyor. Konsoldaki `printf` (`:123`) satırını da kontrol et (aynı sorun varsa aynı düzeltme).
2. **Skill çubuğu:** `GameServer/DatabaseThread.cpp` `CUser::ReqSkillDataLoad` (`:292-299`): `//result << uint16(0);` yorum satırı yüzünden `if (!g_DBAgent.LoadSkillShortcut(result, this))` ifadesi `Send(&result);`'ı kapsıyor; kısayollar yalnız yükleme **başarısız** olunca gönderiliyor, başarıda hiç gönderilmiyor (`docs/reports/u0-1534/D-paket-duzeni-farki.md` §6 U1-08 "Ayrı KI önerisi").

## 2. Bağlam (okunması zorunlu)

- `LogInServer/LoginSession.cpp:115-142` (giriş yanıtı ve günlük satırları).
- `GameServer/DatabaseThread.cpp:286-299`, `GameServer/DBAgent.cpp` `LoadSkillShortcut` (başarısızlıkta `result`'a ne yazıyor? boş yanıtın biçimi ne olmalı: 1453 istemci ve 1534 istemci `WIZ_SKILLDATA` okuması D §2.3'te "değişiklik yok").
- `docs/KNOWN_ISSUES.md` KI-043.

## 3. Kapsam

**Var:**
1. Günlük satırlarında parola **hiçbir biçimde** yazılmaz; `ID=<hesap> Authentication=<sonuç>` olur.
2. `ReqSkillDataLoad`: başarıda kısayollar gönderilir; başarısızlıkta istemcinin beklediği boş yanıt (`uint16(0)` sayaç — `LoadSkillShortcut`'un yazdığı biçimle tutarlı; koddan doğrula) gönderilir. Her durumda tam olarak bir `Send`.

**Yok:** başka günlük biçimi değişikliği, mevcut günlük dosyalarının temizlenmesi (Claude ayrıca yapar), DB.

## 4. Dokunulabilecek dosyalar

| Dosya | Değişiklik |
|---|---|
| `LogInServer/LoginSession.cpp` | günlük satırları |
| `GameServer/DatabaseThread.cpp` | yalnız `ReqSkillDataLoad` |

## 5. Uygulama adımları

1. İki düzeltmeyi yap; `LoadSkillShortcut`'un başarı/başarısızlık çıktısını kodla açıkla (rapor).
2. Derle, testleri koş.

## 6. Kabul kriterleri

- [ ] K1: Release ve Debug hatasız, yeni uyarı yok.
- [ ] K2: `./tools/run-tests.sh Release` taban ile aynı, 0 başarısız.
- [ ] K3: `grep -n -a "password" LogInServer/LoginSession.cpp` çıktısında hiçbir günlük/printf argümanında `password` yok (göster).
- [ ] K4: `ReqSkillDataLoad` her yolda tam bir `Send` yapar (kod alıntısı).
- [ ] K5: `git diff --stat` yalnız §4; kodlama korunur; `git status --short` temiz.

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release
grep -n -a "password" LogInServer/LoginSession.cpp
git diff yukseltme/1534...bot/U1-06
git status --short
```

## 8. Kısıtlar ve uyarılar

- U1-05 aynı anda `DatabaseThread.cpp`'de ittifak oluşturucularına dokunuyor; **yalnız** `ReqSkillDataLoad`'ı değiştir.
- Git: `AGENTS.md` §2.8; commit `[U1-06] ...`.

---

## Uygulayıcı Raporu

(Uygulayıcı doldurur.)

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(Henüz yok.)
