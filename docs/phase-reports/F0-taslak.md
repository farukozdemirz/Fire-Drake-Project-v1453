# Faz Sonuç Raporu — F0 Ortam ve temel doğrulama (TASLAK)

Tarih: 2026-10-02 · Hazırlayan: Claude · Onaylayan: proje sahibi (onay tarihi: bekleniyor)
Değerlendirilen commit: `5d66182` (`main`; F0-02 birleştirmesi `c5c1908`, F0-01 birleştirmesi `43d3500`) · Sunucu commit: `0f52027` · DB özeti: yerel `FDP_kn_online`; `MAGIC.Etc` düzeltmesi elle uygulanmış, `MAGIC_BAK_etc` yedeği var (KI-001, kalıcı betik F1'de)

> Bu bir taslaktır. Faz durumu `KABUL_EDILDI` ancak proje sahibinin onayıyla yazılır (`docs/21` §1). Onaylanana kadar F0 "GELIŞTIRILDI, kabul bekliyor" durumundadır.

## 1. Amaç (`docs/17` §2'den)

Depo, veri ve istemciyle tekrar üretilebilir çalışan bir test sunucusu; mevcut sorunların listesi.

## 2. Teslim edilen kapsam

| İş kalemi | Durum | Commit(ler) | Not |
|---|---|---|---|
| Görev 1: Temiz kurulum betiği/notu | GELIŞTIRILDI | `81e754e` | `tools/check-env.sh` (22 kontrol: araç zinciri, depo, çalışma klasörü, DB (yalnızca SELECT), 32-bit ODBC DSN'leri). Kurulum notu: `start.md` (depoda izlenmiyor) |
| Görev 2: Release ve Debug derleme | GELIŞTIRILDI | `tools/build.sh` (mevcut), `90e097e` | İkisi de WSL'den derleniyor (2026-10-01); `tools/debug-release-diff.sh` farkları raporluyor |
| Görev 3: T-ENV-01 | TEST_EDILDI | `3314544`, `9ae6a87` | Üç sunucuyu `tools/run-servers.sh start\|stop\|status` ile çalıştırma; insan istemcisiyle Ronark Land girişi (aşağıda §4) |
| Görev 3: T-ENV-02 | TEST_EDILDI | `90e097e` | Debug/Release fark tablosu → `docs/02` §2.1 |
| Görev 4: `STATUS.md`, `KNOWN_ISSUES.md` başlat | GELIŞTIRILDI | `aeea40c` | Her plan doğrulamasında güncellendi |
| Görev 5: PR #10 alınmaz | KARARLAŞTIRILDI | — | K-10, ADR-0013; depoya hiçbir şey alınmadı |
| Plan akışı ve otonom döngü altyapısı | GELIŞTIRILDI | `aeea40c` | `plans/`, `AGENTS.md`, `CLAUDE.md`, `opencode.json`, skill'ler, `tools/auto-loop.sh` (faz kapsamı dışı, destekleyici) |

Planlar: F0-01 (KAPANDI, 10/10 kriter), F0-02 (KAPANDI, Tur 2'de 14/14 kriter). Ayrıntı: `plans/README.md`.

## 3. Kapsam dışında kalanlar / ertelenenler

- KI-001 kalıcı SQL betiği (ADR-0003) → F1.
- Bot kodu → F2 ve sonrası. (F0 "kod değişikliği yok" kuralı: F0 planlarında yalnızca `tools/` altında araç betikleri eklendi; `GameServer/`, `AIServer/`, `shared/` değişmedi.)
- F1-01 (paket izleyici, `GameServer/` değişikliği) F0 kabulünden **önce**, proje sahibinin kararıyla yapıldı ve KAPANDI. Bu F1'in kapsamındadır; F0 raporunda yalnızca sıra sapması olarak not edilir.
- Arena doğrulamaları (T-ENV-ARENA-01..04) → F1.

## 4. Çalıştırılan testler

| Test kimliği | Tekrar | Sonuç | Kanıt kaydı |
|---|---|---|---|
| T-ENV-01 (derleme/DB/ini/ODBC) | `check-env.sh` 1 koşu | GEÇTİ, 22/22 | `plans/F0-01-ortam-dogrulama-araclari.md` Doğrulama Tur 1 |
| T-ENV-01 (üç sunucunun başlaması) | F0-02 doğrulamasında start/stop/status birkaç kez | GEÇTİ: üç sunucu ayakta, GameServer↔AIServer bağlı, her biri 1–5 sn'de hazır, `start` ~13–17 sn, `stop` ~9 sn | `plans/F0-02-sunucu-calistirma-betigi.md` Doğrulama Tur 2; `docs/15` T-ENV-01 |
| T-ENV-01 (istemciyle Ronark Land'e giriş) | 1 | GEÇTİ, sorun yok. **Kanıt zayıf `[Ö]`:** proje sahibinin beyanı (2026-10-02); ekran görüntüsü veya saat notu yok | `docs/15` T-ENV-01, `docs/STATUS.md` |
| T-ENV-02 (Debug/Release farkı) | 1 | GEÇTİ | `docs/02` §2.1; `plans/F0-01-…` |
| F0-02 başarısızlık yolu (zaman aşımı, `pause`) | geçici betik kopyasıyla | GEÇTİ (mesaj ve temizlik doğru) | F0-02 Doğrulama Tur 2 |

## 5. Kabul kriterleri

F0'ın kabul kriterleri `docs/17` §2'de AC kimliği olmadan sözle tanımlı; ölçüm olmadığı için güven aralığı uygulanmaz.

| AC kimliği | Kriter | Ölçülen değer | GA (%95) | Karşılandı mı |
|---|---|---|---|---|
| (F0 kabul-1) | Üç sunucu ayakta | AIServer, GameServer, LogInServer `UP`; GameServer↔AIServer bağlı | — | Evet |
| (F0 kabul-2) | İnsan istemcisi Ronark'a giriyor | Giriş yapıldı, sorun yok (beyan, kanıt zayıf) | — | Evet (kanıt notuyla) |
| (F0 kabul-3) | Release/Debug fark tablosu var | `docs/02` §2.1 | — | Evet |

## 6. Sapmalar ve açıklamaları

1. **Sıra sapması:** F1-01 planı ve uygulaması F0 kabulünden önce yapıldı (proje sahibi kararı, 2026-10-02). Etki: yok; F1-01 bayrakla kapalı ve davranış değiştirmiyor.
2. **Kod değişikliği yasağı:** F0 planlarında yalnızca `tools/` betikleri eklendi, üretim kodu değişmedi (§3).
3. **T-ENV-01 kanıtı:** Ronark girişi için ekran görüntüsü/saat notu alınmadı (§4). İstenirse F1-02 oturumunda (aynı istemci girişi) log ve ekran görüntüsüyle yeniden kayda geçirilebilir.

## 7. Yeni bilinen sorunlar (KNOWN_ISSUES kimlikleri)

- KI-007: Elle açılmış sunucular nazikçe kapanmıyor; `stop` zorla kapatıyor (düşük).
- KI-008: `stop` satırı hep `0 sn` yazıyor (düşük, kozmetik).
- KI-009 (F1-01'de bulundu): `.py` satır sonu `.gitattributes` ile sabit değil (düşük).
- F0 başında kayıt altına alınan KI-001..006 `docs/KNOWN_ISSUES.md`'dedir.

## 8. Bu fazda alınan kararlar (ADR kimlikleri)

K-1..K-10 (ADR-0001..0004, ADR-0009..0013), 2026-10-01. Bu fazda yeni ADR yok. Açık: ADR-0005..0008 (ilgili fazlarda).

## 9. Doküman güncellemeleri

| Dosya | Bölüm | Özet |
|---|---|---|
| `docs/02` | §2.1 | Debug/Release farkları tablosu (F0-01) |
| `docs/02` | §3, §16 | `tools/run-servers.sh` çalıştırma komutu ve ölçülen süreler (v1.1) |
| `docs/15` | T-ENV-01/02 | Kanıt ve sonuçlar |
| `docs/STATUS.md`, `docs/KNOWN_ISSUES.md`, `plans/README.md` | — | Güncel durum |
| `docs/20` | §5 REQ-NEW-15 | Durum güncellendi (F0-01 `check-env.sh`) |

## 10. Çıkış kararı

- [x] Tüm çıkış koşulları karşılandı (Kabul: yukarıdaki üç madde; faz raporu bu belge) → onay verilirse `KABUL_EDILDI`
- [ ] Kısmi → `TEST_EDILDI` olarak kalır; eksikler: —

Faz kapısı denetim listesi (`docs/17` §4): iş kalemleri GELIŞTIRILDI ✔ · testler ve kanıt kayıtları ✔ (T-ENV-01 Ronark kanıtı zayıf, §6) · kabul kriterleri ölçülen değerle eşlendi ✔ · yeni sorunlar KNOWN_ISSUES'te ✔ · dokümanlar güncel ✔ · `docs/20` güncellendi ✔ · geri alma yolu: yok (yapılandırma/araç) ✔ · faz raporu yazıldı ✔ (onay bekliyor).

## 11. Geri alma bilgisi

F0 yalnızca araç betikleri ve dokümanlar ekledi; üretim kodu değişmedi. Geri almak için: `git revert -m 1 c5c1908 43d3500` (F0-02 ve F0-01 birleştirmeleri). Geri alma gerekmesi beklenmez.
