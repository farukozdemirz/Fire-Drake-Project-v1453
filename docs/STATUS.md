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

F0 — Ortam ve temel doğrulama — Durum: GELIŞTIRILIYOR (F0-01 KAPANDI; F0-02 HAZIR: sunucu çalıştırma betiği. F0 çıkışı için ayrıca T-ENV-01'in istemciyle Ronark'a giriş kısmı elle yapılmalı)

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
| F0-01 Ortam doğrulama araçları | KAPANDI | `main` @ `43d3500` (merge) |
| F0-02 Sunucu çalıştırma betiği (`tools/run-servers.sh`) | HAZIR | `bot/F0-02` (taban: `main`); `plans/.aktif-plan` bu plana işaret ediyor |

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
| T-ENV-01 kalanı: insan istemcisiyle Ronark Land'e giriş | F0 çıkış koşulu; DeepSeek/Claude yapamaz (istemci GUI) | Proje sahibi | Sunucular `tools/run-servers.sh start` ile (F0-02 sonrası) veya elle açıkken `C:\dev\fdp\Client\KnightOnline.exe` ile giriş, Ronark'a geçiş; kanıt: ekran görüntüsü veya tarih/saat notu → `docs/15` T-ENV-01 |

## Sıradaki 3 adım

1. DeepSeek: `plans/F0-02-sunucu-calistirma-betigi.md` uygular; Claude `/plan-dogrula` ile doğrular. Doğrulama sonrası Claude: `docs/02` §3'e çalıştırma komutunu, `docs/15` T-ENV-01'e "üç sunucu ayakta" kanıtını ve ölçülen açılış sürelerini işler.
2. Proje sahibi: istemciyle Ronark Land'e giriş (T-ENV-01 kalanı, Blokajlar). Sonucu `docs/STATUS.md`'ye bir satır olarak yazması yeterli.
3. F0 faz sonuç raporu (`docs/templates/PHASE_REPORT.md` → `docs/phase-reports/F0-taslak.md`) ve proje sahibinin F0 kabulü; ardından F1 planları.

## Otonom döngü

2026-10-01 19:00'da başlatıldı (`plans/_logs/auto-loop.log`); ilk iterasyonda F0-02 planı yazıldı. Tasarım: `plans/OTONOM_DONGU.md`, `tools/auto-loop.sh`. `--run` ön kontrolü: altyapı dosyaları commit'li + çalışma ağacı temiz. Varsayılanlar: `MAX_ITERATIONS=5`, faz sınırında dur, `claude` modeli `opus`, `opencode` modeli `opencode-go/deepseek-v4.1-flash`.

## Devralan için notlar

- Araştırma sırasında sunucu çalıştırılmadı. Tüm mekanik iddialar kod/veri okumasına dayanır; çalışma zamanı doğrulaması F1'in işi.
- 2026-10-01: üç sunucu proje sahibi tarafından 13:09–13:10'da `C:\dev\fdp\server`'dan elle başlatılmış ve çalışıyor. Claude ~19:00'da gözledi: portlar 10020/15001/15100 LISTEN, GameServer→AIServer bağlantısı kurulu, bağlı istemci yok `[V]`. Aynı makinede yolu okunamayan ilgisiz bir `GameServer.exe` süreci daha var; betikler süreçleri adla değil exe yoluyla tanımalı (F0-02 §5.2).
- `tools/build.sh` ve `tools/auto-loop.sh` git'te `100644` modunda (çalıştırılabilir değil). WSL/drvfs'te sorun çıkarmıyor; temiz bir klonda `bash tools/build.sh` gerekir. Küçük bir düzeltme planına veya proje sahibinin `git update-index --chmod=+x` commit'ine bırakıldı.
- Depo dosyaları CRLF + tab. Bazı dosyalar ISO-8859 (Korece yorumlu) veya UTF-8 BOM'lu; kodlama korunmalı (`AGENTS.md` §3).
- `.sh` dosyaları `.gitattributes` ile LF'e sabitlendi (`core.autocrlf=true` betikleri bozmasın diye).
