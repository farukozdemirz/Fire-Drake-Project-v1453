# ADR-0005: Bot tick'i ve aksiyonları IOCP thread'inde (otonom döngüde Claude kararı — gözden geçirilmeli)

Durum: KABUL (geçici, proje sahibi gözden geçirecek) · Tarih: 2026-10-02 · Karar veren: Claude (gece modu, `AUTO_LOOP=1`; kullanıcıya sorulamadı)
İlgili: K-1 (ADR-0001), R-01, R-CODE-01, R-CODE-03, F2 (S8, S9), F4

## Bağlam
`CUser` başına kilit yok; oturum durumuna IOCP işçisi, DB thread'i ve 30 sn zamanlayıcı dokunuyor ([02](../02_REPOSITORY_ANALYSIS_AND_INTEGRATION_MAP.md) §4.4). Botlar da `CUser` olduğundan, bot aksiyonları gerçek oyuncu paketleriyle aynı thread'de ve sırayla işlenmezse yarış koşulları çoğalır. `GameServer` tek bir IOCP işçi thread'i açar (`shared/SocketMgr.cpp:51`, `SpawnWorkerThreads`), yani IOCP'de işlenen her şey zaten seri yürür.

## Karar
[13](../13_BOT_ARCHITECTURE_AND_DATA_MODEL.md) §3.1'deki öneri uygulanır: bot tick'i ve aksiyonları IOCP işçi thread'inde çalışır. Periyodik tetik, ayrı bir `BotTickTimer` thread'inden `PostQueuedCompletionStatus` ile gönderilen özel bir IOCP olayıdır (`SOCKET_IO_EVENT_BOT_TICK`). Zamanlayıcı yalnızca olayı kuyruğa bırakır; hiçbir oyun durumuna dokunmaz. Önceki tick tüketilmeden yenisi gönderilmez (kuyruk birikmez; atlanan tick sayılır).

Bu mekanizma roadmap'te F4'te geçiyordu ([17](../17_IMPLEMENTATION_ROADMAP_AND_PHASE_GATES.md) §2); F2'de bot `Update()` çağrısı ve giriş/çıkış durum makinesi de IOCP'de çalışması gerektiğinden **F2-02'ye öne alındı**. Karar/algı ayrımı (v2, [13](../13_BOT_ARCHITECTURE_AND_DATA_MODEL.md) §3.2) hâlâ ertelenmiştir.

## Değerlendirilen alternatifler
| Alternatif | Artılar | Eksiler | Neden seçilmedi |
|---|---|---|---|
| Ayrı bot thread'i `CUser`'a doğrudan dokunur | Basit | `CUser` kilitsiz; gerçek paketlerle yarış | R-01, R-CODE-03 |
| Mevcut 30 sn zamanlayıcıya bağlanmak | Yeni thread yok | 30 sn granülarite çok kaba; yine IOCP dışı | Bot tick ≥ 1 Hz, hedef 100 ms |
| Tick'i her seferinde yeni IOCP olayı olarak biriktirmek | Basit | Yavaş tick'te kuyruk büyür | Tek uçuşta olay (pending bayrağı) |

## Sonuçlar
- Bot tick'i yavaşlarsa gerçek oyuncu paketleri de bekler: tick süresi ölçülmeli (MET-PERF-02, F3) ve bütçe aşılırsa botlar gruplara bölünmelidir ([13](../13_BOT_ARCHITECTURE_AND_DATA_MODEL.md) §3.1 madde 4).
- `shared/SocketMgr` bir geri çağrı kancası (`SetBotTickHandler`) ve ek bir olay türü kazanır; `LogInServer` ve `AIServer` kancayı hiç kurmaz, davranışları değişmez.
- Bot sistemi kapalıyken (`[BOT] ENABLED=0`) zamanlayıcı thread'i açılmaz, kanca kurulmaz.

## Doğrulama
F2-02: `Bot_*.log` içinde tick'in zamanlayıcıdan farklı bir thread'de çalıştığı ve 100 tick'in ~10 s'de tamamlandığı satırları. F3: MET-PERF-02 (tick p95 ≤ 5 ms, 16 bot).
