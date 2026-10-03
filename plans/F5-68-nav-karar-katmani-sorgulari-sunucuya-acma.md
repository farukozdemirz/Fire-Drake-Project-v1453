# F5-68: Navigasyon sorgularının karar katmanına açılması: doğrudan yürünebilirlik (`directClear`), geri çekilme noktası (`NavRetreatPlanner`), ulaşılamaz hedef hükmü (`NavReachJudge`), arena sınırı katmanı

| Alan | Değer |
|---|---|
| Durum | TASLAK |
| Faz | F5 — Navigasyon (`docs/17` §2; kapı G5) |
| Branch | `bot/F5-68 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | **F5-62** (`NavService` yol bulma/`NavPathfinder`), **F5-63** (takip durumu), **F5-64** (bütçe), **F5-65** (durum temizliği) `KAPANDI` olmalı; F5-50 (`NavSegment`), F5-58, F5-07 (`NavRetreat.h`), F5-05 (`NavReach.h`), F5-06 (tehlike/yasaklı alan), F5-51 (arena sınırı ve doğuş yolu) `KAPANDI` |
| İlgili gereksinim / kabul | `docs/12` §13.4 (arena modunda geri çekilme arena içinde; ADR-0033-DEG), `docs/11` §4 (geri çekilme), `docs/06` §7 (yaklaşma); F6-03 ve F6-05'in nav girdi sözleşmesi (`plans/F6-03-...md` §3: `nav.targets[i].directClear`; `plans/F6-05-...md` §2: `NavRetreatPlanner::Find`) |
| Tahmini büyüklük | M (tahmin; yazım turunda F5-62..F5-65 koduna göre ≤ 10 dosyaya sığdırılır, gerekirse ikiye bölünür) |
| Hazırlayan / tarih | Claude / 2026-10-03 (iskelet; planlama turunda bulunan boşluk) |

---

## 1. Neden bu plan var (planlama turunda bulunan boşluk)

`BotCore` içinde yazılı ve birim testli olan üç karar girdisi **hiçbir F5 diliminde sunucuya açılmıyor** (F5-59..F5-66 dilim tablosu `plans/F5-55-...md` §1A; F6 planlama ajanının bulgusu, `docs/reports/plan-bagimlilik-F6-2026-10-03.md`):

1. **Doğrudan yürünebilirlik (`directClear`):** F6-03 (warrior yaklaşma) hedefe yakınken (`dist <= warNavHandoffM`) nav yoluna mı yoksa düz adıma mı geçeceğini, "bot ile hedef arası düz kiriş yürünebilir mi" bilgisiyle seçer. Bu bilgi `NavSegment` kiriş denetiminin (F5-50/F5-58) karar katmanına verilen sonucudur; **hedef başına bir bool** olarak sunulması hiçbir dilimde yazılı değil. (Terim F6 planlarının girdi alanıdır; `BotCore`'da bu adla bir fonksiyon **yoktur**: tanım yazım turunda `NavLineClear`/`NavCheckStep` ile eşlenir.)
2. **Geri çekilme noktası:** `BotCore/NavRetreat.h` (`NavRetreatMode`, `NavRetreatQuery`, `NavRetreatResult`, `NavRetreatPlanner::Find`) F5-07'de yazıldı ve birim testli; sunucudan **çağrılmıyor**. F6-05 (HP < %30 geri çekilme) güvenli noktayı bundan alır.
3. **Ulaşılamaz hedef hükmü:** `BotCore/NavReach.h` (`NavReach`, `NavReachJudge::Judge(grid, reach, plan, ...)`, `NavUnreachTracker::Feed`) F5-05'te yazıldı; sunucuya iletilmiyor. Karar katmanı "hedefe yolum yok" bilgisini buradan almalı (aksi halde bot ulaşamayacağı hedefin peşinde dolaşır).

Ek bulgular (F5-61..F5-66 taslak yazım turu): arena sınırı katmanının (`AddForbidOutsideDisc`, F5-51) sunucuda **hangi senaryo/modda** kurulacağı ve `NavFormation`/`NavLos` bağlaması da hiçbir dilimde yok; `NavFormation`/`NavLos` F7 ihtiyacıdır, bu planın kapsamı **değildir** (aşağıda "kapsam dışı").

## 2. Kapsam (iskelet; kesin liste yazım turunda)

1. `NavService`'e karar katmanı sorguları (F5-62/F5-64'ün yol bulma ve bütçe arayüzünün **üstüne**, onları değiştirmeden): `DirectClear(botSlot, targetPos)` (kiriş yürünebilir mi), `FindRetreat(botSlot, query)` (`NavRetreatQuery` → `NavRetreatResult`), `JudgeReach(botSlot, plan)` (`NavReachJudgement`). Sorgular bütçeyi (`P-NAV-TICK-BUDGET-MS`) ve IOCP tek-thread kuralını bozmaz.
2. Arena modunda (`ScenarioRunner` senaryosu `arena:` tanımlıyken) `AddForbidOutsideDisc` katmanının kurulması; serbest modda kurulmaz (ADR-0033-DEG).
3. Birim testleri (saf mantık zaten testli; yeni testler yalnızca sunucu tarafı sarmalayıcının sınır koşulları) ve çalışma zamanı doğrulaması (Claude): hedefe yakın engelli/engelsiz iki konumda `DirectClear` değerinin `NavSegment` oracle'ı (`tools/nav-segment-check.py`) ile aynı olması; HP düşürülmüş botta `FindRetreat` noktasının arena içinde ve tehlike yarıçapı dışında olması; ulaşılamaz hedefte `JudgeReach` hükmü.

## 3. Kapsam dışı

- `NavFormation` (yığılma önleme) ve `NavLos` (görüş hattı, advisory) bağlaması: F7 ihtiyacı, ayrı plan.
- Karar mantığının kendisi (geri çekilme eşiği, yeniden giriş: F6-05; yaklaşma: F6-03).
- Su katmanı (F5-60/F5-67).

## 4. Dokunulabilecek dosyalar

Yazım turunda F5-62..F5-65 sonrası `GameServer/Bot/NavService.{h,cpp}` ve ilgili test dosyası; yeni `BotCore` kodu beklenmez (mevcut başlıklar kullanılır). Tahmini ≤ 5 dosya.

## 5. Neden TASLAK

1. **Temel kod yok:** `NavService` yol bulma/bütçe arayüzü F5-62/F5-64'te yazılacak; bu sorgular onun üstüne eklenir. Arayüz adları ve imzaları o dilimler `KAPANDI` olmadan kesinleşmez.
2. **HAZIR yapma ön koşulu:** F5-62, F5-63, F5-64, F5-65 `KAPANDI`. Yazım turunda yeniden doğrulanacak referanslar: `BotCore/NavRetreat.h:28-108` (`NavRetreatPlanner::Find`), `BotCore/NavReach.h:48-110` (`NavReachJudge::Judge`, `NavUnreachTracker::Feed`), `BotCore/NavSegment.h` (kiriş denetimi), `BotCore/NavDanger.h` (`AddForbidOutsideDisc`), F6-03 §3 ve F6-05 §2'deki girdi alanı adları.
3. **Karar:** `directClear` tanımı (kesin kiriş mi, muhafazakâr süpercover mı): F5-50'nin muhafazakâr denetimi esas alınır; F6-03'ün eşiği (`warNavHandoffM`) buna uyarlanır.

## 6. Kabul kriterleri (taslak)

- [ ] `./tools/build.sh Release` ve Debug rc=0; `./tools/run-tests.sh` `0 failed`; `[BOT] ENABLED=0` ve `NAV=0` davranışı değişmez
- [ ] Çalışma zamanı (Claude): §2 madde 3'teki üç doğrulama; `DirectClear` ↔ `nav-segment-check.py` farkı 0
- [ ] **Durum ayrımı:** kod uygulandı ≠ sunucuda doğrulandı ≠ insanla doğrulandı; faz kabulü ayrı

## Uygulayıcı Raporu (DeepSeek doldurur)

(boş)

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(boş)
