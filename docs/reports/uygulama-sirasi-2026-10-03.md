# Uygulama sırası ve plan numaraları (2026-10-03)

> Okuyan: proje sahibi. Yazan: Claude. Bu belge **sıralamanın** kaynağıdır; her planın içeriği kendi dosyasında, durumu `plans/README.md`'dedir. Çelişirse plan dosyası ve README geçerlidir.

## 1. Hedef ve ilke

**Ana hedef:** mevcut aksiyon (F4) ve navigasyon (F5) bileşenlerini oyunda **birlikte** çalıştırmak; ardından kendi kararını veren bot davranışları (F6/F7). İş seçimi bu hedefin bağımlılıklarına göredir. Öğrenme (F9/F12) ve serbest Ronark (F11) kodu bu sırada **yoktur**; yol haritasında kalır.

**Durum ayrımı (zorunlu):** her iş dört ayrı durumla izlenir ve biri diğerinin yerine yazılmaz: (1) kod uygulandı (plan `KAPANDI`), (2) sunucuda doğrulandı (Claude, gerçek sunucu + bot), (3) insan istemcisiyle doğrulandı (proje sahibi), (4) faz kabulü (`KABUL_EDILDI`, yalnızca proje sahibi). Birim testi geçen ama `GameServer`'dan çağrılmayan modül oyun davranışı olarak kapatılmaz. Tablo: `docs/reports/degerlendirme-takip.md`.

## 2. Kesin sıra (kuyruk sırası = uygulama sırası)

| Sıra | Plan | İş | Plan durumu (2026-10-03) | Önkoşul |
|---|---|---|---|---|
| 0 | F4-53, F4-60 | Gözlenen durum tablosu (saf mantık) ve sunucu bağlaması | `KAPANDI` | — |
| 1 | **F5-59** | `NavService`: yaşam döngüsü, SMD belleğinden ızgara, `[BOT] NAV=1`, CRC32 parmak izi | HAZIR (kuyrukta, döngüde) | — |
| 2 | **F5-60** | Su ve engel verisi denetimi (çevrimdışı), karar kuralı | HAZIR (kuyrukta) | — (F5-59'dan bağımsız) |
| 3 | **F4-55** | Bot giriş el sıkışması serileştirmesi (KI-DEG-01 kalıcı düzeltme) | HAZIR (kuyrukta) | — |
| 4 | F5-61 | Gerçek hareket icrasında kiriş denetimi (CLI-08 `blocked_chord`) | TASLAK | F5-59 |
| 5 | F5-62 | `/bot goto`: nav yolu üzerinden ilerleme | TASLAK (12 dosya: HAZIR yapılırken bölünür) | F5-59, F5-61 |
| 6 | F5-63 | Hareketli hedef: kestirim, yeniden yol, takılmadan kurtulma | TASLAK | F5-62 |
| 7 | F5-64 | Sorgu bütçesi adil dağıtımı, ertelenen sorgu, NAV telemetrisi, `PERF_SAMPLE` nav payı | TASLAK | F5-62, F5-63 |
| 8 | F5-65 | Ölüm, respawn, despawn, bölge değişiminde nav durumu temizliği | TASLAK | F5-62 |
| 9 | **F5-68** | F6'nın ihtiyaç duyduğu ama hiçbir F5 diliminde sunucuya açılmayan sorgular: `directClear`, `NavRetreatPlanner`, `NavReachJudge` iletimi, arena sınırı katmanı | TASLAK (iskelet; F5-62..F5-65 kodu gelince detaylanır) | F5-62..F5-65 |
| 10 | **F4-61** | *(yazılacak)* Gözlenen durum: görünürlük kaybı, yeniden giriş, `uncertain` işareti, ±2 sn ölçüm | **plan dosyası yok** (kaynak taslak: `docs/reports/taslaklar/`) | F4-60 |
| 11 | F5-66 | Oyun içi çalışma zamanı doğrulama: engelli rota, iki ulus doğuş → arena, takip/takılma, 16 bot nav payı ve `BotManager::Tick`, ertelenen sorgu | TASLAK | F5-59..F5-65, F5-68, F4-55 |
| — | F5-67 | Su katmanı düzeltmesi | TASLAK, **koşullu** (yalnız F5-60 sonucu `GEREKLİ` ise) | F5-60 |
| 12 | F8-05 | `ScenarioReset`, başlangıç yerleşimi, `SETUP_FAIL` (KI-DEG-05) | TASLAK | — (sunucu işi; F6-10 resmî EVAL-1v1 için gerekli) |
| 13 | **[hat `f6`, ana hattan ayrı: F6-01..F6-05 ikinci döngüde]** F6-01 → F6-02 ∥ F6-04 → F6-03 → F6-05 → F6-06 | Karar katmanı iskeleti + B0-NAIVE, hedef seçimi, pot, warrior yaklaşma/saldırı, geri çekilme, **warrior solo uçtan uca** (T-IGT-WAR-01) | TASLAK (F6-06: 11 dosya, bölünür) | F6-01..05 saf mantık nav'ı beklemeden yazılabilir; **F6-06 F5-59..F5-66 ve F4-55'i bekler** |
| 14 | F6-07, F6-08 → F6-09 → F6-10 | Priest heal, mage saldırı, priest solo destek, solo EV ve EVAL-1v1 | TASLAK | F6-06 (G6a) |
| 15 | F7-01 → F7-02 → F7-03 → F7-04 → F7-05 → F7-06 → F7-07 | TeamBlackboard + rezervasyon + `pending_heals`, iki priest koordinasyonu, ortak hedef, debuff çağrısı, mage patlama + güvenli summon, buff/cure, diriltme | TASLAK (F7-08 sunucu bağlama **yazılmadı**) | F6 kabulü |
| 16 | F8-03 → F8-04 ∥ F8-06 → F8-07 | 16 bot karakteri (`db/005`), +4 çeşitlilik (`db/006`), sunucu maç olayları, kazanma kuralları | TASLAK | F8-03/04 yeni karakter betiği; F8-06/07 F8-05'ten sonra |

**Plan sırası nasıl işler:** `plans/.queue` HAZIR planları sırayla verir (şu an F5-59 → F5-60 → F4-55). Kuyruk bitince döngü `docs/STATUS.md` "Sıradaki adımlar"daki bu sıraya göre sıradaki TASLAK planı `/plan-olustur` ile **HAZIR yapar**: referansları o günün koduna göre yeniden doğrular (taslaklar bu yüzden somut ama "yazım turunda yeniden doğrulanacak" bölümü taşır). Bağımlılığı `KAPANDI` olmayan plan HAZIR yapılmaz. F6/F7/F8 planları **topluca uygulanmaz**; sırası gelince tek tek.

**Sıra gerekçesi (kısa):** F5-59 diğer tüm nav dilimlerinin zeminidir. F5-60 sunucusuz ve bağımsızdır (su kararı F5-61/F5-62 kiriş ve yol işini etkiler, bu yüzden erken). F4-55 çoklu bot ölçümlerinin (F5-66, F6-06) güvenilirliği için gerekir, nav'dan bağımsızdır. F5-68 ve F4-61 F5-66'dan önce kapanmalı, çünkü F6'nın geri çekilme ve yaklaşma davranışları o sorgulara dayanır. F8-05 F6'dan önce, çünkü resmî EVAL-1v1 ölçümü durum sıfırlaması olmadan güvenilir değildir (`KI-DEG-05`).

## 3. Bu planlama turunda bulunan boşluklar ve çelişkiler

1. **F5 dilim boşluğu:** `NavRetreatPlanner` çağrısı, `NavReachJudge` iletimi ve `directClear` sorgusu hiçbir F5 diliminde sunucuya açılmıyor, oysa F6-03 ve F6-05 onlara dayanıyor → **F5-68** (sıra 9).
2. **Geri çekilme eşiği (Q-29):** gereksinim "HP %30" ile `docs/11` formülü çelişiyor; F6-05 `threatWeight = 0` ile (yani sabit %30 taban) planlandı. Onayına sunuldu (`docs/18` Q-29).
3. **`B0-NAIVE` ve "anlamlı yener" (Q-30):** tanımsız; F6-01 geçici tanım `[A]` yazar, F6-10 öncesi onayın gerekir.
4. **Summon:** `docs/17` summon'u F7'ye koyuyor; F6-08 summon'u dışarıda bırakır, güvenli summon F7-05'tedir.
5. **`db/003` ad çakışması:** giderildi; 16/20 karakter betikleri `db/005` ve `db/006` (`db/README.md` notu). F8 planları bunu kullanır.
6. **F8 bulguları** (`docs/reports/plan-bagimlilik-F8-2026-10-03.md`): `MATCH_END.result` değerleri ADR, araç ve sunucu arasında tutarsız (`bot_lost`/`aborted`/ADR kodu); süre tabanı farklı (sunucu maç açılışından, ADR ve araç `engage`'den sayıyor); `docs/15` §6a "buff çıkışta biter" iddiası yanlış (`USER_SAVED_MAGIC` kalıcı) ve ekipman dayanıklılığı (`ItemWoreOut`) listelenmemiş; `DEATH`/`DAMAGE` varsayılan `TELEMETRY=summary`'de yazılmıyor.
7. **`plans/F8-02` ve diğer kapanmış kayıtlar** değiştirilmedi; ölçüm çelişkileri `docs/12` §13.5.2 madde 10 ile yönlendirilir.
8. **Plan boyutu:** F5-62 (12 dosya), F6-06 (11), F8-05, F8-06 `~10 dosya` sınırını aşar; HAZIR yapılırken bölünmeleri planlarında yazılı.
9. **Ölçüm:** gerçek `BotManager::Tick` (MET-PERF-02) hedef yükte hiç ölçülmedi; 16 bot için 16 karakter gerekir (bugün 12, `db/005` F8-03) → F5-66'nın 16 botluk kısmı ya F8-03'ü bekler ya 12 botla kısmi ölçüm + 16 için ertelenmiş madde olarak yazılıdır.

## 4. İlgili belgeler

- Karar seçenekleri (Q-28 ve ADR'ler): `docs/reports/karar-secenekleri-2026-10-03.md`
- Senin yapman gereken testler (öncelik sırasıyla): `docs/reports/insan-testleri-oncelik-2026-10-03.md`
- Bağımlılık özetleri: `docs/reports/plan-bagimlilik-F6-2026-10-03.md`, `...-F7-...`, `...-F8-...`
- Skill türü sınıflandırması: `docs/adr/ADR-0018-...` Ek 28 (F6'ya geçişi engelleyen skill türü **yok**)
- Ölçüm matrisi: `docs/12` §13.5.1-13.5.3
- Durum özeti: `docs/reports/durum-ozeti-ve-dogrulama-listesi-2026-10-03.md`
