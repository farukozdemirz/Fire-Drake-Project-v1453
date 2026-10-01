# Bot Projesi — Güncel Durum

Son güncelleme: 2026-10-01 · Güncelleyen: Claude
Sunucu commit: `0f52027` (upstream ile aynı) · Bot kodu: henüz yok · Doküman paketi: v1.0
Veritabanı: `FDP_kn_online` (yerel; MAGIC.Etc düzeltmesi elle uygulanmış, `MAGIC_BAK_etc` yedeği var)

## Çalışma düzeni

- **Planlayıcı ve denetçi:** Claude (Claude Code), `/plan-olustur` ve `/plan-dogrula`.
- **Uygulayıcı:** DeepSeek v4.1 Flash, opencode üzerinden (`AGENTS.md`, `opencode.json`).
- Planlar `plans/` altında; akış `plans/README.md`'de.
- Derleme: `./tools/build.sh Release` ve `Debug`. 2026-10-01'de ikisi de WSL'den başarıyla derlendi (Debug tam derleme ~21 sn).

## Aktif faz

F0 — Ortam ve temel doğrulama — Durum: GELIŞTIRILIYOR (F0-01 DOĞRULANDI; F0 çıkışı için T-ENV-01'in sunucu başlatma + istemciyle Ronark'a giriş kısmı açık)

## Faz tablosu

| Faz | Durum | Son rapor | Kabul commit |
|---|---|---|---|
| F0 Ortam | GELIŞTIRILIYOR | — | — |
| F1 Veri ve mekanik doğrulama | PLANLANDI | — | — |
| F2 Bot oturumu | PLANLANDI | — | — |
| F3 Telemetri ve test altyapısı | PLANLANDI | — | — |
| F4 Aksiyon ve adalet | PLANLANDI | — | — |
| F5 Navigasyon | PLANLANDI | — | — |
| F6 Sınıf davranışları ve solo | PLANLANDI | — | — |
| F7 Party koordinasyonu | PLANLANDI | — | — |
| F8 Değerlendirme ve 8v8 | PLANLANDI | — | — |
| F9 L1 öğrenme | PLANLANDI | — | — |
| F10 L2 bandit (opsiyonel) | PLANLANDI | — | — |

## Planlar

Liste: `plans/README.md`.

| Plan | Durum | Not |
|---|---|---|
| F0-01 Ortam doğrulama araçları | DOĞRULANDI | `bot/F0-01` @ `c710379`; birleştirme bekliyor (kullanıcı onayı) |

## Son doğrulamalar

| Tarih | Plan | Karar | Not |
|---|---|---|---|
| 2026-10-01 | F0-01 | DOĞRULANDI | 10/10 kriter; 2 düşük + 3 bilgi bulgusu (`plans/F0-01…` Doğrulama Raporu). Debug/Release tablosu → `docs/02` §2.1 |

## Verilen kararlar

K-1..K-10, 2026-10-01 (`docs/18` §1, `docs/adr/`). Önerilenden farklı seçilenler:

- K-5: tüketilmeyen potlar olduğu gibi kalır.
- K-6: yalnızca arena A.
- K-9: botlar sıralama/ödül/duyurulara tamamen dahil.
- K-10: PR #10 alınmaz.

Açık teknik kararlar: ADR-0005..0008, ilgili fazda verilecek.

## Blokajlar

| Konu | Etki | Sahibi | Not |
|---|---|---|---|
| `docs/`, `plans/`, `AGENTS.md`, `CLAUDE.md`, `opencode.json`, `.claude/`, `tools/build.sh`, `tools/auto-loop.sh`, `.gitattributes` hâlâ hiçbir branch'te commit'li değil | Plan branch'leri `main`'den açılınca bu dosyaları görmez; F0-01 yalnızca ortak çalışma klasörü sayesinde çalıştı | Proje sahibi | `main`'e commit gerekir; sonra `bot/F0-01` merge edilebilir |

## Sıradaki 3 adım

1. Proje sahibi: altyapı dosyalarını (`docs/`, `plans/`, `AGENTS.md`, `CLAUDE.md`, `opencode.json`, `.claude/`, `tools/`, `.gitattributes`) `main`'e commit eder; opencode'da DeepSeek hesabını bağlar.
2. ~~F0-01 uygulama ve doğrulama~~ — yapıldı.
3. F0'ı bitirmek için: sunucuları başlatma (AIServer→GameServer→LogInServer) ve insan istemcisiyle Ronark'a giriş (T-ENV-01 kalanı). Claude bunun için bir plan yazabilir (`tools/run-servers.sh` benzeri başlatma betiği + elle yapılacak giriş adımı); ardından faz sonuç raporu.

## Otonom döngü

Hazır, başlatılmayı bekliyor: `plans/OTONOM_DONGU.md`, `tools/auto-loop.sh`. `--run` ön kontrolü: altyapı dosyaları commit'li + çalışma ağacı temiz. Varsayılanlar: `MAX_ITERATIONS=5`, faz sınırında dur, `claude` modeli `opus`, `opencode` modeli `opencode-go/deepseek-v4.1-flash`.

## Devralan için notlar

- Araştırma sırasında sunucu çalıştırılmadı. Tüm mekanik iddialar kod/veri okumasına dayanır; çalışma zamanı doğrulaması F1'in işi.
- Depo dosyaları CRLF + tab. Bazı dosyalar ISO-8859 (Korece yorumlu) veya UTF-8 BOM'lu; kodlama korunmalı (`AGENTS.md` §3).
- `.sh` dosyaları `.gitattributes` ile LF'e sabitlendi (`core.autocrlf=true` betikleri bozmasın diye).
