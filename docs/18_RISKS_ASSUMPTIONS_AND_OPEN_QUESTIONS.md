# 18 — Riskler, Varsayımlar ve Açık Sorular

> Durum: Taslak v1.0 · Araştırma tarihi: 2026-10-01 · Referans commit: `0f52027`
> Bu doküman açık soruların (Q-*), risklerin (R-*) ve varsayımların (A-*) tek kaynağıdır. Kod düzeyindeki riskler (R-CODE-*) [02](02_REPOSITORY_ANALYSIS_AND_INTEGRATION_MAP.md) §15'te, mekanik hatalar (MB-*) [03](03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md) §15'tedir.

---

## 1. Proje sahibi kararları (2026-10-01'de verildi)

Aşağıdaki kararlar proje sahibiyle tek tek görüşülerek verildi. Her biri `docs/adr/` altında bir karar kaydına (ADR) bağlıdır. Önerilenden farklı seçilenler **kalın** işaretlidir.

| # | Konu | Verilen karar | ADR | Dokümanlara etkisi |
|---|---|---|---|---|
| K-1 | Botların temsil katmanı | Sunucu içi soketsiz `CUser` (ayrılmış slot + bot alıcısı) | [ADR-0001](adr/ADR-0001-bot-temsil-katmani.md) | 02 §10–11, 13 |
| K-2 | Level 80 ve master sınıf | Bot karakterleri sunucu kapalıyken DB kurulum betiğiyle oluşturulur (level 80, master sınıf, stat, skill, ekipman, NP); betik kuralları doğrular | [ADR-0002](adr/ADR-0002-karakter-kurulum-betigi.md) | 04 §3.3, 13 §4.3, 17 F1–F2 |
| K-3 | Test insanlarının karakterleri | İnsan test hesapları da aynı betikle, aynı stat ve aynı referans ekipmanla hazırlanır | [ADR-0002](adr/ADR-0002-karakter-kurulum-betigi.md) | 15 §7 |
| K-4 | `MAGIC.Etc = 1 → 0` düzeltmesi | Depoda, geri alma adımı olan SQL betiği; her temiz kurulumda uygulanır | [ADR-0003](adr/ADR-0003-magic-etc-duzeltmesi.md) | 17 F1, KI-001 |
| K-5 | Tüketilmeyen potlar (MB-01) | **Olduğu gibi kalır.** Kural insan ve bot için aynıdır: envanterde en az bir adet varsa kullanılabilir, sayı azalmaz. Pot cooldown'u (2 sn) geçerlidir. *(Not, 2026-10-02: ölçülen istemci/bot ortak pot aralığı 2,5 sn, `docs/03` CLI-06; karar değişmedi.)* | [ADR-0009](adr/ADR-0009-tuketilmeyen-potlar.md) | 03 MB-01/CLI-06, 04 §6.5, 11 §2/§6, 01 REQ-NEW-09 |
| K-6 | Test arenası | **Yalnızca arena A** (1274, 890), Karus kapısı açıklığı. Maçlar taraf değiştirerek oynanır; ölüm sonrası dönüş ölçümleri ulus bazında ayrı raporlanır. B yedek aday olarak kayıtta kalır. | [ADR-0004](adr/ADR-0004-test-arenasi.md) | 15 §2.4, 17 F8, 01 REQ-NEW-12 |
| K-7 | Görev kapılı master skill'ler (Etc 510–523) | İlk sürümde kullanılmaz; görevler doğrulanınca "ileri profil" | [ADR-0010](adr/ADR-0010-gorev-kapili-master-skilller.md) | 04 CHR-08, 05 SK-08 |
| K-8 | Sunucu mekanik hataları (MB-*) | Şimdilik olduğu gibi oynanır; kayıt altında tutulur; her düzeltme ayrı kararla | [ADR-0011](adr/ADR-0011-mekanik-hatalar.md) | 03 §15 |
| K-9 | Ranking, ödül ve ölüm duyuruları | **Botlar tamamen normal oyuncu gibi dahil**: NP, altın aktarımı, sıralama, ödül dağıtımı ve zone duyuruları değiştirilmez | [ADR-0012](adr/ADR-0012-ranking-odul-duyuru.md) | 02 S7, 13 §12, 15 ARENA-06, 17 F2, 01 REQ-NEW-14 |
| K-10 | Upstream PR #10 | **Hiç alınmaz** (küçük `break;` düzeltmesi dahil) | [ADR-0013](adr/ADR-0013-upstream-pr10.md) | 17 F0 |

ADR-0005 (bot tick'i IOCP thread'inde) 2026-10-02'de otonom döngüde Claude kararıyla kabul edildi ([adr](adr/ADR-0005-bot-tick-thread-modeli.md); gözden geçirilmeli). ADR-0014 (bot oturumu hesap doğrulamasını ve `SET_LOGIN_INFO`'yu atlar) aynı şekilde kabul edildi ([adr](adr/ADR-0014-bot-oturumu-hesap-dogrulamasi.md); gözden geçirilmeli). ADR-0015 (bot çalışma zamanı komut kanalı: konsol `/bot` + `BotCommands.txt`) aynı şekilde kabul edildi ([adr](adr/ADR-0015-bot-calisma-zamani-komut-kanali.md); gözden geçirilmeli). ADR-0007 (telemetri biçimi: JSONL, sınırlı kuyruk, ayrı yazıcı thread) aynı şekilde kabul edildi ([adr](adr/ADR-0007-telemetri-formati-ve-depolama.md); gözden geçirilmeli). Henüz açık olan teknik kararlar faz içinde verilecektir: ADR-0006 (navigasyon verisi/navmesh), ADR-0008 (L1 yöntemi).

Değerlendirme (2026-10-02) ADR'leri **KABUL** (proje sahibi kararları, 2026-10-02): [ADR-0030-DEG](adr/ADR-0030-DEG-ogrenme-duzeyi.md) (öğrenme: önce rol profili, karakter bazlı kalıcı öğrenme F12), [ADR-0031-DEG](adr/ADR-0031-DEG-senaryo-kazanma-kurali.md) (`killdiff_timed` ve ayrı tür `wipe_first`), [ADR-0032-DEG](adr/ADR-0032-DEG-senaryo-baslangic-yerlesimi.md) (maç öncesi kurulum yerleşimi, neden DB yazımı ve canlı `CUser` tutarlılığı), [ADR-0033-DEG](adr/ADR-0033-DEG-arena-siniri-ve-geri-cekilme.md) (arena modu ve serbest Ronark modu ayrı).

## 2. Varsayımlar

| Kimlik | Varsayım | Durum | Doğrulama |
|---|---|---|---|
| A-01 | Yerel `FDP_kn_online` veri tabanı bu sürümün amaçlanan verisidir (forumdaki Database.7z) | Kabul (kaynak: `start.md`) | — |
| A-02 | Yerel 1453 istemcisi sunucu opcode'larıyla uyumlu | Kısmen gözlendi (giriş ve avlanma logu) | T-ENV-01 |
| A-03 | Botlar yalnızca özel test sunucusunda, onaylı katılımcılarla çalışır | Proje kuralı | — |
| A-04 | İstemci, sınıfa uygun olmayan ekipmanı kuşattırmaz | Açık | Q-05 |
| A-05 | `CastTime` ve `ReCastTime` 0,1 sn birimindedir | Sunucu kodu recast için doğruluyor; cast için açık | T-MECH-CLIENT-03 |
| A-06 | İstemcinin Ronark arazisi sunucu SMD'siyle aynıdır (Zones.tbl adı ters olsa da) | Açık | Q-20 |
| A-07 | Testler tek GameServer süreciyle yürütülür | Proje kuralı | — |
| A-08 | 2005–2008 dönem rehberleri bu sürümün oyun tarzını yansıtır (rol beklentileri) | Kısmi `[S]` | İnsan değerlendirmesi |
| A-09 | Bot karar tick'i 100 ms yeterlidir | Açık | MET-ACT-03 |
| A-10 | Dönem oyuncularının "2 sn pot" ifadesi istemci davranışını yansıtır | Kısmi `[S]` | T-MECH-POT-03 |

## 3. Açık sorular

"Kapanış fazı": sorunun en geç hangi fazın çıkışında cevaplanması gerektiği. Kalın olanlar ilgili fazı **bloke eder**.

| Kimlik | Soru | Neden önemli | Doğrulama yöntemi | Kapanış fazı |
|---|---|---|---|---|
| **Q-01** | Gerçek 1453 istemcisinin R aralığı, skill sonrası R kilidi, cast süresi ve iptal kuralları | Adalet sınırları CLI-01..03 | Paket izleyici + insan oyuncu (T-MECH-CLIENT-01..03) | **F1** |
| **Q-02** | Koşu hızı, `WIZ_MOVE` sıklığı ve hız alanı, `WIZ_SPEEDHACK_CHECK` sıklığı | Hareket adaleti, MEC-MOV-04 toleransı | T-NAV-01, T-MECH-CLIENT-04 | **F1** |
| Q-03 | `start.md`'deki "karakter oluşturma sonrası seviye 51" gözleminin kaynağı (DB varsayılanı 1, prosedür ve kod seviye yazmıyor) | Karakter kurulumu, istemci akışının anlaşılması | İstemciyle yeni karakter oluşturup USERDATA satırını izlemek (yalnızca kendi test hesabı) | F1 |
| Q-04 | Etc 510–523 master questleri bu kurulumda tamamlanabilir mi (114 Lua betiğinde var mı)? | İleri profiller | Quest betikleri ve QUEST_* tabloları (kendi test karakteriyle) | F6 (bloke etmez) |
| **Q-05** | İstemcinin ekipmanda sınıf/ırk kısıtı | CHR-05, referans setler | Her referans item'ı istemcide kuşanma (T-DATA-02) | **F1** |
| Q-06 | HP ve MP potları istemcide ortak zamanlayıcı mı? Pot hareketi durdurur mu? | CLI-06 | T-MECH-POT-03/04 | F1 |
| Q-07 | ~~Pot `UseItem` düzeltmesi kararı~~ — **KAPANDI** (2026-10-01): K-5, veri olduğu gibi kalır | — | — | — |
| Q-08 | AC debuff'ının çift uygulanmasının gerçek etkisi (MB-04) | Debuff değerinin doğru modellenmesi | T-MECH-DMG-02 | F1 — cevaplandı (2026-10-02): çift uygulama gerçek, ölçüm ve kod uyumlu (`docs/03` MB-04); örnek n=4, isteğe bağlı 10+ vuruşla tekrar |
| Q-09 | Yüzde HP buff'ı (Undying) hangi maks HP bileşenine uygulanıyor | Buff matrisi BUF-HP-01 | T-MECH-BUF-03 | F7 |
| Q-10 | İstemci engel arkasına skill/saldırıya izin veriyor mu | `P-NAV-LOS-MODE` | T-NAV-LOS-01 | F5 |
| **Q-11** | Arena A'da gerçekten canavar/NPC yok mu; tower'ların kapı önündeki davranışı | Test geçerliliği | T-ENV-ARENA-01..03 | **F1** |
| Q-12 | Mage Gate (110015) Ronark'ta çalışıyor mu | Mage kaçış seçeneği | T-MECH-T8-02 | F6 — cevaplandı (2026-10-03): evet, çalışır; çağıran ulusun `START_POSITION` 71 noktasına ışınlanır (F4-35 çalışma zamanı ölçümü, `docs/03` MEC-MAG-22 `[V]`) |
| Q-13 | Bifrost zamanlayıcısının (`KickOutZoneUsers`) zone 71'i etkileyip etkilemediği | Test kesintisi | Kod incelemesi + 2 saatlik gözlem | F0 |
| **Q-14** | Soketsiz `CUser` tüm oyun içi kontrolleri geçiyor ve AIServer ile senkron kalıyor mu (guard/NPC'ler botu oyuncu gibi algılıyor mu) | ADR-0001'in fizibilitesi | F2 prototipi | **F2** |
| Q-15 | Sunucu rastgeleliği (`myrand`) test modunda seed'lenebilir mi | Yeniden üretilebilirlik derecesi | Kod incelemesi | F8 |
| Q-16 | Party chat'te Türkçe karakterlerin istemcide görünmesi | Chat protokolü | T-PTY-09 | F7 |
| Q-17 | Confusion'ın mage hasarına etkisi | Debuff önceliği | T-MECH-BUF-08 | F7 |
| Q-18 | Gerçek istemcinin hedef HP isteği sıklığı (`WIZ_TARGET_HP`) | Gözlem sözleşmesi `P-OBS-TARGETHP-RATE` | Paket izleyici | F1 |
| Q-19 | Respawn'dan arenaya yürüme süresi ve summon'un değeri | Summon kararları | T-NAV-05 | F6 |
| Q-20 | İstemcinin Ronark arazisinin sunucu SMD'siyle aynı olup olmadığı (KI-003 adlandırma tersliği) | Botların görsel/hareket tutarlılığı | İstemcide bot yolu izleme | F5 |
| Q-21 | `m_bMaxWeightAmount` başlatma sorunu (MB-12) gerçekte maks ağırlığı etkiliyor mu | Stok politikası | T-DATA-05 | F1 — kod düzeyinde cevaplandı (başlatılmıyor, `docs/03` MB-12); çalışma zamanı değeri ölçülecek; 12 botun ağırlık tablosu F1-05'te |
| Q-22 | Binding/provoke (Type7) sunucu etkisi (MB-10) | W-G kullanımı | T-MECH-SKILL-W | F6 |
| Q-23 | Mage armor yansıma hatası (MB-03) düzeltilsin mi (K-8) | Dengelenme | Karar | F6 |
| Q-24 | Değerlendirme için gerekli maç sayısına ulaşmak üzere paralel sunucu örnekleri çalıştırılabilir mi (ayrı DB/port) | R-10 | Deneme | F8 |
| Q-25 | Priest/mage insan aksiyon hızı ve cast döngüsü dağılımı (heal/cure rotasyonu, toplam aksiyon/sn); CLI-11 6/sn emniyet ağının ve cast sınırlarının insanla eşdeğerliği | Adalet (üç katman, `docs/03` §13.4) | Priest ve mage ile T-MECH-CLIENT-01..04 yeniden (insan) | F6 |
| Q-26 | İstemci suya/göl cebine girebiliyor mu, suda yavaşlıyor mu, kıyı olay ızgarasıyla uyumlu mu (T-NAV-09) | Su katmanı gerekir mi (`docs/12` §13.1) | İstemcide göl kıyısında yürüme + sunucu konum kaydı | F5 |
| Q-27 | `ObsTable` bazı bot çiftlerinde neden tek yönlü görüyor (KI-DEG-01, F4-54) | Karar katmanının girdisi | F4-54 teşhis koşusu | F4 |
| Q-28 | 51–54 quest kimlikleri (32 skill) sunucuda `UseStanding` sütununda: sunucu bu quest'leri uygulamasın mı kalsın, yoksa `Etc`'e taşınıp (`UseStanding` 0) sunucu da uygulasın mı (KI-017 yan etkisi de kalkar) | Bot ve insan eşitliği, T-MECH-SKILL ölçümü, KI-017 | Karar (ADR) + `docs/03` MEC-MAG-14; öneri: mevcut hâl (ADR-0003 quest şartlarını gevşetmişti), bot satırlarına yine de quest yazılır | F4 |

## 4. Riskler

| Kimlik | Risk | Olasılık | Etki | Önlem | İzleme |
|---|---|---|---|---|---|
| R-01 | Thread yarışları ve çökme (kilitsiz oturum haritası, `CUser` başına kilit yok) | Orta | Yüksek | Aksiyonlar IOCP thread'inde (ADR-0005); spawn/despawn kilit altında; T-PERF-04/06 | Çökme sayısı, dump |
| R-02 | Botların sunucu boşluklarından gizli avantaj elde etmesi (aynı saniye recast, denetimsiz hareket, cast süresi) | Yüksek (önlem yoksa) | Yüksek | `BotFairnessGuard`, CLI tablosu, MET-FAIR-01, AC-NAV-03 | Telemetri denetimi |
| R-03 | Veri anomalilerinin testleri çarpıtması (MB-01, LEVEL_UP eksik satırları, Etc) | Yüksek | Orta | K-4, K-5; T-DATA-* | KI listesi |
| R-04 | Ulus asimetrileri (skill menzilleri, arena yakınlığı) | Kesin | Orta | Taraf değiştirme + ulus bazlı raporlama (tek arena, K-6) | Taraf bazlı rapor |
| R-05 | Öğrenmede ödül sömürüsü ve aşırı uyum | Orta | Orta | [14](14_LEARNING_AND_ADAPTATION.md) §7.3, kilitli değerlendirme seti | Guard metrikler |
| R-06 | Çok botla performans (zone yayınlarında oturum haritası kopyalama, 3×3 yayın O(k²)) | Orta | Orta | T-PERF; duyurular açık kalır (K-9), maliyeti ölçülür | MET-PERF-* |
| R-07 | İstemci zamanlamasının ölçülememesi | Düşük–Orta | Orta | Muhafazakâr CLI değerleri | Q-01 |
| R-08 | Kapsam büyümesi (rogue/archer, canlı Ronark, ikmal, transform) | Yüksek | Yüksek | Temel sürüm tanımı ([17](17_IMPLEMENTATION_ROADMAP_AND_PHASE_GATES.md) §3), ADR kapısı | Faz raporları |
| R-09 | 4 m ızgaranın dar geçitlerde yetersizliği | Orta | Orta | Takılma ısı haritası; gerekirse navmesh (ADR-0006) | MET-NAV-01 |
| R-10 | Gerçek zamanlı maçlar nedeniyle değerlendirme kapasitesinin düşüklüğü | Yüksek | Orta | Kısa senaryolar, SPRT ile erken durdurma, paralel sunucu örnekleri (Q-24) | Günlük maç sayısı |
| R-11 | Lisans ve fikri mülkiyet: depo GPLv3; snoxd kökeni lisanssız; istemci/TBL/varlıklar tescilli | — | Yüksek (dağıtımda) | Botlu sürüm dağıtılırsa kaynak GPLv3 ile; istemci ve veri dağıtılmaz; hukuki görüş alınmalı | — |
| R-12 | Veri gizliliği: yedek DB'de üçüncü kişilere ait tablolar var (TB_USER, WEB_ITEMMALL_LOG, PUS_PAYPAL_LOG vb.) | Kesin | Yüksek | Bu tablolar okunmaz/kullanılmaz; test DB'si paylaşılmadan önce bu tablolar temizlenir; bu araştırmada okunmadı | Kontrol listesi |
| R-13 | Upstream değişiklikleri (PR #10 gibi) bot entegrasyonunu bozabilir | Düşük | Orta | Fork'ta upstream birleştirmeleri ADR ile | — |
| R-14 | Bot–insan karşılaştırmasında seviye/ekipman farkı | Orta | Yüksek | K-3: insanlar da aynı betik ve setle | İnsan oturum notu |
| R-15 | AIServer A* hatası (MB-11) ve NPC davranışının arena izolasyonunu bozması | Düşük | Orta | 120 m pay, T-ENV-ARENA-01, NPC bastırma seçeneği | `THIRD_PARTY` olayları |

## 5. Bu araştırmanın sınırları

- Sunucu **çalıştırılmadı**. Tüm davranış iddiaları kod okuması (`[D]`), yerel veri (`[V]`) veya dış kaynaklara (`[S]`/`[B]`) dayanıyor. Çalışma zamanında doğrulama F1'in işidir.
- İstemci `.tbl` dosyaları (ör. `Skill_Magic_Main_us.tbl`) şifrelidir. **2026-10-03: `Skill_Magic_Main_us.tbl` çözüldü** (yuvarlanan XOR, `tools/client-tbl-quests.py`; 31 kolon × 1779 satır, dosya sonuna tam oturur) ve sunucu `MAGIC` verisiyle quest kolonu üzerinden karşılaştırıldı: istemci 104 skill'i 18 quest'e bağlar; 72'si sunucu `Etc`'iyle aynı (510–523), 32'si sunucuda `UseStanding`'de (51–54, KI-017/KI-018, `docs/03` MEC-MAG-14/MB-15). Hâlâ karşılaştırılmayan: diğer kolonlar ve diğer `.tbl` dosyaları (Q-01, Q-05 ile ilişkili).
- ko4life forum konuları (1245, 1258) erişim engeli nedeniyle okunamadı.
- Kalais ve benzeri dönem kaynakları çoğunlukla 2005–2008 tarihlidir ve bazıları JAPKO/MYKO istatistiklerine dayanır. Kullanılan değerler yerel DB ile karşılaştırıldı.
- Kişisel veri içeren tablolar okunmadı; USERDATA'dan yalnızca sütun şeması ve varsayılanlar okundu.

## Değişiklik günlüğü

| Tarih | Sürüm | Değişiklik |
|---|---|---|
| 2026-10-01 | v1.0 | İlk sürüm |
| 2026-10-02 | v1.1 | Değerlendirme: Q-25..Q-27, ADR-0030..0033-DEG bağlantıları |
| 2026-10-03 | v1.2 | Kimlik çakışması giderildi: aynı `Q-27` numarasını iki ayrı soru taşıyordu. İlk verilen (2026-10-02) `Q-27` = `ObsTable` tek yönlü görüş; quest kimlikleri sorusu **Q-28** oldu. F4-27 planı, `docs/reports/gece-2026-10-03*.md` ve diğer eski kayıtlardaki "Q-27" (quest anlamında) ifadeleri Q-28'dir; kapanmış planlar/raporlar tarihsel olduğundan metinleri değiştirilmedi |
