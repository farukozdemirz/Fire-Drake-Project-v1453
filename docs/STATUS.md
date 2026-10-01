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

F0 — Ortam ve temel doğrulama — Durum: GELIŞTIRILIYOR → çıkış koşulları karşılandı, faz kabulü (proje sahibi) bekliyor. F0-01 ve F0-02 KAPANDI; T-ENV-01 tamam (istemciyle Ronark Land'e giriş 2026-10-02'de proje sahibi tarafından yapıldı, sorun yok). F1-01 (paket izleyici) F0 kabulünden önce yapıldı ve KAPANDI.

## Faz tablosu

| Faz | Durum | Son rapor | Kabul commit |
|---|---|---|---|
| 2026-10-02 | F1-02 | DOĞRULANDI (Tur 2) | 8/8 kriter ✔; Tur 1 bulgusu (iptal sonrası bayat CASTING) kapandı, ek kenar vakaları bağımsız doğrulandı. `prepare`/`finish` çalışma zamanı doğrulaması ilk gerçek oturumda |
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
| F0-02 Sunucu çalıştırma betiği (`tools/run-servers.sh`) | KAPANDI | `bot/F0-02` @ `9c16d5e` (+ Tur 2 doğrulama commit'i); `main`'e birleştirme ve push proje sahibinin onayında; `plans/.aktif-plan` bu plana işaret ediyor |
| F1-01 Paket izleyici (`FDP_PACKET_TRACE`) | KAPANDI | `bot/F1-01` @ `9197379` (taban: `bot/F0-02`); birleştirme sırası: önce F0-02, sonra F1-01; çalışma zamanı kaydı F1-02'de (insan istemcisi) |
| F1-02 Zamanlama oturumu araçları | KAPANDI | `bot/F1-02` (taban: `main`); `plans/F1-02-zamanlama-oturumu-araclari.md`; sonra insan oturumu `docs/15` §4.2.1 |

## Son doğrulamalar

| Tarih | Plan | Karar | Not |
|---|---|---|---|
| 2026-10-02 | F1-02 | DÜZELTME GEREKLİ (Tur 1) | 7/8 kriter ✔. Engelleyen: CAST süresi eşleştirmesi iptal sonrası bayat CASTING kullanıyor (3300 ms ölçülür, doğrusu 300); ayrıca planda eksik olan CASTING→iptal süresi eklenecek. Kod Claude'un planındaki lafızdan kaynaklı; düzeltme talimatı plan dosyasında |
| 2026-10-02 | F1-01 | DOĞRULANDI (Tur 1) | 11/11 kriter ✔ (Release, Release `--packet-trace`, Debug bağımsız derlendi; bayraklı exe'de log dizgesi var, bayraksızda yok). Sapma: `WIZ_PARTY` kişisel ad okuduğu için izleme dışı (doğru). Yeni: KI-009 (düşük) |
| 2026-10-01 | F0-02 | DOĞRULANDI (Tur 2) | 14/14 kriter ✔; Tur 1'in 6 bulgusu kapandı (zaman aşımı yolu geçici kopyayla çalışma zamanında doğrulandı). Kalan: KI-008 (düşük, `stop` satırı hep `0 sn`) |
| 2026-10-01 | F0-02 | DÜZELTME GEREKLİ (Tur 1) | 14/14 kriter ✔ (§5.4 Claude tarafından bağımsız yeniden çalıştırıldı, K11 sahte istemciyle çalışma zamanında doğrulandı). Engelleyen: süre/zaman aşımı duvar saati değil (300 sn ≈ 480 sn), zaman aşımı mesajı son durumu kaybediyor; ayrıca `[DOWN]` satırı yok, `stop` etiketleri plandan farklı. Yeni: KI-007 |
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
| (açık blokaj yok) | | | T-ENV-01 kalanı kapandı, 2026-10-02 |

## Sıradaki adımlar

1. Tamamlandı: birleştirme (`main` @ `93d0dcf`), push (`origin/main` @ `8cc00fd`, 2026-10-02) ve `docs/02` §3 / `docs/15` T-ENV-01 güncellemesi. **Bundan sonra tüm iş `main`'den ilerler:** yeni plan dalları `main`'den açılır, zincirleme taban gerekmez.
2. Proje sahibi: `docs/phase-reports/F0-taslak.md`'yi okur ve F0'ı kabul eder (`KABUL_EDILDI`) veya düzeltme ister.
3. Proje sahibi: insan zamanlama oturumu (`docs/15` §4.2.1; `tools/trace-session.sh prepare` … `finish`). F1-02 `main`'de (`017f88e`).
4. Claude: oturum özetlerindeki (`plans/_logs/trace/<etiket>.summary.txt`) sayıları `docs/03` §13/§14 ve `docs/12` §6'ya işler (T-MECH-CLIENT-01..04, Q-01/02/18).

## Otonom döngü

2026-10-01 19:00'da başlatıldı (`plans/_logs/auto-loop.log`); ilk iterasyonda F0-02 planı yazıldı. Tasarım: `plans/OTONOM_DONGU.md`, `tools/auto-loop.sh`. `--run` ön kontrolü: altyapı dosyaları commit'li + çalışma ağacı temiz. Varsayılanlar: `MAX_ITERATIONS=5`, faz sınırında dur, `claude` modeli `opus`, `opencode` modeli `opencode-go/deepseek-v4.1-flash`.

## Devralan için notlar

- Araştırma sırasında sunucu çalıştırılmadı. Tüm mekanik iddialar kod/veri okumasına dayanır; çalışma zamanı doğrulaması F1'in işi.
- 2026-10-01: üç sunucu ilk olarak proje sahibi tarafından 13:09–13:10'da `C:\dev\fdp\server`'dan elle açılmıştı. F0-02 testlerinden beri `tools/run-servers.sh` ile açılıp kapatılıyorlar. Claude'un F0-02 Tur 1 doğrulaması sonunda yine `C:\dev\fdp\server` exe'leriyle 3/3 `UP` bırakıldılar (`FDP_SERVER_BIN_DIR=/mnt/c/dev/fdp/server tools/run-servers.sh start`); bağlı istemci yok `[V]`. Elle açılan sunucular `stop` ile nazikçe kapanmıyor (KI-007). Aynı makinede yolu okunamayan ilgisiz bir `GameServer.exe` (pid 4336) var; betik süreçleri exe yoluyla tanıyor ve ona dokunmuyor `[V]`.
- `tools/build.sh` ve `tools/auto-loop.sh` git'te `100644` modunda (çalıştırılabilir değil). WSL/drvfs'te sorun çıkarmıyor; temiz bir klonda `bash tools/build.sh` gerekir. Küçük bir düzeltme planına veya proje sahibinin `git update-index --chmod=+x` commit'ine bırakıldı.
- Depo dosyaları CRLF + tab. Bazı dosyalar ISO-8859 (Korece yorumlu) veya UTF-8 BOM'lu; kodlama korunmalı (`AGENTS.md` §3).
- `.sh` dosyaları `.gitattributes` ile LF'e sabitlendi (`core.autocrlf=true` betikleri bozmasın diye).
