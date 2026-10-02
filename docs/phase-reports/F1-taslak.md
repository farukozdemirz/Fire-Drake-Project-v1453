# Faz Sonuç Raporu — F1 Veri ve mekanik doğrulama (TASLAK)

Tarih: 2026-10-02 · Hazırlayan: Claude (otonom gece döngüsü) · Onaylayan: — (bekliyor)
Değerlendirilen commit: `9148336` (`gece/2026-10-02`; F1-10 birleştirmesi) · Sunucu commit: `0f52027` + F1-01/F1-09 kancaları (bayrakla kapalı) · DB özeti: yerel `FDP_kn_online`; `MAGIC.Etc` düzeltmesi `db/001` ile, 12 bot karakteri `db/002` ile kurulu

> **Durum: TASLAK.** DeepSeek'in yapabileceği F1 işleri bitti (F1-01..F1-10 `DOĞRULANDI`/`KAPANDI`). F1'in kabulü için gereken **insan istemcisi testleri bekliyor** (aşağıda §4 ve `docs/STATUS.md` "Proje sahibi testleri (bekleyen)"). Bu rapor bu yüzden `KABUL_EDILDI` önermez: çıkış kararı §10'da **kısmi**. Faz durumu `GELIŞTIRILDI` düzeyindedir. Gece modunda sonraki faza (F2) geçildi, kabul bekleniyor.

## 1. Amaç (`docs/17` §2'den)

`docs/03`–`05`'teki `[D]`/`[A]` iddiaların çalışma zamanında doğrulanması; istemci zamanlama profilinin ölçülmesi; test verisi düzeltmeleri.

## 2. Teslim edilen kapsam

| İş kalemi | Durum | Commit(ler) / plan | Not |
|---|---|---|---|
| Görev 1: Paket izleyici (derleme bayrağıyla) | GELIŞTIRILDI | F1-01 (`main` @ `93d0dcf`) | `FDP_PACKET_TRACE`; bayraksız exe'de iz yok `[V]` |
| Zamanlama oturumu araçları | GELIŞTIRILDI | F1-02 (`main` @ `017f88e`) | `tools/trace-session.sh`, `packet-trace-summary.py --cli` |
| KI-001: `MAGIC.Etc` kalıcı betiği (ADR-0003) | GELIŞTIRILDI, TEST_EDILDI (T-DATA-06) | F1-03 (`main` @ `f4e4298`) | Geçici kopyada uygula / idempotent / geri al doğrulandı |
| Bot karakter kurulum betiği (ADR-0002) | GELIŞTIRILDI | F1-04 (`main` @ `0061930`) | 12 karakter (6 profil × 2 ulus), geri alma betiği; gerçek DB'de doğrulandı |
| Bot ekipman/ağırlık raporu (Q-21, MB-12) | GELIŞTIRILDI | F1-05 (`main` @ `acc3adb`) | 100 × 1440 HP pot şablonu 8/12 botu ağırlık sınırının üstüne çıkarıyor (stok politikası F2+'de) |
| Fiziksel hasar modeli ve başlangıç HP/MP | GELIŞTIRILDI | F1-06 (`main` @ `7ca8a12`) | `tools/stat-model.py`; botlar Hp=Mp=32000 |
| Büyü ve heal modeli | GELIŞTIRILDI | F1-07 (`main` @ `d66935d`) | `tools/spell-model.py` |
| Arena A veri doğrulaması | GELIŞTIRILDI | F1-08 (`gece/2026-10-02`, merge `24c0f36`) | `tools/arena-report.py`; eksen önerisi A için `angle=15`; zone 71 `bRange=0` bulgusu |
| Sunucu tarafı hasar kaydı (T-MECH-DMG altyapısı) | GELIŞTIRILDI | F1-09 (`gece/2026-10-02`, merge `1fb2d0b`) | `FDP_DAMAGE_TRACE`, bayrakla kapalı |
| Hasar logu özet/karşılaştırma betiği | GELIŞTIRILDI | F1-10 (`gece/2026-10-02`, merge `9148336`) | `tools/damage-trace-summary.py`, ± %15 hükmü |
| Görev 2: istemci zamanlama kayıtları (T-MECH-CLIENT-01..04) | TEST_EDILDI (kısmi) | `docs/03` §13.2–13.3 | Warrior, priest, mage; tek karakter/sınıf, tek silah gecikmesi (164). `[V]` ama tekrar bekliyor |
| Görev 6: etiket yükseltme ve düzeltmeler | KISMEN | `docs/03` §13, `docs/04` §3.4 | CLI-01..06, 11, 12 ölçüldü; `[A]`'ların kalanı aşağıda |

## 3. Kapsam dışında kalanlar / ertelenenler

- Bot kodu → F2 ve sonrası (F1 kapsam dışı).
- Pot verisi değiştirilmedi (K-5, ADR-0009).
- Q-04 (Etc 510–523 skill'leri) → ileri profiller (temel sürüm dışı).

## 4. Çalıştırılan testler ve bekleyenler

| Test kimliği | Tekrar | Sonuç | Kanıt kaydı |
|---|---|---|---|
| T-DATA-06 (`MAGIC.Etc` kalıcı kayıt) | geçici kopya | GEÇTİ (2026-10-02) | `plans/F1-03-…` Doğrulama Tur 1 |
| T-DATA-01 (bot giriş değerleri) | 5/6 bot girişte | GEÇTİ: HP/MP/AC/saldırı modelle birebir | `docs/04` §3.4; kalan M-I girişinin teyidi bekliyor |
| T-MECH-CLIENT-01..04 | 4 oturum (war-r, war-skill, pot, pri-cast/mag-cast, war-combo) | GEÇTİ (kısmi: tek karakter/sınıf) | `docs/03` §13.2–13.3 |
| T-ENV-ARENA veri ön hesabı | araç, 2 çalıştırma | GEÇTİ (veri düzeyi) | `plans/F1-08-…` Doğrulama Tur 2 |
| **T-MECH-DMG-01..03** | R: 124 vuruş | **KISMEN GEÇTİ (2026-10-02)** | R vuruşu modelle ±%1,4 (W-P→W-G 72,8/73,0; W-G→W-P 92,5/92,9; W-P→W-P 156,9/159,1); kalan: Type1 skill, Malice (-02), büyü CHA ölçeği (-03), isabet oranı |
| **T-ENV-ARENA-01, 03** (ve 02, 04) | — | **BEKLİYOR (insan)** | `docs/STATUS.md` bekleyen testler |
| T-DATA-01 (M-I) | 2228/6021/612/57 | **GEÇTİ (2026-10-02)** | modelle birebir |
| **T-DATA-02/03** | — | **BEKLİYOR (insan)** | Kuşanılabilirlik ve maks HP/MP oyunda |
| T-MECH-SKILL-*, T-MECH-BUF-*, T-POT-*, T-MECH-POT-03..05 | — | **BEKLİYOR (insan)** | Çekirdek skill başına MP/recast/menzil; buff çakışmaları; pot hareketi durdurur mu (Q-06) |
| `war-move` (Q-02) ve ikinci priest/mage oturumu | — | **BEKLİYOR (insan)** | `docs/STATUS.md` "Sıradaki adımlar" 3 |

## 5. Kabul kriterleri

| AC kimliği | Kriter | Ölçülen değer | GA (%95) | Karşılandı mı |
|---|---|---|---|---|
| (F1 kabul-1) | Kritik açık soruların cevaplanması (`docs/18` §3 "F1'de kapanmalı") | Q-18, Q-21 (kod düzeyi) cevaplandı; Q-06 kısmen (ortak ~2,5 sn); Q-03, Q-08, Q-21 (çalışma zamanı) açık | — | Hayır (kısmi) |
| (F1 kabul-2) | CLI tablosunun ölçülmüş değerlerle doldurulması | CLI-01..06, 11, 12 ölçüldü (`docs/03` §13.3); tek karakter kapsam sınırı notlu | — | Evet (kapsam notuyla) |
| (F1 kabul-3) | Tüm T-MECH testlerinin `TEST_EDILDI` olması | T-MECH-CLIENT: kısmi; T-MECH-DMG/SKILL/BUF: bekliyor | — | Hayır |

## 6. Sapmalar ve açıklamaları

1. **Sıra sapmaları:** F1-01 F0 kabulünden önce yapıldı (F0 raporu §6). Gece modunda F1-08..F1-10 `gece/2026-10-02` dalına birleşti; `main`'e birleştirme proje sahibinin sabah kararında.
2. **Araç ağırlıklı kapsam:** Hasar doğrulaması (T-MECH-DMG) insan istemcisi gerektirdiğinden bu fazda yalnızca model (F1-06/07), ölçüm altyapısı (F1-09) ve analiz aracı (F1-10) teslim edildi; ölçüm sonucu yok.
3. **Kanıt gücü:** CLI ölçümleri tek karakter, tek silah gecikmesi (164), tek oturum günü `[V]`; farklı silah/sınıf tekrarı yapılmadı.

## 7. Yeni bilinen sorunlar (KNOWN_ISSUES kimlikleri)

F1 sırasında yeni kimlik açılmadı. KI-009 (`.py` satır sonu) F1-01 doğrulamasında bulundu ve 2026-10-02'de kapandı. Bulgular (KI olmayan): 8/12 botun ağırlık sınırı aşımı (F1-05), zone 71 `START_POSITION` `bRange=0` (F1-08).

## 8. Bu fazda alınan kararlar (ADR kimlikleri)

Yeni ADR yok. ADR-0002 (karakter kurulumu) ve ADR-0003 (`MAGIC.Etc`) uygulandı. Açık: ADR-0005..0008 (ilgili fazlarda).

## 9. Doküman güncellemeleri

| Dosya | Bölüm | Özet |
|---|---|---|
| `docs/03` | §13.2–13.3 | Ölçülen istemci değerleri, ölçülmüş CLI özeti, cast iptali kuralı, paket düzeni düzeltmesi (§14) |
| `docs/04` | §3.4 | Kurulum betiği, veri biçimi, T-DATA-01 giriş doğrulaması |
| `docs/15` | T-DATA-06 | Kanıt |
| `docs/STATUS.md`, `plans/README.md`, `docs/KNOWN_ISSUES.md` | — | Güncel durum |
| **Bekleyen (Claude)** | `docs/15` §2.4, §2.3; `docs/03` MEC-ZON-03 | Eksen `angle=15`, spawn payı tanımı notu, zone 71 otomatik taşıma yolları (F1-08 doğrulamasından) |

## 10. Çıkış kararı

- [ ] Tüm çıkış koşulları karşılandı → KABUL_EDILDI
- [x] Kısmi → `GELIŞTIRILDI` olarak kalır; eksikler: T-MECH-DMG-01..03, T-MECH-SKILL/BUF/POT, T-DATA-01 (M-I) ve T-DATA-02/03, T-ENV-ARENA-01..04, Q-03/Q-08/Q-06 (hareket) cevapları, `docs/03` etiket yükseltmeleri (bu testlerin sonuçlarıyla)

Faz kapısı denetim listesi (`docs/17` §4): iş kalemleri ✔ (araçlar/kod) · testler ✘ (insan testleri bekliyor) · kabul kriterleri kısmen eşlendi · yeni sorunlar KNOWN_ISSUES'te ✔ · dokümanlar kısmen güncel · `docs/20` güncellemesi bekliyor · geri alma yolu: DB betikleri geri alma betikleriyle, izleyiciler derleme bayrağıyla ✔ · faz raporu: taslak.

## 11. Geri alma bilgisi

DB: `db/001_magic_etc_fix_rollback.sql`, `db/002`'nin geri alma betiği (`MAGIC_BAK_etc` yedeği). Kod: `FDP_PACKET_TRACE` ve `FDP_DAMAGE_TRACE` yalnızca derleme bayrağıyla açılır; bayraksız derleme davranışı değiştirmez. F1-09/F1-10 birleştirmesini geri almak için `git revert -m 1 1fb2d0b 9148336` (gece dalı).
