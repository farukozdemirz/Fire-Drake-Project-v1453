# Değerlendirme Raporu — 2026-10-02

> Hazırlayan: Claude (arka plan ajanı, `degerlendirme/2026-10-02` dalı; taban `gece/2026-10-02` @ `3a6b408`, F4-22 birleşmeden önce). Üretim koduna dokunulmadı; düzeltmeler doküman, ADR ve DeepSeek planlarıdır.
> Kaynak: kullanıcının paylaştığı 12 maddelik dış değerlendirme. Her tespit güncel kodla/dokümanla karşılaştırıldı; zaten giderilmiş olan yeniden açılmadı; başlamamış fazların eksikliği mevcut kod hatası sayılmadı.
> Durum: **Bu dosya çalışma sırasında artımlı yazılır** (kullanım limiti nedeniyle yarım kalırsa elde kalsın). Son satırdaki "Tamamlanma durumu" bölümü neyin bittiğini söyler.

## 0. Yöntem ve kanıt sınırları

- Kod ve doküman karşılaştırması: `git show`/`grep` ile `gece/2026-10-02` (F4) ve `gece/2026-10-02-nav` (F5, salt okunur kopya). Çalışma ağaçlarına yazılmadı, sunucu çalıştırılmadı.
- **Ölçümler** (`§5`): F5 başlıkları (`NavGrid/NavPath/NavSmooth/NavTrack/NavDanger.h`, saf standart kütüphane) `gece/2026-10-02-nav` @ `bdd7988`'den kopyalanıp **WSL `g++ -O2`** ile, gerçek zone 71 ızgarasıyla (`tools/nav-export.py`, `freezone_a_20050718.smd`) denendi. Bu **MSVC Release değildir**; süreler göreli büyüklük içindir, kabul ölçümü değildir `[V: WSL g++ -O2]`. Betikler depoya eklenmedi (geçici deneme).
- Çalışma zamanı (sunucu/istemci) gerektiren her şey bir **plan kabul kriteri/insan testi** olarak yazıldı; burada "geçti" denmedi.
- Etiketler: `[D]` kod, `[V]` yerel veri/ölçüm, `[Ö]` öneri, `[A]` varsayım (docs/21 §5).

## 1. Tespit tablosu

Durum sözlüğü: **Doğrulandı** (tespit güncel durumda geçerli), **Zaten giderildi**, **Planlanmış** (ilgili fazda yeterince tanımlı), **Doğrulanamadı**. "Şimdi" = bu çalışmada giderildi; "Faz" = ilgili faz başlamadan plan/doküman.

| ID | Değerlendirme maddesi | Güncel kanıt | Durum | Faz / bağımlılık | Şimdi mi, faz öncesi mi | Doğrulama / kabul |
|---|---|---|---|---|---|---|
| DEG-01 | 1: kapsam ile nihai hedef arası fark (serbest Ronark, çatışma arama, yeniden gruplanma, savaşa dönüş, uzun süreli çalışma) | `docs/01` §1 hedef "gerçek oyunculara ve botlara karşı", §2 "Canlı/halka açık sunucu operasyonu" kapsam dışı; `docs/17` §3 "sonraki"; A-03 yalnızca özel sunucu. Arenadaki regroup/return F7'de, 4 saatlik dayanıklılık F8'de var; serbest dolaşma/çatışma arama için faz yok | **Doğrulandı** (yol haritasında bütün eksik) | F11 (yeni, taslak, ADR kapılı); F8 `baseline-v1` + F9 sonrası | Doküman şimdi (docs/17 F11, docs/01 not); kod yok | F11 giriş/çıkış kriterleri yazılı; A-03 kapsam kararı ADR ile açılmadan F11 başlamaz |
| DEG-02 | 2: mimari doğru ama davranış kalitesi ölçülmeli | Mimari bileşenler doğru (ADR-0001/0005/0017). `docs/17` §4 faz kapısı listesinde "oyun içi davranış kanıtı" maddesi yok; yalnızca build/test/doküman | **Doğrulandı** (kapı eksik) | Tüm fazlar; özellikle F5–F8 | Doküman şimdi: docs/17 §4 + §5 "oyun içi kabul kapıları" | Her faz kapısında G-IGT satırı ve insan/oyun içi kanıt zorunlu |
| DEG-03 | 3: F4-18 `PARTY_INSERT` ad uzunluğu | **Zaten giderilmiş.** `BotCore/Perception.h:921-947` ad `u16` uzunluk + bayt; `PartyHandler.cpp:233-260` `<<`'in `m_doubleByte=true` (`shared/ByteBuffer.h:11`); test `Perception_Party_ParseMember` elle yazılmış 25 baytlık `raw[]` içerir (`PerceptionTests.cpp:1413`) ve `AddPartyMember` yardımcısı artık `u16` yazar; plan Tur 2 DOĞRULANDI, çalışma zamanı S1–S5 (üye adı/sınıf/seviye/HP/MP `list` ile birebir), KAPANDI `a7349a1`. `KNOWN_ISSUES`'ta açık kayıt yok | **Zaten giderildi** | F4 | Yeniden açılmadı. Kalan: `PARTY_HPCHANGE` yalnız MP değişimiyle gözlendi (aynı paket); `raw[]` kod okumasından türetildi, runtime ile eşleşti | Ek iş yok. Gerçek yakalama hex'i isteğe bağlı (düşük öncelik) |
| DEG-04 | 5 (zaman/değişim) ve 3 bağlantılı: F4-18 Tur 2 bulgu 3 | Tur 2 raporu: `ObsTable` bazı bot çiftlerinde **tek yönlüydü** (5 m'deki botu bir yön görüyor, diğer yön görmüyor); "teşhis edilmedi", `KNOWN_ISSUES`'ta kayıt yok | **Doğrulandı** (açık, kayıtsızdı) | F4 (F4-12/F4-13 kapsamı); F6 karar katmanı | Şimdi: KI-DEG-01 + plan F4-54 (teşhis) | Simetrik görüş: 3 botun tüm doğuş sıralarında `see` her yönde aynı kümeyi göstermeli (F4-54 K-listesi) |
| DEG-05 | 4: priest `pending_heals` işareti | `docs/07` §5.1 `hp_pred = u.hp − K·incoming·H − pending_heals`: bekleyen heal **çıkarılıyor**. Kod yok (F7) | **Doğrulandı** (doküman hatası) | F7 (priest koordinasyonu) | **Şimdi düzeltildi**: docs/07 §5.1 `+ pending_heals`, `min(maxHP, …)` | T-PRI-03 + `BotCore` birim testi (F7 planında kabul kriteri olarak yazıldı) |
| DEG-06 | 4: rezervasyon temizliği | `docs/09` §4.2 `Reservation` ömrü "Bitişe kadar"; iptal/ölüm/menzil dışı/tamamlanma temizliği ve çift sayım koruması tanımsız | **Doğrulandı** | F7 | **Şimdi**: docs/09 §4.3 rezervasyon yaşam döngüsü | AC-PRI-03 (çift heal ≤ %5) + T-PRI-03 |
| DEG-07 | 4: HP değişimi ≠ gelen hasar | `docs/07` §2 `incoming_*` "HP değişim akışından"; `PARTY_HPCHANGE` **net** HP taşır (`User.cpp:2057-2065`) | **Doğrulandı** | F7; algı F4-50..52 | **Şimdi**: docs/07 §5.1 `incoming_est = max(0, −ΔHP + etkisi_bilinen_heal)` | T-PRI-02 patlama senaryosu |
| DEG-08 | 5: düşman HP, isim, skill olayları, durum etkileri, zaman içi değişim algı katmanına taşınmamış | `UnitView` yalnızca konum/sınıf/seviye/ölü/oturuyor (`Perception.h:~800`); ad `UnitObs`'ta var ama `UnitView`'a geçmiyor ("client never learns" yorumu yanlış: ad istemciye gelir, `docs/03` §16); `WIZ_TARGET_HP` yalnız eylem sonucu için tek kayıt (`BotSession.cpp:79-90`); `WIZ_MAGIC_PROCESS` yalnız **kendi** cast yankısı (`:60-67`); `WIZ_STATE_CHANGE` yalnız kendi (`:71-79`); birim başına konum geçmişi/hız yok | **Doğrulandı** | F4 kapsamı (`docs/17` F4 "Perception"); F6/F7 karar katmanı bağımlı | **Şimdi planlandı**: F4-50 (meta/ad/geçmiş), F4-51 (düşman HP tablosu), F4-52 (skill olay halkası), F4-53 (gözlenen durum, TASLAK) | Plan kabul kriterleri; oyun içi: `/bot snap` çıktısı sunucu `list` ile çapraz |
| DEG-09 | 5: bilgi kaynak sınıfları ayrı tanımlanmalı | docs/13/14'te yalnız "sözleşme" var; doğrudan gözlem / party'den gelen / tahmin / yalnız-test gerçeği ayrımı yok; `list` komutu sunucu nesnesini okur (test amaçlı) | **Doğrulandı** | F4 (sözleşme), F6+ | **Şimdi**: docs/13 §5.3 kaynak sınıfları (O/P/E/G); F4-50 `src` alanı, statik denetim | AC-LRN-03 genişletmesi: `G` sınıfı yalnız `Eval/test` koduna |
| DEG-10 | 5: görünürlük ≠ son güncelleme; bayatlama; bölge ≠ LoS | `UnitObs.lastSeenMs` "paketin zamanı" (adı yanıltıcı); durağan görünür birim `ageMs` büyür; bayatlama eşiği yok (`EnemyIntel` 10 sn tek yer); `TARGET_LOST_VIS` "3 sn görünmüyor" tanımsız; `docs/03` §16 3×3 bölgeyi LoS yerine koymuyor ama bunu karar katmanına bağlayan kural yok | **Doğrulandı** | F4 + F6 | **Şimdi**: docs/13 §5.3 (üç ayrı kavram: `in_region`, `pos_age`, `los`), F4-50 | `/bot snap` her birim için ayrı `pos_age`/`seen` alanı |
| DEG-11 | 6: buff/debuff/cure/summon/alan/eşya skill desteği hangi fazda | `ActionExecutor.cpp:721-729`: yalnız Type1/Type3, `bType[1]==0`, `bFlyingEffect==0`, `iUseItem==0`, `sEtc==0`; tek hedef adıyla. Mage ana skill'leri (110533 Fire burst) uçan/alan → `unsupported_skill`. `docs/17` hiçbir faza bu aksiyonları atamıyor | **Doğrulandı** (F4'te eksik tek başına hata değil; F6/F7 başlamadan planlı değil) | F6 (mage alan/uçan; priest kendine buff), F7 (buff/debuff/cure/diriltme/summon) | **Şimdi doküman**: docs/17 §2 "aksiyon desteği matrisi" + docs/20 zinciri + docs/13 §8 | Her aksiyon için "oyun içi kabul" satırı (docs/17 §5) |
| DEG-12 | 7: skill + R kilidi | `docs/03` §13 tablosu CLI-02 hâlâ "0,3 sn"; §13.2/13.3 ölçüm **kilit yok** (61/62 ms komşu aksiyon); `docs/05` §5.1(3), §5.2, `docs/06` satır 74 kilidi sürdürüyor; kod kilit uygulamıyor (`BotCombat.h`) | **Doğrulandı** | F6 warrior | **Şimdi doküman** (03 §13.4, 05, 06) | `war-combo` yeniden ölçümü (T-MECH-CLIENT-01, insan) |
| DEG-13 | 7: hareket sıklığı | `docs/12` §6 "ölçülene kadar 250 ms"; ölçüm ~1,5 sn (CLI-05); kod `kMovePeriodMs=1500` (`BotMotion.h:14`) | **Doğrulandı** | F5 | **Şimdi doküman** (12 §6 + §13) | — |
| DEG-14 | 7: pot aralığı | CLI-06 ortak **2,5 sn**; `docs/11` §3.3 "2 sn'de bir pot ile ~960 MP/sn", `docs/05` §5.1(5) "2 sn'de bir", `docs/11` AC-SUR-04 "2 sn grup cooldown", `docs/10` §? "2 sn pencere", `docs/00` §95; ortak sayaçla kapasite 1920/2,5 = 768 MP/sn (HP+MP birlikte yarıya iner) | **Doğrulandı** | F6 pot kararı | **Şimdi doküman** (11, 05, 10, 00; 18 K-5 metnine ölçüm notu) | T-MECH-POT-03 (HP/MP ayrı sayaç mı) |
| DEG-15 | 7: respawn | `docs/09` §9 sözde kodu "u respawns immediately" ↔ CLI-14 `dead_wait` ≥ 3,0 sn (`BotCombat.h:409`, `[A]`); T-REGENE-01 insan ölçümü bekliyor | **Doğrulandı** | F6/F7 | **Şimdi doküman** (09 §9, 03 CLI-14 notu) | T-REGENE-01 |
| DEG-16 | 7: aksiyon sınırı gerekçesi | CLI-11 6/sn toplam (`BotCombat.h:14`); ölçülen insan ≤ 3/sn tek karakter/tek oturum; sunucu: R ≤ 1/sn, Type1 ≤ 1/sn, pot kendi recast'i; "insanı aşmaz" gerekçesi yanlış (6 > 3). Tek toplam sınır skill/R/pot zamanlamasının insanla eşdeğerliğini kanıtlamaz | **Doğrulandı** | F4 kapısı, F6 | **Şimdi doküman**: docs/03 §13.4 üç katman (ölçülen / sunucu / bot sınırı); Q-25 | Priest/mage insan ölçümü (T-MECH-CLIENT-01..04 yeniden) |
| DEG-17 | 8: takılma tespiti paket sıklığıyla | `docs/12` §10 "1,5 sn ilerleme < 1 m", "4 sn'de ≥ 3 salınım"; konum gözlemi paket başına (~1,55 sn); salınım ölçütü paket hızında **ulaşılamaz** (4 sn'de ≤ 3 örnek); 1,5 sn pencere paket aralığına eşit (marj 0). Simülasyon (§5.4): kendi zamanlamasıyla yanlış alarm **üretmedi** (0/5000+ tick); yani sorun ölçülmüş yanlış alarm değil, tanım hatası | **Doğrulandı** (kısmen: yanlış alarm ölçülmedi, tanım gerekçesi geçerli) | F5 (nav loop F5-09 öncesi) | **Şimdi**: docs/12 §13.3 tanım; plan F5-54 (`NavStuckDetector`, algı/niyet ayrımı) | Birim test + F5-55 çalışma zamanı T-NAV-04 |
| DEG-18 | 8: segment/çapraz köşe/su/eğim; yalnız bitiş noktası | Bot hareketi şu an düz hedef adımı (`TickMove`/`StepToward`, 6,75 m/paket yürüyüş, ~10 m sprint) ve **yürünebilirlik denetimi yok** (CLI-08 F5 entegrasyonu bekliyor). Planlayıcı çıktısı: 998 yol, 4887 düzleştirilmiş segment ve 33 503 paket kirişi, tümü muhafazakâr süpercover testinden geçti (0 ihlal) `[V: WSL g++]`. Çapraz köşe `EdgeOpen`'da var; eğim `maxSlope 0,625 [A]`; **su** için ayrı katman yok (göl kıyıları olay ızgarasında engelli `[V/I]`, istemci davranışı doğrulanmadı) | **Kısmen doğrulandı**: planlayıcı güvenli, **icra katmanında koruma yok** | F5 | **Şimdi planlandı**: F5-50 (kiriş denetimi `NavSegmentWalkable`), F5-55 (TASLAK, sunucu entegrasyonu + guard) | AC-NAV-03 (engelli hücreye giren hareket = 0) çalışma zamanında; T-NAV-02 (eğim), T-NAV-09 (su/kıyı, yeni) |
| DEG-19 | 8 (dolaylı, yeni bulgu): hareketli hedef hız kestirimi paket sıklığıyla çalışmıyor | `NavTrack.h` `Velocity`: pencere 1000 ms; hedef gözlemleri ~1,5 sn aralıklı ise hız **her zaman 0** (§5.2: 1500/1540 ms'de %100 sıfır, 500/1000 ms'de %0) | **Doğrulandı (yeni)** | F5 (F5-04'ün varsayımı ölçümle çelişiyor) | **Şimdi planlandı**: F5-52 | Birim test: 1,5 sn aralıklı sabit hızlı hedefte kestirim ≠ 0 ve hata ≤ %10 |
| DEG-20 | 8: ölüm/respawn/savaşa dönüş ↔ arena sınırı | `AddForbidOutsideDisc` + `forbiddenPenalty 10`: El Morad doğuşu→arena merkezi **`NodeLimit`** (20 000 düğüm, yol bulunamıyor); Karus 4148 düğüm; arena içinden dışarıdaki doğuş noktasına hedef `InvalidGoal` (§5.3). `docs/11` §4.3 solo geri çekilme "kendi tower halkasına" ↔ `docs/12` §7 "arena dışına yol planlamaz" çelişkisi | **Doğrulandı** (ölçüm) | F5; F6/F7 geri çekilme | **Şimdi planlandı**: F5-51; doc çelişkisi docs/12 §13.4 + ADR-0033 | Birim test: iki ulus doğuşu → arena `Found`, düğüm ≤ 6000; arena içi geri çekilme sınır içinde |
| DEG-21 | 8: çoklu bot A* toplam tick bütçesi | §5.5: 16 bot aynı tick'te near64: tick toplamı p95 2,83 ms (p99 4,5, max 7,2); mid150: p95 8,8 ms; tüm harita: p95 30 ms; 64 bot near64: p95 9,5 ms. MET-PERF-02 toplam hedefi 16 bot p95 ≤ 5 ms **tüm** BotManager için | **Doğrulandı** (bütçe stratejisi tanımsız) | F5 → F6 | **Şimdi planlandı**: F5-53 (`NavBudget`: tick başına bütçe, kuyruk, sıra dönüşümü, yol önbelleği) | Birim test: 16 bot × yoğun sorguda tick toplamı ≤ bütçe, yönlendirme adil |
| DEG-22 | 9: 8v8 için 16 karakter | `BOT_TABLE` 12 sabit giriş (`BotManager.cpp:60-66`); `db/002` 12 karakter; `MAX_BOTS=16` yalnız slot havuzu; C8-A bir ulusta 2 W-P + 1 W-G + 2 priest + 2 M-F + 1 M-I = 8 → ulus başına 8 (toplam 16); C8-B/C için ulus başına 3 W-P/3 M-F | **Doğrulandı** | F8 (ön koşul: F7 sonu) | **Şimdi doküman**: docs/04 §3.3, docs/17 F8, docs/15 §6a | `db/003` + `BOT_TABLE` genişletme planı (F8 ön koşulu); 20 karakter set doğrulaması |
| DEG-23 | 9: senaryo başlangıcı sıfırlama | `ScenarioRunner` yalnız `bots/seeds/repeat/duration_sec/script` (`ScenarioRunner.cpp:~188-475`); envanter doldurma "henüz yok" (docs/13 §10); HP/MP/NP/konum DB'de kalıcı (bot çıkışında kayıt, AC-ARCH-05); KI-013 (NP 0 → regene yok) | **Doğrulandı** | F8 (kısmen F6'da tek bot testleri için) | **Şimdi doküman**: docs/15 §6a sıfırlama sözleşmesi + başlangıç doğrulaması, ADR-0032 | `SETUP_FAIL` ile geçersiz maç; maç öncesi doğrulama listesi |
| DEG-24 | 9: kazanma koşulu | `MATCH_END.result` yalnız `completed`/`aborted` (docs/16 §3.3); `docs/16` MET-OUT-01 "senaryo hedefine göre" tanımsız; süre dolması = tamamlanma | **Doğrulandı** | F8 (EVAL) ve F6/F7 küçük senaryolar | **Şimdi doküman**: docs/15 §6b, docs/16 MET-OUT-05, ADR-0031 | `win_rule` senaryo anahtarı (F8 planı); `completed ≠ win` |
| DEG-25 | 10: öğrenme düzeyi (rol profili vs karakter) | `docs/14` §6: politika rol profili düzeyinde, karakter düzeyinde **tutulmaz**; ama proje hedefi "deneyimlerinden gelişen botlar" ve bu ayrım proje sahibi kararı olarak kayıtlı değil | **Doğrulandı** (belirli ama onaysız) | F9/F10 | **Şimdi**: ADR-0030-DEG (rol profili düzeyi + oturum içi kestirim; karakter kalıcı öğrenme yok), docs/14 §6.1 | — (proje sahibi onayı bekler) |
| DEG-26 | 10: F9 sonucu "iyileşme yok" olabilir; öğrenme başarısı ayrı; geçersiz yükleme reddi ≠ kötü politika geri alma | `docs/17` F9 çıkış zaten "iyileşme yok kanıtı" geçerli; ama AC-LRN-05 örneği "geri çekilme eşiği %95" **aynı dokümanda** AC-LRN-04 ile yüklemede reddedilir (aralık %20–40): geri alma testi hiç çalışmaz | **Doğrulandı** (iç çelişki) | F9 | **Şimdi doküman**: docs/14 AC-LRN-05 geçerli-ama-kötü örnek, AC-LRN-07/08 | Birim testi (reddetme) ve canary geri alma (ayrı) |
| DEG-27 | 11: metrikler rol bilinçli, boşta kalma, kaçırılan fırsat, başarısız dönüş; öğrenme/değerlendirme ayrımı | `docs/16` §6: MET-TGT-03 payda "fiilen katılabilecek" ama kritik heal atan priest hariç tutulmuyor; kaçırılan kritik heal/cure fırsatı, boşta süre, başarısız savaşa dönüş, stall kararı metrikleri yok. `docs/14` §9 eğitim/doğrulama/kilitli küme ayrımı **var**; değerlendirme setine erişim denetimi yok | **Kısmen doğrulandı** (ayrım var, rol-bilinçli yorum ve bazı metrikler yok) | F6–F8 | **Şimdi doküman**: docs/16 §6.9 (MET-ROLE/IDLE/…), docs/14 §9 erişim bütçesi | Faz kapısında metrik tablosu ve insan formu |
| DEG-28 | 12: faz sırası, somut kabul | `docs/17` F6 tek faz (warrior+priest+mage solo), F7 tek faz; faz içi alt kapı ve oyun içi kabul yok | **Doğrulandı** | F5–F8 | **Şimdi doküman**: docs/17 alt kapılar G5/G6a..G8, docs/15 §4.9 oyun içi kabul testleri | Her alt kapı için oyun içi senaryo ve kanıt türü |
| DEG-29 | (ek, tutarlılık) docs/13 §5.2 notunda F4-17/F4-18 "HAZIR" kalıntısı; STATUS "Faz tablosu" plan listesi gibi kullanılmış | `docs/13` §5.2 not: iki "HAZIR" cümlesi; `docs/STATUS.md` faz tablosu satırları plan sonuçları | **Doğrulandı** | — | **Şimdi**: docs/13 not temizliği; STATUS'a "Faz kabul takibi" (yalnız ekleme) | Bireysel işlerin kapanması ≠ faz kabulü ayrı izlenir |

## 2. Şimdi yapılan düzeltmeler

(artımlı doldurulur; commit listesi §7)

## 3. Plan eşlemesi (hangi madde hangi plana)

(artımlı doldurulur)

## 4. Açık kararlar ve bağımlılıklar

(artımlı doldurulur)

## 5. Ölçümler (WSL `g++ -O2`, geçici betikler, depoda yok)

### 5.1 Segment/kiriş yürünebilirlik (DEG-18)

998 rastgele near64 yol (Chebyshev ≤ 64 hücre, tohum 20261002), `NavSmoothPath` çıktısı; her segment ve her 6,75 m'lik paket kirişi için hücre kenarlarına dokunan **tüm** hücreler (muhafazakâr süpercover, köşe/vertex dokunuşu dahil) `Walk` mi? Sonuç: 4887 segment, 33 503 kiriş, ihlal 0. Yorum: planlayıcının çıktısı güvenli; ama (a) `/bot move` gibi düz hedef adımları ve (b) gelecekte NavService dışından gelen her paket için **icra tarafında** bu denetim yok (CLI-08).

### 5.2 Hız kestirimi (DEG-19)

`NavTargetTracker::Velocity` (pencere 1000 ms, `minSpan` 100 ms), sabit hızlı hedef (4,5 m/s), gözlem aralığı 500/1000/1500/1540 ms: sıfır hız oranı %0 / %0 / **%100** / **%100**.

### 5.3 Arena sınırı ve doğuş yolu (DEG-20)

`NavCostLayer::AddForbidOutsideDisc(1274, 890, 60)` + varsayılan `NavCostParams`:

| Sorgu | Alan yok | Arena sınırı | Yalnız takım bölgeleri |
|---|---|---|---|
| Karus doğuşu → arena merkezi | Found, 406 düğüm, 271,5 m | Found, **4148** düğüm, 0,55 ms | Found, 602 |
| El Morad doğuşu → arena merkezi | Found, 2897 düğüm, 727,8 m | **NodeLimit (20 000 düğüm), 2,7 ms** | Found, 2869 |
| Arena merkezi → Karus doğuşu | Found | **InvalidGoal** (dışarısı yasaklı) | Found |

Neden: başlangıç yasaklı bölgedeyken her adım `(1 + 0,5·(10+10)) = 11×` maliyetli, sezgisel ise birim maliyetli → A* ~11× fazla düğüm genişletir.

### 5.4 Takılma tespiti zamanlaması (DEG-17)

Bot, `elapsed ≥ 1500 ms` olan ilk tick'te paket gönderir (`TickMove`). Tick 100±0/±10 ms ve 110,8±20 ms (+%3 olasılıkla +250 ms gecikme) simülasyonunda "son paketten bu yana ≥ pencere" koşulu 1500/2000/3000/3500 ms pencerelerde hiç oluşmadı (0/~5000 tick): kendi zamanlamasından yanlış alarm yok. Ama 1,5 sn pencere paket aralığına eşittir (marj sıfır); sunucuda reddedilen/geciken paket, `elapsedMs` kırpması ve waypoint'te kısalan adım tespiti bozabilir; 4 sn'de ≥ 3 salınım ölçütü ≤ 3 konum örneğiyle sağlanamaz.

### 5.5 Çoklu bot sorgu bütçesi (DEG-21)

Zone 71, `NavPathfinder::Find`, aynı tick'te N bot tek sorgu (süreler tek iş parçacığı):

| Sorgu seti | Bot/tick | Sorgu p50 / p95 / p99 / max (ms) | Tick toplamı p50 / p95 / p99 / max (ms) |
|---|---|---|---|
| near64 | 16 | 0,046 / 0,389 / 0,665 / 4,28 | 1,43 / 2,83 / 4,51 / 7,24 |
| mid150 (≤ 150 hücre) | 16 | 0,253 / 1,158 / 1,846 / 3,35 | 5,91 / 8,82 / 10,20 / 11,27 |
| tüm harita | 16 | 0,967 / 3,837 / 4,312 / 5,73 | 20,8 / 30,2 / 33,0 / 38,7 |
| near64 | 64 | 0,045 / 0,396 / 0,693 / 3,23 | 6,26 / 9,49 / 12,16 / 12,92 |

`expanded` p95: near64 2567, mid150 6444, tüm harita 20 000 (düğüm sınırı). Bütçe: MET-PERF-02 16 bot için p95 ≤ 5 ms (tüm BotManager).

## 6. Doğrulanamayanlar / çalışma zamanı gerektirenler

(artımlı doldurulur)

## 7. Commit listesi

(sonda doldurulur)

## Tamamlanma durumu

- [x] §1 tespit tablosu, §5 ölçümler
- [ ] §2–§4, §6 (artımlı)
- [ ] Doküman düzeltmeleri, ADR'ler, planlar, takip kayıtları
