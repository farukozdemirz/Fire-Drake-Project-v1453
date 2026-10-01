# 20 — Gereksinim İzlenebilirlik Matrisi

> Durum: Taslak v1.0 · Araştırma tarihi: 2026-10-01 · Referans commit: `0f52027`
> Gereksinimler [01](01_PRODUCT_SCOPE_AND_REQUIREMENTS.md)'de, fazlar [17](17_IMPLEMENTATION_ROADMAP_AND_PHASE_GATES.md)'de, testler [15](15_TEST_ARENA_SCENARIOS_AND_ACCEPTANCE_CRITERIA.md)'te tanımlıdır. Durum değerleri [21](21_PROJECT_TRACKING_TEMPLATES_AND_DOC_RULES.md) §1'dedir.
> "Bu pakette karşılandı" yalnızca **dokümantasyon/araştırma** gereksinimleri için kullanılır. Tüm uygulama gereksinimleri `PLANLANDI` durumundadır.

---

## 1. Araştırma ve dokümantasyon

| REQ | Karşılandığı yer | Faz | Test / AC | Durum |
|---|---|---|---|---|
| REQ-RES-01 | 00 başlık, 02 §1, 19 §2 | — | — | Bu pakette karşılandı |
| REQ-RES-02 | Tüm dokümanlarda `dosya:satır` bağlantıları; 19 §2.1 | — | — | Bu pakette karşılandı |
| REQ-RES-03 | 03 §1, 19 §4–5 | — | — | Bu pakette karşılandı |
| REQ-RES-04 | 05 §1 (betikle üretilmiş tablolar), `appendix/A1–A3` | — | — | Bu pakette karşılandı |
| REQ-RES-05 | 02 §1, 18 §3 ve §5 | F1 | T-DATA-* | Bu pakette karşılandı (eksikler listelendi) |
| REQ-RES-06 | 19 §1 etiketleri; tüm dokümanlar | — | — | Bu pakette karşılandı |
| REQ-RES-07 | 18 §5 ("sunucu çalıştırılmadı") | — | — | Bu pakette karşılandı |
| REQ-DOC-01 | 00 | — | — | Bu pakette karşılandı |
| REQ-DOC-02 | 21 §5 (sahiplik tablosu) | — | — | Bu pakette karşılandı |
| REQ-DOC-03 | 06, 07, 08, 09, 10, 11, 12 | — | — | Bu pakette karşılandı |
| REQ-DOC-04 | 03 (MEC/CLI) ve davranış dokümanlarındaki P-* tabloları; 13 §9 | — | — | Bu pakette karşılandı |
| REQ-DOC-05 | Bu doküman | Her faz | Faz kapısı denetimi | Bu pakette karşılandı |
| REQ-DOC-06 | Tüm paket | — | — | Bu pakette karşılandı |
| REQ-DOC-07 | 21, `templates/` | Her faz | — | Bu pakette karşılandı |

## 2. Mimari, karakter, skill, adalet

| REQ | Karşılandığı yer | Faz | Test / AC | Durum |
|---|---|---|---|---|
| REQ-ARC-01 | 02 §10, 13 §1, 18 K-1 | F2 | AC-ARCH-01, Q-14 | PLANLANDI (araştırma tamam) |
| REQ-ARC-02 | 02 §11.1, 13 §2 | F4 | MET-ACT-02, AC-WAR-01 | PLANLANDI |
| REQ-ARC-03 | 02 tamamı; 03 | — | — | Bu pakette karşılandı |
| REQ-NEW-01 | 13 §1, §12 | F2 | AC-ARCH-02 | PLANLANDI |
| REQ-NEW-02 | 02 §4.3, 13 §4.3 | F2 | T-MECH-BUF-* (bot ile), AC-ARCH-01 | PLANLANDI |
| REQ-NEW-03 | 13 §4.1 | F2 | AC-ARCH-04 | PLANLANDI |
| REQ-CHR-01 | 04 §5 | F1–F2 | T-DATA-01, T-DATA-03 | PLANLANDI (tasarım tamam) |
| REQ-CHR-02 | 04 §5.1 (doğrulandı: 255/162 geçerli) | F1 | T-DATA-01/02 | Araştırmada doğrulandı; çalışma zamanı bekliyor |
| REQ-CHR-03 | 04 §6 | F1 | T-DATA-02 | PLANLANDI |
| REQ-NEW-04 | 04 §3.3, 13 §4.3 | F1–F2 | T-DATA-01 | PLANLANDI |
| REQ-NEW-05 | 04 CHR-08, 05 SK-08 | F6 | T-DATA-04 | PLANLANDI |
| REQ-SKL-01 | 05 §5–7, `appendix/A1–A3` | F1 | T-MECH-SKILL-* | Bu pakette karşılandı (veri); doğrulama F1 |
| REQ-SKL-02 | 05 §5.1, 06 §6 | F6 | T-WAR-01, AC-WAR-01/02 | PLANLANDI |
| REQ-SKL-03 | 03 §13.1, 05 §5.2 | F1, F4 | T-MECH-CLIENT-01, MET-FAIR-01 | PLANLANDI |
| REQ-NEW-06 | 05 §4, 07 §7 | F1, F7 | T-MECH-BUF-01..08, AC-PRI-04 | PLANLANDI |
| REQ-FAIR-01 | 03 §13, 14 §4.3/§5.2, 13 §2 | F4 | MET-FAIR-01, AC-LRN-03, AC-ARCH-06 | PLANLANDI |
| REQ-FAIR-02 | 12 §10, 15 §1 | F5, F8 | AC-NAV-05 | PLANLANDI |
| REQ-NEW-07 | 03 §13 (CLI-01..12) | F1 (ölçüm), F4 | T-MECH-CLIENT-*, MET-FAIR-01 | PLANLANDI |
| REQ-NEW-08 | 03 §16, 14 §5.2 | F4 | AC-LRN-03, AC-ARCH-06 | PLANLANDI |
| REQ-NEW-09 | 03 MB-01/CLI-06, 11 §2 | F4 | AC-SUR-04, T-POT-01, T-MECH-POT-05 | PLANLANDI (karar K-5: olduğu gibi) |

## 3. Sınıf davranışları

| REQ | Karşılandığı yer | Faz | Test / AC | Durum |
|---|---|---|---|---|
| REQ-WAR-01 | 06 §4, §6.3; 12 §4.2 | F5–F6 | T-WAR-03, AC-NAV-04 | PLANLANDI |
| REQ-WAR-02 | 06 §6 | F6 | T-WAR-01, AC-WAR-01 | PLANLANDI |
| REQ-WAR-03 | 06 §4 (5, 7), 09 §6 | F6–F7 | T-PTY-03, AC-WAR-02, AC-PTY-03 | PLANLANDI |
| REQ-WAR-04 | 06 §4 (4), 09 §8 | F7 | T-WAR-04, AC-WAR-04 | PLANLANDI |
| REQ-WAR-05 | 06 §7, P-WAR-CHASE-*, P-WAR-FRONTLINE-MAX | F6 | T-WAR-02, AC-WAR-03 | PLANLANDI |
| REQ-WAR-06 | 06 §4 (1), 11 §4 | F6 | T-SUR-01 | PLANLANDI |
| REQ-PRI-01 | 04 §5.3–5.4, 07 §1 | — | — | Bu pakette doğrulandı (bütçe) |
| REQ-PRI-02 | 07 §5 | F6 | T-PRI-01/02, AC-PRI-01/02 | PLANLANDI |
| REQ-PRI-03 | 07 §7 | F7 | T-PRI-04, AC-PRI-04 | PLANLANDI |
| REQ-PRI-04 | 07 §8 | F7 | T-PRI-05, AC-PRI-05 | PLANLANDI |
| REQ-PRI-05 | 07 §11, 11 §4 | F6–F7 | T-PRI-07 | PLANLANDI |
| REQ-PRI-06 | 07 §6, §7.4 | F7 | T-PRI-03, AC-PRI-03, MET-BUFF-03 | PLANLANDI |
| REQ-PRI-07 | 07 §12, 11 §3.3 | F6 | T-POT-02 | PLANLANDI |
| REQ-PRI-08 | 07 §9.1, 05 §4 | F7 | T-PRI-06 | PLANLANDI |
| REQ-PRI-09 | 07 §9.2, 09 §4, §12 | F7 | T-PRI-06, T-PTY-09, AC-PRI-06, AC-PTY-07 | PLANLANDI |
| REQ-MAG-01 | 08 §6 | F6 | T-MAG-01, T-MAG-04 | PLANLANDI |
| REQ-MAG-02 | 08 §7 | F6 | T-MAG-02, AC-MAG-02 | PLANLANDI |
| REQ-MAG-03 | 08 §6.3, §5 | F6–F7 | T-MAG-03, AC-MAG-03 | PLANLANDI |
| REQ-MAG-04 | 08 §8, 09 §9 | F7 | T-MAG-05/06, AC-MAG-04 | PLANLANDI |
| REQ-MAG-05 | 08 §2, §8.1 | F7 | T-MAG-05, AC-MAG-04/05, T-MECH-T8-01 | PLANLANDI |

## 4. Takım, solo, hayatta kalma, pot

| REQ | Karşılandığı yer | Faz | Test / AC | Durum |
|---|---|---|---|---|
| REQ-PTY-01 | 09 §2–3 | F7 | T-PTY-01 | PLANLANDI |
| REQ-PTY-02 | 09 §2.3, 04 §5.4 | F7 | T-PTY-01, AC-PTY-06 | PLANLANDI |
| REQ-PTY-03 | 09 §2.4 | F7–F8 | EVAL-8v8-A/MIX | PLANLANDI |
| REQ-PTY-04 | 09 §3 | F7 | T-PTY-05, AC-PTY-04 | PLANLANDI |
| REQ-PTY-05 | 09 §5 | F7 | T-PTY-02, AC-PTY-01 | PLANLANDI |
| REQ-PTY-06 | 09 §7 | F7 | T-PTY-02, MET-DEBUFF-02 | PLANLANDI |
| REQ-PTY-07 | 09 §8, 06 §4 | F7 | T-WAR-04, MET-PEEL-01 | PLANLANDI |
| REQ-PTY-08 | 09 §8, §10 | F7 | T-PTY-06, AC-PTY-05 | PLANLANDI |
| REQ-PTY-09 | 09 §9–10, 08 §8 | F7 | T-PTY-07, EVAL-WIPE | PLANLANDI |
| REQ-PTY-10 | 09 §11 | F7–F8 | T-PTY-08, EVAL-THIRD | PLANLANDI |
| REQ-PTY-11 | 09 §6 | F7 | T-PTY-03, EVAL-HEALSTALL, AC-PTY-03 | PLANLANDI |
| REQ-PTY-12 | 09 §5.2–5.4 | F7 | T-PTY-04, AC-PTY-02 | PLANLANDI |
| REQ-NEW-10 | 09 §4.1 | F7 | AC-LRN-03 | PLANLANDI |
| REQ-SOLO-01 | 10 | F6 | T-SOLO-01..06 | PLANLANDI |
| REQ-SOLO-02 | 10 §1, AC-SOLO-01 | F6 | EVAL-1v1 | PLANLANDI |
| REQ-SUR-01 | 11 §4.1–4.2 | F6 | T-SUR-01, AC-SUR-02 | PLANLANDI |
| REQ-SUR-02 | 11 §4.2 | F6 | T-SUR-02, AC-SUR-01 | PLANLANDI |
| REQ-SUR-03 | 11 §4.3–4.4 | F6–F7 | T-SUR-03/04, AC-SUR-05 | PLANLANDI |
| REQ-POT-01 | 03 §6.2, 11 §1 | — | T-POT-01 | Bu pakette doğrulandı (veri); çalışma zamanı F1 |
| REQ-POT-02 | 11 §3.2–3.3 | F6 | T-POT-02, AC-SUR-03 | PLANLANDI |
| REQ-POT-03 | 11 §3.1, §3.4–3.5 | F1, F6 | T-MECH-POT-03, T-POT-03 | PLANLANDI |
| REQ-POT-04 | 11 §6 | F6 | AC-SUR-04 | PLANLANDI |
| REQ-POT-05 | 11 §7 | F6, F9 | MET-POT-*, MET-SUR-* | PLANLANDI |

## 5. Navigasyon, öğrenme, test, plan

| REQ | Karşılandığı yer | Faz | Test / AC | Durum |
|---|---|---|---|---|
| REQ-NAV-01 | 12 §2–4, §10 | F5 | T-NAV-04, AC-NAV-01 | PLANLANDI |
| REQ-NAV-02 | 12 §1–3 | — | — | Bu pakette karşılandı (araştırma) |
| REQ-NAV-03 | 12 §2 (eğim), §5 (LoS) | F5 | T-NAV-02, T-NAV-LOS-01 | PLANLANDI |
| REQ-NAV-04 | 12 §4.2, §9 | F5, F7 | T-NAV-06/08 | PLANLANDI |
| REQ-NAV-05 | 12 §4.3, §8, §10 | F5 | T-NAV-07, AC-NAV-04 | PLANLANDI |
| REQ-NEW-11 | 03 CLI-08, 12 §1 | F5 | AC-NAV-03 | PLANLANDI |
| REQ-LRN-01 | 14 §2–3 | F9 | — | Bu pakette karşılandı (tasarım) |
| REQ-LRN-02 | 14 §4–7 | F9 | AC-LRN-06 | PLANLANDI |
| REQ-LRN-03 | 14 §7.3–13 | F9–F10 | AC-LRN-01..05 | PLANLANDI |
| REQ-TST-01 | 15 §2 | F1 | T-ENV-ARENA-01..04 | Araştırmada aday belirlendi; doğrulama F1 |
| REQ-TST-02 | 15 §4 | F1–F8 | Tümü | PLANLANDI |
| REQ-TST-03 | 15 §1, §5, §7 | F8 | AC-EVAL-01..03 | PLANLANDI |
| REQ-MET-01 | 16 §6 | F3 | — | Bu pakette karşılandı (tanım) |
| REQ-MET-02 | 16 §4–5 | F3, F6 | Karar logu örnekleri | PLANLANDI |
| REQ-MET-03 | 16 §7, 14 §8 | F8 | AC-LRN-02 | PLANLANDI |
| REQ-NEW-12 | 15 §2.4, §6 | F8 | EVAL-* (taraf değişimli, ulus bazlı rapor) | PLANLANDI (karar K-6) |
| REQ-NEW-13 | 17 F1 | F1 | T-MECH-CLIENT-* | PLANLANDI |
| REQ-PLN-01 | 17 §2 | — | — | Bu pakette karşılandı |
| REQ-PLN-02 | 17 §1 (F4–F8 önce, F9–F10 sonra) | — | — | Bu pakette karşılandı |
| REQ-PLN-03 | 21 §1, 17 §4 | Her faz | Faz kapısı | Bu pakette karşılandı |
| REQ-PLN-04 | 17 §3, 01 §2 | — | — | Bu pakette karşılandı |
| REQ-NEW-14 | — | — | — | KALDIRILDI (K-9) |
| REQ-NEW-15 | 18 R-12 | F0 | Kontrol listesi | PLANLANDI |

## 6. Ters izlenebilirlik (faz → gereksinim özeti)

| Faz | Ana gereksinimler |
|---|---|
| F0 | REQ-NEW-15, REQ-TST-01 (ortam) |
| F1 | REQ-CHR-02/03, REQ-NEW-04/07/09/13, REQ-POT-01/03, REQ-SKL-01/03, REQ-TST-01 |
| F2 | REQ-ARC-01, REQ-NEW-01/02/03/14 |
| F3 | REQ-MET-01/02 |
| F4 | REQ-ARC-02, REQ-FAIR-01, REQ-NEW-07/08 |
| F5 | REQ-NAV-*, REQ-NEW-11, REQ-FAIR-02 |
| F6 | REQ-WAR-*, REQ-PRI-02/05/07, REQ-MAG-01..03, REQ-SOLO-*, REQ-SUR-*, REQ-POT-02..05 |
| F7 | REQ-PTY-*, REQ-PRI-03/04/06/08/09, REQ-MAG-04/05, REQ-NEW-06/10 |
| F8 | REQ-TST-02/03, REQ-MET-03, REQ-NEW-12 |
| F9–F10 | REQ-LRN-* |

## Değişiklik günlüğü

| Tarih | Sürüm | Değişiklik |
|---|---|---|
| 2026-10-01 | v1.0 | İlk sürüm |
