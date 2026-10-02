# ADR-0007: Telemetri formatı ve depolama: JSONL, sınırlı kuyruk, ayrı yazıcı thread (otonom döngüde Claude kararı — gözden geçirilmeli)

Durum: KABUL (geçici, proje sahibi gözden geçirecek) · Tarih: 2026-10-02 · Karar veren: Claude (gece modu, `AUTO_LOOP=1`; kullanıcıya sorulamadı)
İlgili: `docs/16` (olay şeması tek kaynak), `docs/13` §2 (`Telemetry` bileşeni), `docs/17` F3, ADR-0005 (bot durumu IOCP thread'inde)

## Bağlam
F3, her sonraki fazın ölçülebilir olmasını sağlar. `docs/16` olay modelini (§3), karar logu şemasını (§4) ve depolama önerisini (§9) verir ama üç uygulama kararı açık bırakılmıştı: dosya biçimi, kuyruk yapısı, kuyruk dolunca davranış. Kısıtlar (`docs/16` §2): konsol çıktısı dosyaya yönlendirilince boş kalıyor (`start.md` §9) `[V]`, bu yüzden telemetri kendi dosya yazıcısına yazmalı; oyun (IOCP) thread'i bloklanmamalı.

## Karar
- **Biçim:** satır başına bir JSON nesnesi (JSONL), UTF-8, `docs/16` §3.1 ortak alanlarıyla. Uygulanabilir olmayan ortak alanlar (`role`, `policy`) yazılmaz (alan yok = geçerli değil). JSON kütüphanesi eklenmez; yazıcı elle serileştirir (alanlar sayı/sabit kodlar; metin alanları kaçışlanır).
- **Kuyruk:** tek `std::mutex` ile korunan sınırlı `std::vector` (az kilitli, kilitsiz değil). Üretici thread yalnızca olay yapısını (sabit `ev` kodu + önceden biçimlenmiş ek alan parçası) kuyruğa ekler; **JSON satırının birleştirilmesi ve disk yazımı yazıcı thread'dedir** (`docs/16` §9). Yazıcı thread kuyruğu 100 ms'de bir `swap` ile alır, bir toplu `fwrite` + `fflush` yapar.
- **Taşma politikası:** yumuşak sınır 6144 olay: aşılınca `droppable` olaylar (`PERF_SAMPLE`, `DECISION`) düşürülür ve sayılır; sert sınır 8192: aşılınca her olay düşürülür ve ayrıca sayılır. Düşürme sayaçları `PERF_SAMPLE` kaydında ve kapanış logunda görünür; sessiz kayıp yoktur.
- **Seviyeler:** `[BOT] TELEMETRY=off|summary|decisions|trace`, varsayılan `summary` (yalnızca `ENABLED=1` iken anlamlı; bot sistemi kapalıyken telemetri hiç başlatılmaz). Olay başına gereken en düşük seviye kod tarafında sabittir.
- **Depolama:** `Logs/bots/<YYYY-MM-DD>/live-<HHMMSS>.jsonl` (süreç başına bir dosya; senaryo koşucusu geldiğinde `docs/16` §9'daki `<match>.jsonl` adlandırmasına F3-03 geçer). Mevcut `Logs/*.log` dosyalarına dokunulmaz.

## Değerlendirilen alternatifler
| Alternatif | Artılar | Eksiler | Neden seçilmedi |
|---|---|---|---|
| Kilitsiz halka tamponu (SPSC/MPSC) | En düşük üretici maliyeti | Birden çok üretici thread (IOCP, DB, konsol) için MPSC gerekir; doğrulaması zor; ek bağımlılık/karmaşıklık | `[Ö]` bütçe (MET-PERF-02'nin %10'u) mutex ile de rahat karşılanır; ölçüm F3-01'de |
| Üretici thread'de tam JSON üretip kuyruğa satır koymak | Basit yazıcı | Serileştirme oyun thread'inde; `docs/16` §9 ile çelişir | Gereksiz oyun thread'i yükü |
| Konsola `printf` | Hiç altyapı yok | Dosyaya yönlendirmede boş kalıyor `[V]`; analiz aracı yok | Güvenilmez |
| Binary (MessagePack/protobuf) | Küçük hacim | Araç ve bağımlılık, elle incelenemez | Hacim `docs/16` §9'a göre 20–80 MB/10 dk, JSONL yeterli |
| SQLite/DB'ye yazım | Sorgulanabilir | Oyun DB'sine yük/kilit riski, kurulum | Python analiz aracı JSONL'i doğrudan okur |

## Sonuçlar
- Olumlu: Python analiz aracı (F3-06) ve `ScenarioRunner` aynı biçimi kullanır; yazıcı bağımsız test edilebilir (F3-01 öz-sınaması); bot kapalıyken hiçbir şey çalışmaz.
- Olumsuz: Üretici başına küçük bir mutex ve bellek tahsisi var (tick başına yalnızca birkaç olay beklenir); sert sınırda olay kaybı kabul edilir (sayılır). Çökmede son ≤ 100 ms olay kaybolabilir.
- Geri alma: `[BOT] TELEMETRY=off` ya da `Telemetry.*` dosyalarının ve tek `BotManager` kancasının kaldırılması.

## Doğrulama
F3-01: öz-sınama (yazıcı duraklatılmış taşma: 856 yumuşak + 952 sert düşürme, toplam yazılan = kuyruktakiler); `PERF_SAMPLE` satırlarının her biri `python3 -m json.tool` ile geçerli; kapalıyken (`TELEMETRY=off` ve `ENABLED=0`) `Logs/bots/` oluşmaz.


## Ek (F3-02): maç bağlamı, `<match>.jsonl` ve `summary.json` (otonom döngüde Claude kararı — gözden geçirilmeli)

Tarih: 2026-10-02 · Karar veren: Claude (gece modu, `AUTO_LOOP=1`)

- **Maç sınırları kuyrukta sıralı denetim olaylarıdır** (`MATCH_START`, `MATCH_END`): tek kuyruk korunur, yazıcı thread maç dosyasına geçişi olayların üretim sırasına göre yapar (ikinci kuyruk veya üretici tarafında dosya açma yok). Denetim olayları sınır denetiminden muaftır (maç başına en çok iki).
- **Dosya:** maç açıkken `Logs/bots/<YYYY-MM-DD>/<match>.jsonl` (maçın başladığı yerel gün); maç yokken `live-<HHMMSS>.jsonl` aynen sürer ve maç penceresindeki olaylar yalnızca maç dosyasına yazılır.
- **Maç kimliği:** `<senaryo>-<seed>-<tekrar>`; `tekrar` süreç boyunca artan sayaçtır, aynı kimlikli dosya varsa sıradaki boş sayıya atlanır (üzerine yazma yok). `docs/16` §3.1'deki `-<taraf>` eki ScenarioRunner (F3-03) ile gelir.
- **`summary.json`:** `<match>.summary.json` (aynı klasör), maç kapanırken yazıcı thread'i tarafından tek satır JSON olarak yazılır: `match`, `mode`, `file`, `start` (MATCH_START alanları), `end` (MATCH_END alanları), `lines`, `events` (olay türü başına satır sayısı). Maç sırasındaki düşürme sayaçları `end.dropped_soft/hard` alanındadır.
- **Kapanış:** `Telemetry::Stop()` açık maçı `result":"aborted"` ile kapatır. Zorla sonlandırmada (KI-010, çökme) maç `MATCH_END`'siz kalabilir.
- Geri alma: `match` komutu kullanılmazsa F3-01 davranışı; kod geri alması `Telemetry` maç bölümünün ve `BotManager::CommandMatch`'in kaldırılmasıdır.
