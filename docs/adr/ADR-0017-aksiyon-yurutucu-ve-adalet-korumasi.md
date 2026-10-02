# ADR-0017: `ActionExecutor` ve `BotFairnessGuard`: aksiyonlar gerçek handler üzerinden, guard saf mantık olarak `BotCore`'da (otonom döngüde Claude kararı — gözden geçirilmeli)

Durum: KABUL (geçici, proje sahibi gözden geçirecek) · Tarih: 2026-10-02 · Karar veren: Claude (gece modu, `AUTO_LOOP=1`; kullanıcıya sorulamadı)
İlgili: `docs/13` §2 (bileşenler), §3.1 (aksiyonlar doğrudan `HandlePacket`), §5.2 (`ActionType`), §8; `docs/03` §14 (CLI-01..12), MEC-MOV-01..05; `docs/16` §3.2 (`ACTION_SUBMIT`/`ACTION_RESULT`/`FAIRNESS_REJECT`); `docs/17` F4; ADR-0005, ADR-0016

## Bağlam
F4 botun tüm temel aksiyonlarını **gerçek handler'lar** üzerinden ve CLI sınırları içinde yapabilmesini ister (AC-LRN-03: bot, sunucuya gerçek oyuncunun gönderebileceğinden fazlasını gönderemez). Sunucu hareketi neredeyse hiç doğrulamaz (MEC-MOV-03: yalnızca harita sınırı), yalnızca hız alanı tavanı vardır ve aşılırsa oturumu koparır (`SpeedHackUser`). Yani adaleti sunucu değil bot tarafı sağlamak zorundadır. F4 büyüktür; hangi sırayla ve hangi biçimde yazılacağı karar ister.

## Karar
1. **Aksiyon = gerçek paket + `CUser::HandlePacket()`.** `ActionExecutor` her aksiyonu istemcinin göndereceği paketin aynısına çevirir ve `HandlePacket` ile işletir; `MoveProcess`, `SetPosition`, `m_curx` gibi iç duruma doğrudan yazan kısayol **yoktur**. Sonuç, handler'ın gerçekten etki edip etmediğine bakılarak (`ACTION_RESULT`) eşlenir; handler sessizce dönse bile (ölü, geçersiz konum) bu bir `FAILED` olarak görünür.
2. **`BotFairnessGuard` aksiyon başına saf bir fonksiyondur** (ilk kural: `BotCore::CheckMoveStep`, hız alanı tavanı CLI-05 + "ışınlanma yok" adım sınırı CLI-08), `ActionExecutor`'ın `HandlePacket`'tan **hemen önce** çağırdığı. Reddedilen aksiyon sunucuya gitmez, `FAIRNESS_REJECT` yazılır; `ACTION_SUBMIT`/`ACTION_RESULT` yalnızca sunucuya verilen aksiyonlar için yazılır (MET-ACT-02 paydası böyle temiz kalır). Sunucu hız tavanı `ServerSpeedLimit()` ile `SpeedHackUser()`'ın birebir yansımasıdır; guard botu kopma sınırının altında tutar.
3. **Saf mantık `BotCore`'da, başlık-yalnızca (`BotCore/BotMotion.h`); `GameServer` `BotCore.lib`'e bu planda bağlanmaz, göreli `#include` kullanır.** ADR-0016 "ilk tüketici `ProjectReference` ekler" dedi; ancak ilk BotCore modülü yalnızca küçük `inline` fonksiyonlardır ve `GameServer` projesine bağlantı eklemek (kütüphane yolu, `shared.lib` ile CRT uyumu, `.sln` bağımlılığı) bu dilimin riskini gereksiz artırır. İlk `.cpp` içeren modül (F5 A*) geldiğinde `ProjectReference` eklenir; başlık-yalnızca dosyalar o zaman da aynen kalır.
4. **Dilim sırası:** F4-01 hareket (`Move`/`Stop`) + guard iskeleti ve telemetri olayları (tetikleyici: `/bot move`). Sonraki dilimler aynı iskelete eklenir: saldırı (`Attack` + CLI-01/02/11), cast (`CastStart/Effect` + CLI-03/04/09), pot (`UsePotion` + CLI-06), sonra `Perception` + sözleşme denetimi, betikli test dizileri (F4 kabulü). Her dilim `ENABLED=0` iken davranışı değiştirmez.
5. **Adım zamanlaması:** yürüyüş paketi ~1,5 sn'de bir (`docs/03` §14 ölçümü), hız alanı 45 (yürüme) / 67 (sprint), paket konumu bir sonraki 1,5 sn'lik noktadır; sunucu konumu her pakette o noktaya atlar (gerçek istemci de böyle gönderir). Ölçülmeyen kısımlar `[A]` kalır (arazi yüksekliği, buff'ların hıza etkisi).
6. **CLI-08 yürünebilirlik ızgarası F4'te yoktur:** F4-01'in `move` komutu düz çizgi yürür, test noktaları elle seçilir; yürünebilirlik ve yükseklik F5'te (`docs/12`). Guard bu planda yalnızca "ışınlanma yok" parçasını (adım ≤ hız × süre × 1,10 + 0,15 m) uygular.
7. **Olay seviyesi ve kimlik:** `ACTION_SUBMIT`, `ACTION_RESULT`, `FAIRNESS_REJECT` `decisions` seviyesindedir (`docs/16` §3.3 tablosu). Karar katmanı olmadığından `decision_id` oturum başına artan bir aksiyon sayacıdır; karar katmanı gelince gerçek karar kimliğine dönüşür.

## Değerlendirilen alternatifler
| Alternatif | Artılar | Eksiler | Neden seçilmedi |
|---|---|---|---|
| Aksiyonu doğrudan `CUser` durumuna yazmak (konumu `SetPosition` ile değiştirmek) | Basit, hızlı | AC-LRN-03'ü ve "gerçek handler" ilkesini bozar; bölge/olay/`AG_USER_MOVE` yan etkileri kaçırılır; testler gerçek davranışı yansıtmaz | İlke ihlali |
| Guard'ı `ActionExecutor` içinde sunucu başlıklarına bağımlı yazmak | Tek dosya | Birim test edilemez (sunucu başlatmadan sınanamaz); `BotCore` ilkesine aykırı | Saf fonksiyon + birim test daha güvenli |
| `GameServer`'ı bu planda `BotCore.lib`'e bağlamak | ADR-0016'ya harfiyen uygun | Proje/CRT/bağımlılık riski, dilimi büyütür; ilk tüketici bir `.cpp` yazmıyor | Başlık-yalnızca ile aynı yarar, daha az risk |
| Hareketi her tick'te (100 ms) göndermek | Pürüzsüz konum | İstemci davranışından (1,5 sn) sapar, paket yoğunluğu 15 kat | CLI-05 ölçümü 1,5 sn |

## Sonuçlar
- Olumlu: bot aksiyonları gerçek istemciyle aynı yoldan geçer; adalet kuralı sunucusuz birim test edilir; hata, handler'ın sessizce yutması bile, görünür olur.
- Olumsuz: sunucu konumu 1,5 sn'lik sıçramalarla ilerler (gerçek istemciyle aynı); `GameServer` içindeki `../../BotCore/...` göreli include'u geçicidir.
- Geri alma: `BotManager.cpp` içindeki `move`/`stop` dalları ve `ActionExecutor` dosyaları kaldırılır; `[BOT] ENABLED=0` ile zaten devre dışıdır.

## Doğrulama
F4-01: birim testleri (`Motion_*`), `ENABLED=1` altında `move`/`stop`/guard reddi çalışma zamanı sınamaları (`plans/F4-01-aksiyon-yurutucu-hareket.md` §7), `ACTION_*` ve `FAIRNESS_REJECT` telemetri satırları.

## Ek (F4-02): saldırı dilimi (otonom döngüde Claude kararı — gözden geçirilmeli)

Tarih: 2026-10-02 · Karar veren: Claude (gece modu, `AUTO_LOOP=1`) · Plan: `plans/F4-02-aksiyon-yurutucu-saldiri.md`

1. **Aksiyon = gerçek `WIZ_ATTACK` + `HandlePacket`** (Karar 1'in devamı). Alan sırası `bType=1, bResult=1, tid, delaytime = silah.Delay + 10, distance = hedefe mesafe × 10` (`docs/03` T-MECH-CLIENT-02). Sunucunun `Attack()` kapıları (kör/ölü, gecikme, `FREEZE`) sessiz döndüğünde bot bunu `no_result` olarak görür.
2. **Sonuç yalnızca sunucunun yayınladığı sonuç paketinden okunur** (`WIZ_ATTACK, bType, bResult, saldıranId, tid`; `SendToRegion` saldıranı da kapsar, `CUser::Send` bot alıcısını aynı thread'de çağırır). `ACTION_RESULT` eşlemesi: `1` → `hit`, `2/3` → `killed`, `0` → `srv_fail` (MET-ACT-02 paydasına girer), paket yoksa `no_result`. Hedefin HP'sine/hasara bakılmaz (AC-LRN-03: bot, gerçek oyuncunun göremeyeceği bilgiyle sonuç çıkarmaz).
3. **Guard kuralları saf mantık olarak `BotCore/BotCombat.h`'de** (Karar 2/3 deseni): menzil (MEC-R-04, `distance ≤ m_sRange` 0,1 m biriminde; sunucunun `15 + menzil` m cömertliğinden **dar**, çünkü gerçek istemci yalnızca silah menzilinde vurur), vuruş aralığı (CLI-01: `max(Delay × 10 ms, 1000 ms)`; 1000 ms zemini MEC-R-07 + `UNIXTIME` 1 sn çözünürlüğü) ve aksiyon hızı tavanı (CLI-11: kayan pencerede ≤ 6/sn, hareket hariç). Aralık, çağıran zamanlamayla zaten sağlanır; guard'ın `TOO_SOON` kuralı savunma katmanıdır ve normal akışta tetiklenmez (yalnızca gerçek ihlalde `FAIRNESS_REJECT`).
4. **Hedef girdisi geçicidir:** `/bot attack <bot> <hedefbot> [adet]` test komutu hedef konumunu/kimliğini hedef botun oturumundan okur ve `AttackTarget` yapısıyla `ActionExecutor`'a verir. Bu, üretim algısı değildir; `Perception` dilimi (sıradaki F4 planları) kaynağı değiştirir, `AttackTarget` arayüzü kalır. Sözleşme denetimi (AC-LRN-03, "sözleşme dışı algı erişimi 0") o dilimde yapılır; o zamana dek algı okuyan tek yer `BotManager::TickSessions()`'taki test sürücüsüdür.
5. **CLI-02 bu dilimde yok:** F1 ölçümü R ile skill arasında kilit olmadığını gösterdi (`docs/03` §14); cast diliminde R ve skill bağımsız zamanlayıcılar olarak uygulanır. Saldırı hızı buff'ı (`BUFF_TYPE_ATTACK_SPEED`) algı gelene kadar uygulanmaz; aralık tavan kalır (buff yokken doğru).
6. **Ölüm/yeniden doğuş kapsam dışı:** hedef ölürse seri `killed` ile biter; `Regene`/ölüm yönetimi ayrı dilimde. Test serileri kısa (≤ 5 vuruş) ve botların HP'si 32000.
