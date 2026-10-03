# Değerlendirme Takibi: plan hazır → kod uygulandı → oyun içinde doğrulandı

> Kalıcı takip tablosu (proje sahibi isteği, 2026-10-02). Her düzeltme/plan için **üç durum ayrı sütunda** tutulur. Kaynak rapor: [`degerlendirme-2026-10-02.md`](degerlendirme-2026-10-02.md), eki [`degerlendirme-2026-10-02-ek.md`](degerlendirme-2026-10-02-ek.md). Durumlar plan dosyalarının `Durum` satırından ve dallardan okunur (son okuma: `gece/2026-10-02` @ `d3605e2`, `gece/2026-10-02-nav` @ `196857d`); satırı güncelleyen kişi okuduğu dal ucunu ve tarihi yazar.

## Kapanış kuralı (değişmez)

**Bir madde yalnızca doküman güncellendiği veya birim testleri geçtiği için "doğrulandı" yazılmaz.** Üç durum ayrıdır:

| Durum | Anlamı | Kim yazar |
|---|---|---|
| **Plan** | `TASLAK` / `HAZIR` / `UYGULANIYOR` / `UYGULANDI` / `DOĞRULANDI` / `KAPANDI` (plan dosyasının `Durum` satırı; `plans/README.md` akışı) | planlayıcı/denetçi |
| **Kod** | Branch ve commit; entegrasyon dalına birleşti mi (merge kimliği). `—` = kod değişikliği yok (doküman/ADR/araç) | döngü betiği / denetçi |
| **Oyun içi** | İlgili davranışın **çalışan sunucuda** (bot + gerekirse insan istemcisi) gözlendiği ve kanıtın yazıldığı doğrulama: test kimliği + `BEKLİYOR` / `YAPILDI` (+ kim, nerede). Birim testi, derleme veya doküman bu sütunu **kapatmaz** | Claude (sunucu+bot) / proje sahibi (insan istemcisi) |

`GEREKMİYOR` yalnızca saf araç/doküman maddeleri içindir ve gerekçesi yazılır. Faz kabulü (`KABUL_EDILDI`) bu tablodan bağımsızdır ve yalnızca proje sahibindedir (`docs/STATUS.md` "Faz kabul takibi").

## 1. Planlar

| ID | Konu | Faz | Plan | Kod | Oyun içi doğrulama (test, durum) | Kanıt yolu |
|---|---|---|---|---|---|---|
| F4-50 | Algı: oyuncu adı, konum yaşı/hız/geçmiş, kaynak etiketi, tazelik (R5 aracı güncellemesi) | F4 | DOĞRULANDI | `bot/F4-50` → birleşti (`gece/2026-10-02`, merge `d3605e2`) | K10 `snap`/`see` (hız, `pos_age`, ad `list` ile aynı): **YAPILDI** (sunucu+bot, Claude, plan Doğrulama Tur 2); T-PERC-01 insan ölçümü (tazelik eşikleri `[A]`): **BEKLİYOR** | `plans/F4-50-algi-gozlem-meta-verisi.md` Doğrulama Raporu |
| F4-51 | Algı: düşman/hedef HP tablosu (`WIZ_TARGET_HP`) | F4 | HAZIR | `bot/F4-51` uygulanıyor (birleşmedi) | K10 (`snap` düşman HP `list` ile çapraz, ölünce `hp=?`): **BEKLİYOR** | `plans/F4-51-algi-hedef-hp-tablosu.md` |
| F4-52 | Algı: skill olay halkası (`WIZ_MAGIC_PROCESS`) | F4 | HAZIR | başlamadı | K9 (kurban + izleyici `snap events`, CASTING→EFFECTING): **BEKLİYOR** | `plans/F4-52-algi-skill-olay-halkasi.md` |
| F4-53 | Algı: gözlenen buff/debuff/heal tablosu (tahmin sınıfı E) | F4 | TASLAK | başlamadı | gözlenen durum ↔ gerçek buff listesi (±2 sn): **BEKLİYOR** (ADR-0018 m.4 sonrası) | `plans/F4-53-algi-gozlenen-durum-tablosu.md` |
| F4-54 | `ObsTable` tek yönlü görüş teşhisi (KI-DEG-01) | F4 | HAZIR | başlamadı | K8 (12 koşu simetri): **BEKLİYOR** | `plans/F4-54-algi-tek-yonlu-gorus-teshisi.md` |
| F5-50 | Kiriş yürünebilirlik denetimi `NavSegment.h` (CLI-08) | F5 | HAZIR | `bot/F5-50` uygulanıyor (birleşmedi) | AC-NAV-03: düz `/bot move` engel kesen adımı `FAIRNESS_REJECT CLI-08` ile durdurur (F5-55): **BEKLİYOR** | `plans/F5-50-nav-kiris-yurunebilirlik-denetimi.md` |
| F5-51 | Arena sınırı ve doğuş yolu (A* `NodeLimit`) | F5 | HAZIR | başlamadı | T-NAV-10 (Karus ve El Morad ölüm→respawn→arena, ayrı ayrı): **BEKLİYOR** | `plans/F5-51-nav-arena-siniri-ve-dogus-yolu.md` |
| F5-52 | Hız kestirimi 1,5 sn paket sıklığı | F5 | DOĞRULANDI | `bot/F5-52` → birleşti (`gece/2026-10-02-nav`, merge `196857d`) | T-NAV-06 (hareketli hedef takibi, öngörü noktası ↔ gerçek konum): **BEKLİYOR** (birim + güncel kod ölçümü yapıldı; oyun içi yok) | `plans/F5-52-nav-hedef-hiz-kestirimi-paket-sikligi.md` |
| F5-53 | Çoklu bot sorgu bütçesi, ertelenen sorgu sözleşmesi, önbellek | F5 | DOĞRULANDI | `bot/F5-53` (taban `gece/2026-10-02-nav`; birleştirme döngü betiğinde; birim + gerçek harita ölçümü yapıldı, oyun içi yok) | T-NAV-11 (16 bot oyun içi tick/yol bütçesi, ertelemede bekleme/takip): **BEKLİYOR** | `plans/F5-53-nav-sorgu-butcesi-ve-onbellek.md` |
| F5-54 | Takılma tespiti paket sıklığı hazır ayarı + guard-engeli dedektörü (F5-09 sonrası uyarlama) | F5 | HAZIR | başlamadı | T-NAV-04 (takılma ≤ 2/bot-saat): **BEKLİYOR** | `plans/F5-54-nav-takilma-tespiti.md` (nav dalı) |
| F5-55 | Nav sunucu entegrasyonu ve gerçek harita doğrulaması | F5 | TASLAK | başlamadı | T-NAV-04/05/09/10/11, AC-NAV-01..07: **BEKLİYOR** | `plans/F5-55-nav-sunucu-entegrasyonu-ve-gercek-harita.md` |
| F5-56 | Hız kestirimi dayanıklılık (zaman damgası, değişken aralık, kayıp, eski veri, ani değişim, sıçrama) | F5 | DOĞRULANDI | `bot/F5-56` (taban `gece/2026-10-02-nav`; birleştirme döngü betiğinde; birim ölçüm yapıldı, oyun içi yok) | T-NAV-06: **BEKLİYOR** | `plans/F5-56-nav-hiz-kestirimi-dayaniklilik.md` |
| F5-57 | Takılma: hareket niyeti ve gerçek rota ilerlemesi (`NavProgressAssessor`) | F5 | DOĞRULANDI | `bot/F5-57` (taban `gece/2026-10-02-nav`; birleştirme döngü betiğinde; birim + sentetik ölçüm yapıldı: F5-09 varsayılanı 6 yanlış epizot → 0, oyun içi yok) | T-NAV-04: **BEKLİYOR** | `plans/F5-57-nav-niyet-ve-gercek-ilerleme.md` |
| F5-58 | Duvar denetimi kalıcı regresyonu (ham yol, düzleştirme, `NavLineClear`, düz adım vektörleri) | F5 | HAZIR | başlamadı | AC-NAV-03 (F5-55): **BEKLİYOR** | `plans/F5-58-nav-duvar-denetimi-kalici-regresyon.md` |

## 2. Plan dışı düzeltmeler (doküman, ADR, araç)

Bunlar kod değiştirmez; oyun içi doğrulamaları ilgili davranış fazında ayrı satırlarla izlenir.

| ID | Konu | Faz | Plan | Kod | Oyun içi doğrulama (test, durum) | Kanıt yolu |
|---|---|---|---|---|---|---|
| DEG-03 | F4-18 party isim uzunluğu (zaten giderilmişti) | F4 | DOĞRULANDI (F4-18) | birleşti (`a7349a1`) | S1–S5 **YAPILDI** (sunucu+bot, Claude); `PARTY_HPCHANGE` yalnız HP düşüşüyle: **BEKLİYOR** (düşük öncelik) | `plans/F4-18-algi-takim-gorunumu.md` Doğrulama Tur 2 |
| DEG-05/06/07 | Priest `+ pending_heals`, rezervasyon yaşam döngüsü, net HP→`incoming_est` | F7 | — (doküman düzeltildi) | — (kod yok) | T-PRI-03 (iki priest çift heal ≤ %5), AC-PRI-09: **BEKLİYOR** (F7) | `docs/07` §5.1, `docs/09` §4.3 |
| DEG-12..16 | Mekanik tutarsızlıkları (skill+R, hareket 1,5 sn, pot, respawn, aksiyon sınırı) | F6 | — (doküman düzeltildi) | — | Q-25 priest/mage insan hızı, T-REGENE-01, T-MECH-POT-03: **BEKLİYOR** | `docs/03` §13.4 |
| M1 | Öğrenme: rol profili önce, karakter bazlı kalıcı öğrenme **F12** (taslak) | F9/F12 | ADR-0030-DEG KABUL | — | AC-LRN-01..08, AC-CHR-01..06: **BEKLİYOR** (F9/F12) | `docs/adr/ADR-0030-DEG-ogrenme-duzeyi.md`, `docs/17` F12 |
| M2 | Kazanma kuralları `killdiff_timed` / `wipe_first` (örnekler, yapılandırılabilir eşikler) | F8 | ADR-0031-DEG KABUL | — | `ScenarioRunner` `win_rule` + 20 maç pilot (beraberlik %15–35): **BEKLİYOR** (F8) | `docs/adr/ADR-0031-DEG-senaryo-kazanma-kurali.md`, `docs/15` §6b |
| M3 | Başlangıç yerleşimi: maç öncesi DB yazımı, canlı `CUser` tutarlılığı | F8 | ADR-0032-DEG KABUL | — | T-IGT-EVAL-01 başlangıç doğrulaması %100: **BEKLİYOR** (F8) | `docs/adr/ADR-0032-DEG-senaryo-baslangic-yerlesimi.md`, `docs/15` §6a |
| M4 | Arena modunda geri çekilme arena içinde; serbest Ronark (F11-a/b/c) ayrı | F5–F11 | ADR-0033-DEG KABUL | — | T-NAV-10 (F5-55); T-FREE-06..08: **BEKLİYOR** | `docs/adr/ADR-0033-DEG-arena-siniri-ve-geri-cekilme.md`, `docs/17` F11 |
| M6 | 16 (asgari, C8-A) ve 20 (kompozisyon çeşitliliği) karakter dökümü | F8 | doküman | — | `db/003` + 16/20 karakterle 8v8 başlatma: **BEKLİYOR** (F8) | `docs/15` §6a, `docs/04` §3.3 |
| M7a | Kalıcı ölçüm aracı `tools/nav-measure.sh` (+ `nav_measure.cpp`) | F5 | araç | `degerlendirme-2/2026-10-02` dalı | GEREKMİYOR (ana makine ölçümü; MSVC Release kabul ölçümleri ilgili planlarda) | `tools/nav-measure.sh`; çıktılar `degerlendirme-2026-10-02-ek.md` §3 |
| M7b | Bağımsız süpercover oracle `tools/nav-segment-check.py` | F5 | araç | `degerlendirme-2/2026-10-02` dalı | `--selftest` PASS (yapıldı); F5-50/58 çapraz kontrolü: **BEKLİYOR** | `tools/nav-segment-check.py` |

## 3. Güncelleme kuralı

- Plan/kod sütunu her birleştirmede, oyun içi sütun yalnızca kanıt yazılınca güncellenir (kanıt yolu: plan Doğrulama Raporu, `Logs/bots/`, insan test kaydı).
- Bir planın `DOĞRULANDI` olması "oyun içi **YAPILDI**" demek değildir: planın kendi çalışma zamanı kriteri (ör. K10) Claude tarafından koşulduysa yazılır, aksi halde `BEKLİYOR`.
- Yeni değerlendirme planı eklendiğinde (`plans/README.md` "Değerlendirme planları") bu tabloya satır eklenir (mevcut satırlar yeniden düzenlenmez).
