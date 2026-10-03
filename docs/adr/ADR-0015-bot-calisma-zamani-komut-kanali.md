# ADR-0015: Bot çalışma zamanı komut kanalı: konsol `/bot` + komut dosyası (otonom döngüde Claude kararı; proje sahibi onayladı: 2026-10-03, ADR-0023)

Durum: KABUL (geçici, proje sahibi gözden geçirecek) · Tarih: 2026-10-02 · Karar veren: Claude (gece modu, `AUTO_LOOP=1`; kullanıcıya sorulamadı)
İlgili: F2 kapsamı "`/bot spawn/despawn` (minimum)" (`docs/17` §2), S11 (`docs/02` §11), `docs/13` §10, ADR-0005 (bot durumu IOCP thread'inde), F3 (`+bot`/`/bot` komut aileleri)

## Bağlam
F2-03..F2-05 sonunda botlar yalnızca sunucu açılışında (`[BOT] SPAWN_ON_START`) ve ini'deki zamanlayıcıyla (`DESPAWN_AFTER_SEC`, `RESPAWN_CYCLES`) yönetiliyor; çalışma zamanında tek bir bot eklenemiyor ya da çıkarılamıyor. F2 kapsamı `/bot spawn/despawn` komutunu (en az düzeyde) içeriyor. Sunucuda iki mevcut komut yolu var: konsol komutları (`ConsoleInputThread`, `/` öneki, `GameServer/ChatHandler.cpp` `InitServerCommands`) ve oyun içi GM komutları (`+`). Sorun: GameServer, `tools/run-servers.sh` ile ayrı küçültülmüş konsol penceresinde açılıyor (`Start-Process ... -WindowStyle Minimized`), konsol girişi `_kbhit()` ile klavyeden okunuyor; dolayısıyla **otomasyon (Claude doğrulaması, gece döngüsü, F3'ün senaryo koşucusu) konsola komut yazamaz** `[S]`. Bot durumu (`BotManager`) yalnızca IOCP thread'inde değiştirilebilir (ADR-0005); konsol thread'i doğrudan dokunamaz.

## Karar
Tek bir komut çekirdeği, iki giriş yolu:
- **Komut çekirdeği:** `BotManager` içinde komut satırı ayrıştırıp çalıştıran, **yalnızca IOCP thread'inde** (`Tick()` içinden) çalışan bir yürütücü. Komutlar: `spawn <ad>[,<ad>...]`, `despawn <ad|all>`, `list`. Yalnızca `BOT_TABLE`'daki 12 sabit bot (ADR-0014) kabul edilir.
- **Giriş yolu 1 — konsol:** `/bot <komut ...>` (`docs/13` §10 ile uyumlu). Konsol thread'i komutu kilitli bir kuyruğa koyar; `Tick()` kuyruğu boşaltır.
- **Giriş yolu 2 — komut dosyası:** `Tick()` saniyede bir `./BotCommands.txt` dosyasını arar (GameServer çalışma dizini); varsa **atomik olarak** `BotCommands.processing` adına yeniden adlandırır, satır satır (satır başına bir komut, `#` yorum) çalıştırır ve siler. Otomasyon (betikler, `ScenarioRunner`) bu yolu kullanır.
- Her iki yol da yalnızca `[BOT] ENABLED=1` iken etkindir; kapalıyken kuyruk/dosya yoklaması yoktur (davranış değişmez).
- `RESPAWN_CYCLES != 0` iken komutlar reddedilir (soak modu sabit bir döngü yürütür; elle müdahale ölçümü bozar).
- Sonuçlar `Bot_*.log`'a yazılır (`BotManager: cmd ...`); konsola yalnızca "komut kuyruğa alındı/bot sistemi kapalı" yazılır.

## Değerlendirilen alternatifler
| Alternatif | Artılar | Eksiler | Neden seçilmedi |
|---|---|---|---|
| Yalnızca konsol `/bot` | `docs/13` ile birebir, en küçük kod | Otomasyon konsola yazamaz; gece döngüsü ve F3 senaryo koşucusu kullanamaz | Doğrulanamaz |
| Yalnızca komut dosyası | Otomasyona uygun | İnsan için elverişsiz; `docs/13` §10'dan sapma | İki yol aynı çekirdeği paylaşıyor, maliyet düşük |
| TCP/named pipe/HTTP komut portu | Gerçek zamanlı, çift yönlü | Yeni ağ yüzeyi, kimlik doğrulama gerektirir, F2 için büyük | Gereksiz risk |
| PowerShell ile konsol penceresine tuş vuruşu enjekte etmek (`AttachConsole`/`WriteConsoleInput`) | Sunucu kodu değişmez | Kırılgan, pencere/oturum bağımlı, sunucu başına ayrı konsol | Güvenilmez |
| Konsol thread'inden doğrudan `BotManager` durumuna dokunmak | Kuyruk yok | ADR-0005 ve thread kuralını ihlal eder (`CUser` kilitsiz) | Yarış |

## Sonuçlar
- Olumlu: botlar çalışma zamanında istenen sırayla girip çıkabilir; test betikleri ve F3'ün senaryo koşucusu aynı kanalı kullanabilir; ek ağ yüzeyi yok.
- Olumsuz: komut dosyası, sunucu çalışma dizinine yazma hakkı olan herkesin bot yönetebilmesi demektir (yalnızca `ENABLED=1` test sunucusunda etkin; üretim dışı). Saniyelik yoklama IOCP thread'inde küçük bir `rename` çağrısı ekler (bot kapalıyken yok).
- F3 `+bot` (GM, oyun içi) ve `/bot scenario|start|stop` komutları bu ADR'nin kapsamı dışında; aynı çekirdeğe eklenecek.
- Geri alma: `ChatHandler.cpp`'deki tek tablo satırı ve `BotManager`'daki komut bölümü kaldırılır.

## Doğrulama
F2-06: `BotCommands.txt` ile `spawn`/`despawn all`/`list`/yeniden `spawn` senaryosu (`Bot_*.log` satırları); `ENABLED=0` iken dosyanın dokunulmadan kaldığı; kodda komut yürütücüsünün yalnızca `Tick()` çağrı zincirinden erişildiği (`grep`).


## Ek (F3-02): `match` komutu (otonom döngüde Claude kararı; proje sahibi onayladı: 2026-10-03, ADR-0023)

Tarih: 2026-10-02 · Karar veren: Claude (gece modu, `AUTO_LOOP=1`)

Komut çekirdeğine iki alt komut eklenir (aynı iki giriş yolu, yalnızca IOCP thread'inde): `match start <senaryo> [seed]` ve `match end [sonuç]`. `docs/13` §10'daki `/bot start`/`/bot stop` yerine geçmez; ScenarioRunner (F3-03) bunları `scenario`/`start`/`stop` komutlarının içinden çağırır. `match` bağımsız bir komut olarak kalır ki ScenarioRunner olmadan da (ölçüm betikleri, elle testler) maç sınırları üretilebilsin. `RESPAWN_CYCLES != 0` iken diğer komutlar gibi reddedilir. Sonuçlar `Bot_*.log`'a `BotManager: cmd match ...` satırlarıyla yazılır.


## Ek (F3-03): `scenario` komutu ve senaryo dosyası (otonom döngüde Claude kararı; proje sahibi onayladı: 2026-10-03, ADR-0023)

Tarih: 2026-10-02 · Karar veren: Claude (gece modu, `AUTO_LOOP=1`)

Komut çekirdeğine `scenario run <ad>`, `scenario stop`, `scenario status` eklenir (aynı iki giriş yolu, yalnızca IOCP thread'i). `docs/13` §10'daki `/bot scenario <yaml>` + `/bot start|stop` ikilisi yerine **tek komut ailesi** seçildi: senaryo `run` ile yüklenir **ve** başlar (ayrı "yükle, sonra başlat" adımı yok), çünkü senaryo dosyasında seed listesi ve tekrar sayısı sabittir (`docs/15` §6) ve ara durum (yüklü ama başlamamış) otomasyon için yalnızca hata yüzeyi ekler. `match start|end` bağımsız kalır; `ScenarioRunner` onları çağırır.

Senaryo dosyası `./Scenarios/<ad>.yaml` (sunucu çalışma dizini; `BotCommands.txt` ile aynı kural), **YAML'ın küçük bir alt kümesi**: üst düzey `anahtar: skaler|[liste]`, harici YAML kütüphanesi yok. Tanınmayan anahtar ve girintili/iç içe yapı **hata**dır (docs/13 §5.1'deki `teams`/`arena`/`consumables` sonraki planlarda desteklenene kadar): sessizce yok sayılan bir yazım hatası `seeds`/`repeat` gibi değerlendirme protokolünü bozardı. Desteklenen anahtarlar: `scenario_id`, `zone` (yalnızca 71), `bots`, `seeds`, `repeat`, `duration_sec`. Her koşu sonunda botlar despawn edilir (konum/HP sıfırlama olmadığı için temiz başlangıç yolu budur).

Alternatifler: JSON/INI senaryo biçimi (docs/13 YAML diyor; ileride tam YAML'a evrilme yolu açık kalsın diye aynı sözdizimi alt kümesi), `/bot scenario` + `/bot start` ayrı komutlar (yukarıdaki gerekçeyle reddedildi), tam YAML ayrıştırıcı (F3 için gereksiz bağımlılık/boyut). Geri alma: `ScenarioRunner.*` ve `ExecuteCommand`'daki tek dal kaldırılır.


## Ek (F3-04): oyun içi GM komutu `+bot` (otonom döngüde Claude kararı; proje sahibi onayladı: 2026-10-03, ADR-0023)

Tarih: 2026-10-02 · Karar veren: Claude (gece modu, `AUTO_LOOP=1`)

Komut çekirdeğine **üçüncü giriş yolu** eklenir: GM yetkili (`isGM()`) oyuncunun `+bot ...` sohbet komutu (`CUser::InitChatCommands`). `CUser::Chat` bot tick'inden farklı bir IOCP worker thread'inde çalışabildiğinden (ADR-0005) işleyici `BotManager` durumuna dokunmaz: `spawn|despawn|match|scenario` satırları olduğu gibi `EnqueueCommand`'a gider (sonuç yine `Bot_*.log`'da); `list` ise **kuyruğa girmez**, `Tick()`'in 1 sn'de bir yenilediği kilitli anlık görüntüden (`GetStatusSnapshot`) GM'e özel mesajlarla (`SendHelpDescription`) hemen yanıtlanır. Anlık görüntü ile `/bot list` log çıktısı **aynı biçimlendirme yardımcısını** (`BuildStatusLines`) kullanır; böylece otomasyonla (komut dosyası + `Bot_*.log`) doğrulanabilir bir kod yolu GM yanıtını da üretir.

Kapsam: `docs/13` §10'daki `+bot why|pause|resume|policy|verbose|testtp` bu ekte **yoktur**: karar motoru (F6+), politika deposu (F9), duraklatma ve test teleportu henüz yok; ilgili fazlarda aynı çekirdeğe eklenir. GM'e sonuç/hata yanıtı yalnızca `list` için vardır; diğer komutların sonucunu GM'e geri yollamak (yanıt kuyruğu) ayrı bir karardır.

Alternatifler: `+bot list`'i de kuyruğa alıp sonucu GM'e geri yollamak (yanıt kanalı + oturum kimliği taşıma gerektirir, F3 için büyük), `list`'i işleyicide doğrudan `m_sessions`'tan okumak (kilitsiz yarış, ADR-0005'i ihlal eder), anlık görüntüyü her tick'te yenilemek (10 Hz gereksiz). Geri alma: `ChatHandler.cpp`'deki tablo satırı + işleyici, `User.h` satırı ve `BotManager`'daki anlık görüntü bölümü kaldırılır.
