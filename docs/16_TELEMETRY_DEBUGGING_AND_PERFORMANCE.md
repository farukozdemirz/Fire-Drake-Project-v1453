# 16 — Telemetri, Hata Ayıklama ve Performans

> Durum: Taslak v1.0 · Araştırma tarihi: 2026-10-01 · Referans commit: `0f52027`
> Bu doküman **metrik kimliklerinin (MET-\*) ve karar logu şemasının tek kaynağıdır.** Diğer dokümanlar metrikleri yalnızca kimlikle referans verir. Mevcut sunucu loglama altyapısına dair doğrulanmış bilgiler [02](02_REPOSITORY_ANALYSIS_AND_INTEGRATION_MAP.md)'dedir.

---

## 1. Amaç

"Botlar iyi PK yapıyor" iddiası yalnızca bu dokümandaki metriklerle, [15](15_TEST_ARENA_SCENARIOS_AND_ACCEPTANCE_CRITERIA.md)'teki senaryolarda ve aşağıdaki istatistik kurallarıyla ölçülerek ileri sürülebilir. Telemetrinin üç işlevi vardır:

1. **Kalite ölçümü:** Taktik kararların ve mekanik uygulamanın niceliksel değerlendirmesi.
2. **Açıklanabilirlik:** Her önemli karar için "ne gözlemlendi, hangi seçenekler değerlendirildi, neden bu seçildi, sonuç ne oldu" kaydı.
3. **Güvenlik ve regresyon tespiti:** Fairness ihlali, takılma, chat spam, performans gerilemesi.

## 2. Mevcut altyapı ve kısıtlar

- Sunucu, `Logs/` altında günlük dosyalara (ör. `DeathNpc_<g>_<a>_<y>.log`, `DeathUser_...`, `Chat_...`, `Cheat_...`, `Login_...`) yazar. Bu dosyaların bu makinede 2026-10-01 tarihinde oluştuğu gözlendi `[V]`; hangi fonksiyonların yazdığı [02](02_REPOSITORY_ANALYSIS_AND_INTEGRATION_MAP.md)'de açıklanır.
- Kurulum notuna göre konsol çıktısı dosyaya yönlendirildiğinde CRT tamponlaması nedeniyle boş kalıyor (`start.md` §9) `[V]`. Bu nedenle bot telemetrisi **konsola değil, kendi dosya yazıcısına** yazmalıdır.
- Botlar oyun thread'i içinde karar verir ([13](13_BOT_ARCHITECTURE_AND_DATA_MODEL.md)); telemetri yazımı oyun thread'ini bloklamamalıdır. Öneri: kilitsiz/az kilitli bir kuyruk + ayrı yazıcı thread `[Ö]`.

## 3. Olay modeli

### 3.1 Ortak alanlar

Her telemetri kaydı tek satır JSON'dur (JSONL). Ortak alanlar:

| Alan | Tip | Açıklama |
|---|---|---|
| `t` | int64 | Sunucu monoton zamanı (ms). Duvar saati ayrıca `ts_utc` olarak yalnızca maç başlangıç/bitiş kayıtlarında yazılır. |
| `match` | string | Maç/episode kimliği (`<senaryo>-<seed>-<tekrar>-<taraf>`) |
| `bot` | int | Bot birim kimliği (sunucu içi `GetID()`), `-1` takım/sistem olayları için |
| `name` | string | Karakter adı (yalnızca bot karakterleri; gerçek oyuncu adları yalnızca test oturumunda ve onaylı katılımcılarla) |
| `role` | string | Rol profili kimliği (ör. `priest.heal_debuff`) |
| `policy` | string | `policy_id@version` |
| `ev` | string | Olay tipi (§3.2) |
| `mode` | string | `train` / `eval` / `live` / `debug` |

### 3.2 Olay tipleri

| `ev` | Ne zaman | Ek alanlar |
|---|---|---|
| `MATCH_START` / `MATCH_END` | Senaryo başı/sonu | senaryo, seed, kompozisyonlar, ekipman seti, sonuç, süre, commitler, veri hash'leri |
| `DECISION` | Karar katmanı bir aksiyon seçtiğinde (her tick değil; **seçim değiştiğinde** veya en az 1 sn'de bir) | §4 şeması |
| `ACTION_SUBMIT` | Aksiyon sunucu handler'ına verildiğinde | aksiyon tipi, skill/item kimliği, hedef, konum; `UsePotion` için `item`, `skill`, `kind` (`hp`/`mp`), `stock` (paket öncesi çanta adedi), `use` (seri sırası); `ACTION_RESULT` `stock_after` taşır (yalnızca operatör içindir, sonuç eşlemesi buna bakmaz); `TargetHpReq` için `target` (hedef kimliği), `echo` (1 seçim, 0 yoklama), `ACTION_RESULT` `reason` `observed`/`no_result` ve `observed` iken `hp`/`max_hp` (yalnızca sunucunun `WIZ_TARGET_HP` cevabından); `FAIRNESS_REJECT` `type:"TargetHp"` (`CLI-10` `out_of_view`/`poll`, `CLI-11` `rate`) |
| `ACTION_RESULT` | Handler sonucu (başarılı/başarısız + sebep) | sonuç kodu, sunucu fail sebebi, gecikme; alan skill'inin EFFECTING sonucunda ek `victims` (çağırana gelen kurban kimlikli `MAGIC_EFFECTING` sayısı, NPC dahil, isabet değil; F4-29, `docs/03` MEC-MAG-16) |
| `FAIRNESS_REJECT` | `BotFairnessGuard` bir aksiyonu reddettiğinde | ihlal edilen kural, beklenen bekleme süresi |
| `DAMAGE` | Bot hasar verdiğinde/aldığında | kaynak, hedef, miktar, skill, hedef HP önce/sonra |
| `HEAL` | Bot heal verdiğinde/aldığında | miktar, etkili miktar, overheal, hedef HP önce/sonra, `incoming_3s` (sonradan doldurulur) |
| `BUFF_APPLY` / `BUFF_EXPIRE` / `BUFF_REMOVED` | Buff/debuff başlangıç/bitiş/cure | buff tipi, skill, süre, kaynak |
| `TARGET_SET` | Botun hedefi değiştiğinde | eski/yeni hedef, gerekçe kodu (§5.2) |
| `TARGET_CALL` | Takım ortak hedef çağrısı | çağıran, hedef, gerekçe (`DEBUFF_SUCCESS`, `LOW_HP`, `HEALER_PRESSURE`...) |
| `CHAT_SENT` | Bot chat gönderdiğinde | kanal, metin, ilişkili `TARGET_CALL` |
| `STATE_CHANGE` | Davranış durumu değiştiğinde | eski/yeni durum, tetik |
| `DEATH` / `RESPAWN` / `RESURRECT` | Ölüm/yeniden doğma/diriltme | öldüren, konum, respawn tipi |
| `SUMMON` | Summon denemesi/sonucu | çağıran, çağrılan, güvenlik kontrol sonuçları |
| `POTION` | Pot kullanımı | item, eksik miktar, beklenen/etkili kazanım, cooldown durumu |
| `NAV_PATH` | Yol hesaplandığında | başlangıç, hedef, uzunluk, düğüm sayısı, süre (µs), başarı |
| `NAV_STUCK` / `NAV_RECOVERY` | Takılma tespiti / kurtarma aşaması | konum, aşama, süre, sonuç |
| `TEST_TELEPORT` | Test kurtarma teleportu | **eval modunda olması maçı geçersiz kılar** |
| `POLICY_LOAD` / `POLICY_ROLLBACK` | Politika yüklendi/geri alındı | sürümler, gerekçe |
| `SCRIPT_START` / `SCRIPT_STEP` / `SCRIPT_END` | Betik (`/bot script run`, F4-20) başlayınca / her adım çalıştırılırken (komuttan hemen önce) / bitince veya durdurulunca; `bot:-1`, `name` yok | `SCRIPT_START`: `script`, `steps`, `duration_ms`; `SCRIPT_STEP`: `script`, `step` (1 tabanlı), `line`, `offset_ms`, `late_ms` (gerçek − planlanan, ≥ 0), `verb`; `SCRIPT_END`: `script`, `result` (`completed`/`stopped`), `steps_run`, `steps_total`, `elapsed_ms`, `max_late_ms`. Sonraki `ACTION_*`/`FAIRNESS_REJECT` olayları aynı `t` damgasıyla adıma bağlanır |
| `PERF_SAMPLE` | Periyodik (5 sn) | tick süreleri, kuyruk uzunluğu, bot sayısı |

### 3.3 Uygulama notları (ADR-0007, F3-01)

- **Alan kuralı:** Uygulanabilir olmayan ortak alanlar yazılmaz (alan yok = geçerli değil): `role`, `policy` yalnızca rol profili olan botlarda; `name` yalnızca bot olaylarında; `bot` sistem olaylarında `-1`. Maç bağlamı yokken (`ScenarioRunner` gelene kadar, F3-03) `match` = `"-"`, `mode` = `"live"`.
- **`t`:** `steady_clock` zamanı, milisaniye (süreç içi karşılaştırma için; duvar saati yalnızca `MATCH_START/END`).
- **Seviye eşlemesi:** `summary`: `MATCH_START/END`, `PERF_SAMPLE`; `decisions`: `+` `DECISION`, `ACTION_*`, `FAIRNESS_REJECT`, `SCRIPT_*`, `TARGET_*`, `STATE_CHANGE`, `DEATH/RESPAWN`, `POTION`, `BUFF_*`, `HEAL`, `DAMAGE`; `trace`: `+` `NAV_*` ve tek bota özel ayrıntı. Seviye eşlemesinin tek kaynağı bu tablodur; kodda sabittir.
- **Düşürülebilir olaylar:** `PERF_SAMPLE`, `DECISION` (kuyruk yumuşak sınırı aşılınca düşer); diğerleri yalnızca sert sınırda düşer. Düşürme sayaçları `PERF_SAMPLE` kaydındaki `dropped_soft`/`dropped_hard` alanlarındadır.
- **`PERF_SAMPLE` alanları (F3-01):** `window_ms`, `tick_n`, `tick_p50_us`, `tick_p95_us`, `tick_p99_us`, `tick_max_us` (BotManager `Tick()` toplam süresi, MET-PERF-02), `sessions`, `in_game`, `pool_free`, `skipped_ticks`, `queue_len`, `written`, `dropped_soft`, `dropped_hard`. Bot başına tick süresi (MET-PERF-01) karar katmanı gelince eklenir.
- **`SELFTEST`:** yalnızca `[BOT] TELEMETRY_SELFTEST=1` iken yazıcı/taşma öz-sınaması üretir (`i` alanı); analiz araçları yok sayar.
- **Dosya:** `Logs/bots/<YYYY-MM-DD>/live-<HHMMSS>.jsonl`.
- **Maç bağlamı (ADR-0007 Ek, F3-02):** `/bot match start <senaryo> [seed]` ile açılan maçta olaylar `Logs/bots/<YYYY-MM-DD>/<match>.jsonl` dosyasına `"match":"<senaryo>-<seed>-<tekrar>"` ile yazılır; `/bot match end [sonuç]` ile `MATCH_END` yazılıp dosya kapanır ve `<match>.summary.json` oluşur. Maç yokken `match` = `"-"`, dosya `live-*.jsonl`. `mode` bu aşamada hep `live`; taraf eki ve ScenarioRunner alanları F3-03'te.
- **`MATCH_START` alanları (F3-02):** `ts_utc`, `scenario`, `seed`, `run`, `composition` (oyundaki bot adları), `in_game`. **`MATCH_END` alanları:** `ts_utc`, `duration_ms`, `result` (`completed` varsayılan, `aborted` = sunucu kapanışı), `dropped_soft`, `dropped_hard` (maç boyunca), `in_game`, `perf_samples`, `tick_p95_max_us`, `tick_max_us` (tamamlanmış 5 sn pencereleri). Ekipman seti, commit ve veri hash alanları henüz yok (F3-03/F3-06).
- **`summary.json`:** tek satır JSON: `match`, `mode`, `file`, `start{}`, `end{}`, `lines`, `events{<ev>:<satır sayısı>}`.

## 4. Karar logu (açıklanabilirlik)

Her `DECISION` kaydı şu dört soruyu cevaplar: **ne gözlendi, hangi seçenekler değerlendirildi, neden bu seçildi, sonuç ne oldu.** Sonuç alanı, ilgili `ACTION_RESULT` ve sonraki etkiler geldikçe aynı `decision_id` ile ilişkilendirilir (ayrı satırlar; analiz aracında birleştirilir).

```json
{
  "t": 1834221, "match": "EVAL-8v8-A-17-2-K", "bot": 10241, "role": "priest.heal_debuff",
  "policy": "priest.heal_debuff@1.0.0", "ev": "DECISION", "decision_id": "10241-5512",
  "state": "COMBAT_SUPPORT",
  "obs": {
    "self": {"hp": 0.82, "mp": 0.41, "mp_abs": 1210, "cd_ready": ["heal_t3", "debuff_a"], "pot_cd_ms": 0},
    "allies_low": [{"id": 10233, "hp": 0.28, "incoming_2s": 640, "dist": 9.5, "los": "unknown"}],
    "team_target": {"id": 20117, "called_by": 10239, "age_ms": 3400, "net_dmg_5s": -150},
    "enemies_visible": 7, "enemy_healers": 2
  },
  "options": [
    {"a": "HEAL", "target": 10233, "skill": "<id>", "score": 0.91, "terms": {"urgency": 0.72, "saved_est": 0.95, "mp_cost": -0.06}},
    {"a": "DEBUFF", "target": 20117, "skill": "<id>", "score": 0.44, "terms": {"conversion": 0.51, "already_debuffed": 0.0}},
    {"a": "MOVE_LOS", "score": 0.10}
  ],
  "chosen": 0, "reason": "EMERGENCY_HEAL_RULE", "override": true
}
```

Kurallar:

- `options` en fazla 5 seçenek içerir (en yüksek skorlu 4 + seçilen, aynı değilse). Skor bileşenleri (`terms`) utility ağırlıklarıyla birlikte loglanır; böylece L1 optimizasyonu sonrası farklar açıklanabilir.
- `reason` kapalı bir kod listesinden gelir (§5). Serbest metin yoktur.
- `override=true`, utility skorundan bağımsız olarak bir acil kuralın devreye girdiğini gösterir (ör. acil heal, ölüm önleme geri çekilmesi).
- `los` alanı görüş hattının doğrulanamadığı durumlarda `"unknown"` olur ([12](12_NAVIGATION_AND_POSITIONING.md)).

## 5. Kod listeleri

### 5.1 Aksiyon sonuç kodları

`OK`, `SRV_FAIL_<sebep>` (sunucunun döndürdüğü fail sebebi; sebep adları [03](03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md)'teki doğrulama sırasına göre), `GUARD_<kural>` (fairness guard), `PRECHECK_<sebep>` (botun kendi ön kontrolü: menzil, kaynak, cooldown, hedef geçersiz, `quest_locked`: skill'in `Etc` quest'i botun listesinde tamamlanmamış, `docs/03` MEC-MAG-14), `TIMEOUT` (beklenen sonuç gelmedi).

### 5.2 Hedef değişim gerekçeleri

`INIT`, `TARGET_DEAD`, `TARGET_LOST_VIS`, `TARGET_UNREACHABLE`, `TEAM_CALL`, `DEBUFF_CALL`, `HEALER_SWITCH`, `FINISHABLE`, `PEEL_THREAT`, `SELF_DEFENSE`, `RETREAT`, `LEADER_ORDER`, `SCORE_MARGIN`.

### 5.3 Karar gerekçe kodları

`UTILITY_MAX`, `EMERGENCY_HEAL_RULE`, `DEATH_AVOID_RULE`, `COMMIT_HOLD`, `TEAM_CALL_FOLLOW`, `RESOURCE_RESERVE`, `NAV_RECOVERY`, `SAFETY_RULE`, `IDLE_NO_OPTION`.

## 6. Metrik kataloğu

Tanımlarda "fırsat", botun **o anda** aksiyon yapabileceği durumdur: canlı, hedef geçerli ve menzilde, aksiyon kilidi yok, kaynak yeterli, ilgili cooldown hazır. Fırsatın tespiti [03](03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md)'teki doğrulanmış kurallara göre yapılır.

### 6.1 Mekanik uygulama

| Kimlik | Ad | Tanım | Hedef/eşik (başlangıç) |
|---|---|---|---|
| MET-ACT-01 | Saldırı fırsatı kullanma oranı | Fırsat bulunan tick'lerden, fırsat başladıktan sonra `P-ACT-LATENCY` (varsayılan 250 ms) içinde geçerli saldırı/skill gönderilenlerin oranı | Warrior ≥ %85, mage ≥ %75 (rol bazlı) |
| MET-ACT-02 | Geçersiz aksiyon oranı | `ACTION_RESULT` içinde `SRV_FAIL_*` / tüm `ACTION_SUBMIT` | ≤ %2; sebep dağılımı raporlanır |
| MET-ACT-03 | Fırsat → aksiyon gecikmesi | p50/p95 | p95 ≤ 400 ms |
| MET-FAIR-01 | Fairness guard reddi | `FAIRNESS_REJECT` sayısı / bot-saat | Bilgi amaçlı; reddin **kendisi** sorun değildir ama sunucuya giden ihlal 0 olmalı. Sunucuya ihlalli aksiyon ulaşması (guard atlatılmışsa) = 0 |

MET-ACT-02 / MET-FAIR-01 uygulama notu (F4-21, ADR-0017 Eki F4-21): telemetrideki karşılığı `ACTION_RESULT.ok=false` ve `reason` ∈ {`srv_fail`, `handler_noop`, `refused_*`} (geçersiz); `no_result` ayrı sütundur, paya girmez; payda tüm `ACTION_SUBMIT`. Hüküm: PASS ≤ %1 (F4 kapısı), WARN ≤ %2, FAIL > %2. MET-FAIR-01 bot-saat oranı tahmindir. Hesaplayan araç: `tools/bot-telemetry-report.py` (F4-21).

### 6.2 Hedefleme ve baskı

| Kimlik | Ad | Tanım | Başlangıç eşiği |
|---|---|---|---|
| MET-TGT-01 | Hedefe erişim süresi | Hedef atanması → ilk kez etkili menzilde olma | Melee p50 ≤ 4 sn (ulaşılabilir hedefte) |
| MET-TGT-02 | Baskı sürekliliği | Hedef atanmış ve ulaşılabilirken, kayan 2 sn pencerede hedefe hasar/debuff uygulanmış zaman oranı | Warrior ≥ %70 |
| MET-TGT-03 | Ortak hedefe katılım oranı | `TARGET_CALL` sonrası 6 sn içinde ortak hedefe hasar/debuff/kontrol uygulayan üye sayısı / **fiilen katılabilecek** üye sayısı (canlı, geri çekilmede değil, yol uzunluğu ≤ rol menzili + `P-TGT-REACH-SLACK`) | ≥ %75 |
| MET-TGT-04 | Hedef değişim sıklığı | Bot başına dakikada `TARGET_SET`; gerekçe kodu dağılımı ile | ≤ 4/dk (`SCORE_MARGIN` gerekçeli ≤ 1.5/dk) |
| MET-TGT-05 | Ortak hedefte öldürme süresi | Çağrı → ölüm (ölmeyenler ayrı) | Raporlanır |
| MET-DMG-01 | Ortak hedefe hasar payı | Ortak hedefe verilen hasar / toplam verilen hasar | Party modunda ≥ %60 |
| MET-AOE-01 | Alan hasarı verimi | Alan skill'i başına isabet eden düşman sayısı ortalaması | ≥ `P-MAG-AOE-MIN` |
| MET-PEEL-01 | Destek koruması | Düşman melee'nin kendi priest/mage'e temas süresi (≤ 2 m) / toplam savaş süresi | Baseline'a göre azalma raporlanır |

### 6.3 Destek

| Kimlik | Ad | Tanım | Başlangıç eşiği |
|---|---|---|---|
| MET-HEAL-01 | Etkili heal | Gerçekten geri kazandırılan HP toplamı (max HP üstü hariç) | Raporlanır |
| MET-HEAL-02 | Overheal oranı | Overheal / nominal heal | ≤ %25 (acil durum hariç) |
| MET-HEAL-03 | Kurtarılan kritik durum (`heal_saved`) | Müttefik HP < %25 iken heal → sonraki 5 sn içinde ölmeden HP ≥ %50; ayrıca §7.2 karşı-olgusal tahmin ([14](14_LEARNING_AND_ADAPTATION.md)) | Raporlanır; baseline ile karşılaştırılır |
| MET-HEAL-04 | Çift heal | Aynı hedefe iki priest'in 1 sn içinde heal'i ve toplamın eksik HP'yi %30'dan fazla aşması | ≤ %5 |
| MET-BUFF-01 | Buff kapsama | Savaş süresince gerekli buff'ların aktif olduğu süre / toplam süre (üye başına, buff başına) | ≥ %90 |
| MET-BUFF-02 | Buff yenileme gecikmesi | Bitiş → yeniden uygulama | p95 ≤ 5 sn (savaş içinde) |
| MET-BUFF-03 | Tekrarlanan buff | Hedefte aynı BuffType aktifken buff gönderme denemesi (sunucu zaten reddeder, MEC-BUF-02; deneme boşa cast'tir) | 0 |
| MET-DEBUFF-01 | Debuff başarı oranı | Başarılı uygulama / deneme | Raporlanır (sunucu başarı şansına bağlı) |
| MET-DEBUFF-02 | Debuff → baskı geçişi | Başarılı debuff → takımın ilk hasarı (çağrılan hedefe) | p50 ≤ 2 sn |
| MET-DEBUFF-03 | Debuff dönüşüm oranı | Başarılı debuff sonrası 10 sn içinde hedefin ölmesi / geri çekilmeye zorlanması | Raporlanır |
| MET-DEBUFF-04 | Yanlış çağrı | Başarılı debuff olmadan yapılan debuff çağrısı | **0** |
| MET-CURE-01 | Cure gecikmesi | Cure edilebilir kritik debuff başlangıcı → cure | p95 ≤ 3 sn (cure'a uygun priest varsa) |

### 6.4 Kaynak ve hayatta kalma

| Kimlik | Ad | Tanım | Başlangıç eşiği |
|---|---|---|---|
| MET-POT-01 | Pot verimi | Etkili kazanım / nominal pot değeri | ≥ %80 |
| MET-POT-02 | Kaynak yetersizliğinden kaçırılan aksiyon | Fırsat varken MP/HP/item yetersizliği nedeniyle yapılamayan aksiyon sayısı / dk | Raporlanır |
| MET-POT-03 | Pot tüketimi | Pot / dakika (HP, MP ayrı) | Raporlanır |
| MET-POT-04 | Kritik MP rezervi ihlali | MP < rezerv iken rezerv dışı skill kullanımı | 0 (acil heal hariç) |
| MET-SUR-01 | Geri çekilme başarısı | Geri çekilme başlangıcından sonra ölmeden `P-SUR-REENTER-HP` eşiğine ulaşma oranı | ≥ %70 (sıkışmış durumlar ayrı) |
| MET-SUR-02 | Savaşa dönüş süresi | Geri çekilme başlangıcı → yeniden savaşa giriş | Raporlanır |
| MET-SUR-03 | Geri çekilmede ölüm | Geri çekilme sırasında ölüm / geri çekilme | Raporlanır |
| MET-SUR-04 | Savaş dışı süre oranı | Canlıyken savaş dışında geçen süre / maç süresi | Guard metrik (artış = alarm) |
| MET-SUR-05 | Ölüm sıklığı | Ölüm / dk | Raporlanır |
| MET-SUR-06 | Durum salınımı | 10 sn içinde ≥ 3 kez `COMBAT ↔ RETREAT` geçişi | 0 |

### 6.5 Takım

| Kimlik | Ad | Tanım | Başlangıç eşiği |
|---|---|---|---|
| MET-PTY-01 | Ölüm sonrası katılım süresi | Respawn → party merkezine `P-PTY-REGROUP-R` içinde olma (yürüyerek veya summon) | Raporlanır; summon'lu ve summon'suz ayrı |
| MET-PTY-02 | Party yayılımı | Canlı üyelerin party merkezine maksimum uzaklığı (zaman serisi) | p95 ≤ `P-PTY-SPREAD-MAX` |
| MET-PTY-03 | Lider değişim süresi | Lider ölümü/kopması → yeni liderin ilk komutu | ≤ 2 sn |
| MET-PTY-04 | Regroup başarısı | Regroup komutu → üyelerin %75'inin toplanması | ≤ 15 sn (ulaşılabilir alan) |
| MET-SUM-01 | Summon sonrası savaşa dönüş | Summon tamamlanması → ilk etkili aksiyon | Raporlanır |
| MET-SUM-02 | Güvensiz summon | Summon sonrası 5 sn içinde summon edilen üyenin ölmesi | ≤ %10 |
| MET-CHAT-01 | Chat oranı | Bot başına mesaj / dk; limit ihlali | Limit ihlali 0 |

### 6.6 Navigasyon

| Kimlik | Ad | Tanım | Başlangıç eşiği |
|---|---|---|---|
| MET-NAV-01 | Takılma oranı | `NAV_STUCK` / bot-saat | ≤ 2 |
| MET-NAV-02 | Kurtarma süresi | `NAV_STUCK` → hareketin yeniden başlaması | p95 ≤ 5 sn |
| MET-NAV-03 | Yol bulma başarısızlığı | Başarısız `NAV_PATH` / tüm | Raporlanır (ulaşılamayan hedefler ayrı) |
| MET-NAV-04 | Ulaşılamayan hedefi bırakma | Ulaşılamaz tespiti → hedef bırakma | ≤ 3 sn |
| MET-NAV-05 | Test teleportu | `TEST_TELEPORT` sayısı | Eval modunda **0** (aksi halde maç geçersiz) |
| MET-NAV-06 | Üst üste yığılma | Aynı party'den iki üyenin < 1 m mesafede 2 sn'den uzun kalması | Raporlanır |

### 6.7 Sonuç

| Kimlik | Ad | Tanım |
|---|---|---|
| MET-OUT-01 | Kazanma oranı | `docs/15` §6b `win_rule` sonucuna göre (`draw` = 0,5, `invalid` paya girmez); Wilson %95 GA ile |
| MET-OUT-02 | Kill/death farkı | Takım bazında |
| MET-OUT-03 | Maç süresi | Zaman aşımı oranı ile birlikte |
| MET-OUT-04 | Elo/TrueSkill | Politika sürümleri ve taktik profilleri arası lig |
| MET-OUT-05 | Sonuç kodu dağılımı | `win_a`/`win_b`/`draw`/`invalid`/`no_result` oranı, `win_rule` türüne göre ayrı; `invalid` nedenleri (`NO_ENGAGE`, `SETUP_FAIL`, `TEST_TELEPORT`, `THIRD_PARTY`); `killdiff_timed` için `fark` ortalaması/standart sapması ve beraberlik oranı (pilot kalibrasyonu, `docs/15` §6b) |

### 6.8 Performans

| Kimlik | Ad | Tanım | Başlangıç bütçesi `[Ö]` |
|---|---|---|---|
| MET-PERF-01 | Bot tick süresi | Bot başına karar+aksiyon süresi p50/p95/p99 | p95 ≤ 0.3 ms |
| MET-PERF-02 | BotManager toplam süre | Tick başına tüm botlar | 16 bot için p95 ≤ 5 ms; 64 bot için ≤ 15 ms |
| MET-PERF-03 | Yol bulma maliyeti | Arama başına süre ve genişletilen düğüm | p95 ≤ 2 ms, düğüm limiti [12](12_NAVIGATION_AND_POSITIONING.md) |
| MET-PERF-04 | Sunucu etkisi | Botsuz ve botlu durumda ana timer gecikmesi, CPU, bellek | Botlu/botsuz CPU artışı raporlanır |
| MET-PERF-05 | Ağ etkisi | Botların ürettiği bölge yayını (paket/sn, bayt/sn) | Raporlanır |

Eşikler başlangıç hipotezidir. F3–F6 fazlarında ölçülen dağılımlara göre güncellenir; güncelleme [17](17_IMPLEMENTATION_ROADMAP_AND_PHASE_GATES.md)'deki karar kaydı şablonuyla yapılır.

### 6.9 Rol bilinçli yorum ve ek metrikler (değerlendirme 2026-10-02)

**Yorum kuralı:** her metrik rolün görevine göre okunur; bir rolün birincil olmayan metriği başarısızlık sayılmaz. Örnek: kritik heal atan priest o sırada ortak hedefe saldırmadığı için MET-TGT-03/MET-DMG-01'de **başarısız sayılmaz**; payda yalnızca o anda yüksek öncelikli görevde olmayan üyeleri içerir.

| Kimlik | Ad | Tanım | Başlangıç eşiği `[Ö]` |
|---|---|---|---|
| MET-ROLE-01 | Rol bilinçli katılım | MET-TGT-03 ve MET-DMG-01 paydasından, `DECISION.override = true` ve gerekçesi `EMERGENCY_HEAL_RULE`/cure/res/`PEEL`/`DEATH_AVOID_RULE` olan üyelerin o süreleri çıkarılır; "görevde geçen süre" ayrıca raporlanır | MET-TGT-03 eşiği aynı |
| MET-HEAL-05 | Kaçırılan kritik heal | Müttefik tahmini HP < `P-PRI-HEAL-EMERG`, menzilde, MP/cooldown uygun iken `P-ACT-LATENCY` + cast süresi içinde heal başlamayan fırsat oranı | ≤ %5 |
| MET-HEAL-06 | Gereksiz heal/buff | Hedefte aşırı overheal (`P-PRI-OVERHEAL-MAX` üstü) veya ayakta buff'ı varken yapılan heal/buff sayısı / tüm heal/buff (MET-BUFF-03 dahil) | ≤ %10 |
| MET-CURE-02 | Kaçırılan cure | Kritik debuff başladı, cure fırsatı var, 3 sn içinde cure başlamadı oranı | ≤ %10 |
| MET-IDLE-01 | Boşta süre ve ulaşamama | Canlı + aksiyon fırsatı yok + hareket yok süre / canlı süre; ayrıca hedef atandı ama 10 sn içinde etkili menzile girilemedi oranı (MET-TGT-01 ile) | Guard metrik (artış = alarm) |
| MET-SUR-07 | Başarısız savaşa dönüş | `REENTER` sonrası 10 sn içinde ölüm veya aynı geri çekilmeye yeniden giriş / tüm dönüşler | ≤ %20 |
| MET-STALL-01 | Heal-stall kararı | Stall tespiti → karar (`HEALER_SWITCH`/`SPLIT`/`BURST_NOW`) ve 15 sn sonucu (hedef öldü / healer öldü / sonuçsuz); "karar yok" oranı | Karar yok ≤ %10; sonuç dağılımı raporlanır |

Öğrenme ile değerlendirme rakipleri ayrıdır; kilitli sete erişim denetimi `docs/14` §9'dadır.
## 7. İstatistik kuralları

- **Tek maç kanıt değildir.** Her karşılaştırma en az: N tekrar × 2 taraf (Karus/El Morad değişimi) × aynı seed listesi. Başlangıç N = 20 (8 vs 8 için), 1 vs 1 için N = 50.
- **Ekipman dengesi:** Karşılaştırmalarda iki takım aynı referans ekipman setini kullanır ([04](04_CHARACTER_BUILDS_STATS_AND_EQUIPMENT.md)).
- **Güven aralıkları:** Oranlar için Wilson; sürekli metrikler için 2000 örnekli bootstrap; kazanma oranı karşılaştırmasında SPRT ([14](14_LEARNING_AND_ADAPTATION.md) §8).
- **Çoklu karşılaştırma:** Aynı raporda çok sayıda metrik karşılaştırılıyorsa yalnızca önceden ilan edilmiş birincil metrikler (MET-OUT-01 ve senaryonun birincil metriği) karar için kullanılır; diğerleri keşif amaçlıdır.
- **Geçersiz maç:** `TEST_TELEPORT` (eval modunda), sunucu çökmesi, beklenmeyen üçüncü taraf (senaryo bunu içermiyorsa), bot spawn hatası. Geçersiz maçlar silinmez, ayrı sayılır.

## 8. Hata ayıklama araçları `[Ö]`

| Araç | Amaç | Not |
|---|---|---|
| `+bot` GM komut ailesi | Bot listeleme, durum, politika sürümü sabitleme, karar logu ayrıntı seviyesini değiştirme, tek botu duraklatma | Komut ayrıştırma mevcut GM komut altyapısına eklenir ([02](02_REPOSITORY_ANALYSIS_AND_INTEGRATION_MAP.md)) |
| `+bot why <isim>` | Botun son kararını özetleyip GM'e özel mesaj olarak gönderme | Oyun içi hızlı teşhis |
| Replay/analiz aracı (Python) | JSONL → zaman çizelgesi, harita üzerinde konum izi (PNG/HTML), metrik raporu | Harita arka planı [12](12_NAVIGATION_AND_POSITIONING.md)'deki ızgara çıktısından |
| Senaryo raporu üretici | MATCH_START/END + metrikler → Markdown rapor | [17](17_IMPLEMENTATION_ROADMAP_AND_PHASE_GATES.md) faz sonuç raporuna eklenir |
| Ayrıntı seviyeleri | `off` / `summary` / `decisions` / `trace` | `trace` yalnızca tek bot için açılır |

## 9. Depolama ve hacim

- Dosya: `Logs/bots/<tarih>/<match>.jsonl`; maç sonunda `summary.json`.
- Tahmini hacim `[Ö]`: `decisions` seviyesinde bot başına ~5–20 kayıt/sn × ~400 bayt → 16 bot için 10 dakikalık maçta ≈ 20–80 MB. `summary` seviyesi canlı testlerde varsayılandır.
- Telemetri yazımı oyun thread'inde yalnızca kuyruğa ekleme yapar; serileştirme ve disk yazımı ayrı thread'dedir. Kuyruk dolarsa en düşük öncelikli olaylar (`PERF_SAMPLE`, `DECISION`) düşürülür ve düşürme sayacı tutulur.

## 10. Performans test planı (özet)

Ayrıntılı senaryolar [15](15_TEST_ARENA_SCENARIOS_AND_ACCEPTANCE_CRITERIA.md)'te (T-PERF-*). Ölçüm yöntemi:

1. Botsuz sunucu referansı (aynı zone, aynı oyuncu sayısı).
2. 16 bot (8 vs 8), 32 bot, 64 bot; her biri 30 dk.
3. Her seviye için MET-PERF-01..05 ve sunucu timer gecikmeleri.
4. Bellek sızıntısı kontrolü: 4 saatlik dayanıklılık testinde bot başına bellek eğilimi.

## 11. Açık konular

- Mevcut sunucu döngüsünün gerçek tick aralıkları ve bot kararlarının bağlanacağı timer ([02](02_REPOSITORY_ANALYSIS_AND_INTEGRATION_MAP.md), [13](13_BOT_ARCHITECTURE_AND_DATA_MODEL.md)).
- Sunucunun döndürdüğü fail sebeplerinin istemciye/bota ne ayrıntıda iletildiği ([03](03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md)).

## Değişiklik günlüğü

| Tarih | Sürüm | Değişiklik |
|---|---|---|
| 2026-10-01 | v1.0 | İlk sürüm |
| 2026-10-02 | v1.1 | Değerlendirme: §6.9 rol bilinçli yorum ve yeni metrikler (MET-ROLE-01, MET-HEAL-05/06, MET-CURE-02, MET-IDLE-01, MET-SUR-07, MET-STALL-01), MET-OUT-01 tanımı, MET-OUT-05 |
