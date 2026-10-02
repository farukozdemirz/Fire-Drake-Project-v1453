# Faz Sonuç Raporu — F2 Bot oturumu (TASLAK)

Tarih: 2026-10-02 · Hazırlayan: Claude (otonom gece döngüsü) · Onaylayan: — (bekliyor)
Değerlendirilen commit: `ed3922f` (`gece/2026-10-02`; F2-06 birleştirmesi) · Sunucu commit: `0f52027` + F1-01/F1-09 kancaları (bayrakla kapalı) + F2 bot kodu (`[BOT] ENABLED=0` varsayılan) · DB özeti: yerel `FDP_kn_online`; 12 bot karakteri `db/002` ile kurulu (koşular logout kaydıyla satırları günceller)

> **Durum: TASLAK.** DeepSeek'in yapabileceği F2 işleri bitti (F2-01..F2-06 `KAPANDI`). F2'nin kabulü için gereken **insan istemcisi testleri bekliyor** (aşağıda §4 ve `docs/STATUS.md` "Proje sahibi testleri (bekleyen)": T-ARCH-01..04). Bu rapor bu yüzden `KABUL_EDILDI` önermez: çıkış kararı §10'da **kısmi**. Faz durumu `GELIŞTIRILDI` düzeyindedir. Gece modunda sonraki faza (F3) geçildi, kabul bekleniyor.

## 1. Amaç (`docs/17` §2'den)

Soketsiz bot oyuncunun dünyaya girmesi, görünmesi, durması ve güvenle çıkması. Kapsam: S1 (ayrılmış slotlar), S2 (bot alıcısı), S3–S5 (giriş, zaman aşımı), S7 (yalnızca sabit IP / atlama; ranking/ödül/duyuruya dahil, K-9), S8 (`Update`), bot karakter kurulum betiği, `/bot spawn/despawn` (minimum). Kapsam dışı: karar verme; bot hareketsizdir.

## 2. Teslim edilen kapsam

| İş kalemi | Durum | Commit(ler) / plan | Not |
|---|---|---|---|
| S1+S2: rezerve slot havuzu (`KOSocketMgr`), `CUser::m_botSink` + `Send` geçersiz kılma, `BotManager::Startup` + havuz öz-sınaması | GELIŞTIRILDI, TEST_EDILDI (çalışma zamanı) | F2-01 (`gece/2026-10-02`, merge `e7f8119`) | `ENABLED=1`: `reserved 16 sessions (ids 2984-2999), pool self-test OK`; `ENABLED=0`: log yok `[V]` |
| `BOT_TICK` IOCP olayı ve bot zamanlayıcı thread'i (ADR-0005) | GELIŞTIRILDI, TEST_EDILDI | F2-02 (merge `b9b89ba`) | Tick IOCP thread'inde; `TICK_MS=100` → ort. 110,5 ms, `TICK_MS=20` → 31,7 ms, skipped 0 `[V]` |
| S3/S7: bot girişi (spawn), `WIZ_SEL_CHAR` DB isteği, `GameStart(1/2)` taklidi (ADR-0014) | GELIŞTIRILDI, TEST_EDILDI (sunucu tarafı) | F2-03 (merge `3734ec4`) | `SPAWN_ON_START` ile 4/4 `in game` `[V]`; görünürlük insan testinde |
| S4/S5/S8: bot çıkışı (despawn), `Update()` ve zaman aşımı muafiyeti | GELIŞTIRILDI, TEST_EDILDI (sunucu tarafı) | F2-04 (merge `617a922`) | `DESPAWN_AFTER_SEC=20`: 4/4 `despawned`, `logout save` 106–112 ms, `names cleared yes`, `pool free 16/16` `[V]` |
| Yeniden spawn döngüsü (`RESPAWN_CYCLES`), T-PERF-06 altyapısı | GELIŞTIRILDI, TEST_EDILDI | F2-05 (merge `0765ab8`) | 12 bot × 84 = **1008 spawn/despawn**: çökme 0, slot sızıntısı 0, isim sızıntısı 0, 0 `FAILED`/`TIMEOUT` `[V]` |
| `/bot spawn\|despawn\|list` (konsol) ve `BotCommands.txt` (ADR-0015) | GELIŞTIRILDI, TEST_EDILDI (dosya yolu) | F2-06 (merge `ed3922f`) | Komut dosyası yolu çalışma zamanında doğrulandı; konsol yolu T-ARCH-04 (insan) bekliyor |
| Bot karakter kurulum betiği (6 profil × 2 ulus) | GELIŞTIRILDI | F1-04 (`main` @ `0061930`) | Faz F1'den; F2 koşuları bu satırları kullandı |

## 3. Kapsam dışında kalanlar / ertelenenler

- Karar verme ve hareket (F4 sonrası); botlar hareketsizdir.
- R-CODE-01 (kilitsiz harita kopyaları): 1008 döngüde tetiklenmedi; kök neden düzeltmesi yapılmadı (kapsam dışı). 4 saatlik/64 bot dayanıklılığı (AC-ARCH-03, T-PERF-04) F2 kapsamında değil.
- `MAX_BOTS=16` havuzu vardır ama **yalnızca 12 sabit bot** (`BOT_TABLE`) tanımlıdır; "16 bot spawn" ifadesi bu fazda 12 bot ile karşılanır (sapma, §6).
- Ölüm/regene yönetimi (`WIZ_REGENE`), zone değişimi, bot kapanışında `KickOutAllUsers` çifte kayıt incelemesi: ilgili fazlarda.

## 4. Çalıştırılan testler ve bekleyenler

| Test kimliği | Tekrar | Sonuç | Kanıt kaydı |
|---|---|---|---|
| T-PERF-06 (spawn/despawn × 1000) | 1 koşu, 1008 döngü, ~4 dk | GEÇTİ: çökme 0, slot sızıntısı 0, isim sızıntısı 0 | `plans/F2-05-…` Doğrulama Tur 1 |
| AC-ARCH-02 / T-PERF-05 (bot kapalı regresyon) | her F2 planında `ENABLED=0` senaryosu | GEÇTİ (log yok, ini'ye anahtar yazılmıyor, davranış değişmiyor); ölçülü CPU karşılaştırması yapılmadı | F2-01..F2-06 Doğrulama raporları |
| AC-ARCH-05 (despawn sonrası kayıt tam) | 4 bot × her koşu | GEÇTİ (sunucu tarafı): `WIZ_LOGOUT` kaydı bitince slot iadesi, `logout save` ~110 ms, `names cleared yes`; USERDATA içeriği gizlilik kuralı gereği okunmadı | F2-04 Doğrulama Tur 1 |
| Çalışma zamanı komutları (dosya yolu) | 8 senaryo/madde | GEÇTİ | `plans/F2-06-…` Doğrulama Tur 1 |
| **T-ARCH-01 (AC-ARCH-04)** | — | **BEKLİYOR (insan)** | Gerçek bağlantı 2984-2999 kimliklerini almıyor mu; `ENABLED=1` iken normal giriş |
| **T-ARCH-02 (AC-ARCH-01)** | — | **BEKLİYOR (insan)** | 4 bot doğru sınıf/ırk/ekipmanla görünüyor mu |
| **T-ARCH-03 (AC-ARCH-01)** | — | **BEKLİYOR (insan)** | Despawn sonrası hayalet/donuk bot yok; istemci donmuyor |
| **T-ARCH-04 (AC-ARCH-01)** | — | **BEKLİYOR (insan)** | Konsol `/bot spawn\|despawn\|list` |

## 5. Kabul kriterleri

| AC kimliği | Kriter | Ölçülen değer | GA (%95) | Karşılandı mı |
|---|---|---|---|---|
| (F2 kabul-1) | 16 bot spawn | 12 sabit bot (4'lü ve 12'li koşular), 12/12 `in game` | — | Kısmi (12 ≤ 16, sapma §6) |
| (F2 kabul-2) | İnsan istemcisi botları doğru sınıf/ırk/ekipmanla görüyor | — | — | Hayır (T-ARCH-02..04 bekliyor) |
| (F2 kabul-3) | 1000 spawn/despawn: çökme ve slot sızıntısı 0 | 1008 döngü: çökme 0, sızıntı 0 | — | Evet |
| (F2 kabul-4) | Çıkışta DB kaydı tam | `WIZ_LOGOUT` işleniyor, slot kayıttan sonra iade; içerik doğrulaması gizlilik nedeniyle yapılmadı | — | Kısmi (sunucu tarafı kanıt) |
| AC-ARCH-04 | Gerçek oyuncu bot slotu almaz | kod incelemesi: rezerve kimlikler `m_idleSessions`'a hiç girmez | — | Kısmi (T-ARCH-01 bekliyor) |

## 6. Sapmalar ve açıklamaları

1. **"16 bot" → 12 bot:** `BOT_TABLE` 12 sabit hesap/karakterle sınırlıdır (6 profil × 2 ulus, ADR-0014); `MAX_BOTS` havuz boyutudur. 16 bot gerekirse F8'de ek hesap/karakter kurulumu gerekir.
2. **Sıra sapması:** F2 gece modunda F1'in insan testleri tamamlanmadan başladı; F1 kabulü bekliyor.
3. **Otonom kararlar:** ADR-0005, ADR-0014 (+F2-04 eki), ADR-0015 proje sahibi onayı olmadan kabul edildi; gözden geçirilmeli.

## 7. Yeni bilinen sorunlar (KNOWN_ISSUES kimlikleri)

F2 sırasında yeni KI kimliği açılmadı. Gözlemler: `PHASE_FAILED`/`PHASE_DESPAWN_STUCK` oturumları süreç sonuna kadar kalır (kurtarma komutu yok, F2-06 kapsam dışı); `spawn complete` özeti süreç başına bir kez yazılır (komutla gelen sonraki botlar için tekrarlanmaz); sunucu kapanışında `PHASE_DESPAWN_WAIT` içindeki bot için DB kaydı çifte yazılabilir (F2-04 §8; gerçek oyuncularda da aynı).

## 8. Bu fazda alınan kararlar (ADR kimlikleri)

ADR-0005 (bot tick'i IOCP thread'inde), ADR-0014 (bot oturumu hesap doğrulamasını ve `SET_LOGIN_INFO`'yu atlar; çıkışta `AccountLogout` de atlanır), ADR-0015 (bot çalışma zamanı komut kanalı). Üçü de "otonom döngüde Claude kararı — gözden geçirilmeli".

## 9. Doküman güncellemeleri

| Dosya | Bölüm | Özet |
|---|---|---|
| `docs/adr/` | ADR-0005, 0014, 0015 | Yeni kararlar |
| `docs/STATUS.md`, `plans/README.md` | — | Plan durumları |
| `docs/13` | §4.4 (yeni) | `[BOT]` ini anahtarları tablosu (F2 + F3-01) |
| `docs/16` | §3.3 (yeni) | Telemetri uygulama notları (ADR-0007) |

## 10. Çıkış kararı

- [ ] Tüm çıkış koşulları karşılandı → KABUL_EDILDI
- [x] Kısmi → TEST_EDILDI olarak kalır; eksikler: T-ARCH-01..04 insan testleri (görünürlük, hayalet bot yok, gerçek bağlantı slot almıyor, konsol komutu); "16 bot" sapmasının kabulü

## 11. Geri alma bilgisi

Bu fazın değişiklikleri `[BOT] ENABLED=0` (varsayılan) ile devre dışıdır. Kod olarak geri almak için `gece/2026-10-02` dalında `e7f8119`..`ed3922f` birleştirmeleri (F2-01..F2-06) ters sırayla geri alınır; `ENABLED=1` ile açılan sunucuda botlar `/bot despawn all` veya `DESPAWN_AFTER_SEC` ile güvenle çıkarılır.
