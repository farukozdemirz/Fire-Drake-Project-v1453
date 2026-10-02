# ADR-0032-DEG: Senaryo başlangıç yerleşimi ve durum sıfırlama: neden DB yazımı, canlı `CUser` ile tutarlılık

Durum: KABUL (proje sahibi, 2026-10-02: "maç başlamadan önce konum test kurulumu kapsamında belirlenebilir; maç başladıktan sonra hareket, respawn ve savaşa dönüş normal oyun mekanikleriyle olmalı; DB yazımının neden gerektiği ve canlı CUser ile tutarlılığı ADR'de açıklansın")
Tarih: 2026-10-02 · Karar veren: proje sahibi (ilk taslak: Claude değerlendirme ajanı)
İlgili: docs/15 §6a; CON-03 (gizli teleport yok), ARENA-05, MET-NAV-05; KI-013, KI-DEG-05; ADR-0002 (karakter kurulumu DB'den), F8; `docs/reports/degerlendirme-2026-10-02-ek.md` madde 3

## Bağlam

- Bot oturumu normal giriş yoluyla açılır: `WIZ_SEL_CHAR` → DB iş parçacığı `LOAD_USER_DATA` → `CUser` alanları (`GameServer/Bot/BotManager.cpp` spawn, `CharacterSelectionHandler`). **Konum, HP/MP/NP, envanter girişte `USERDATA`/`WAREHOUSE` satırından yüklenir.** `db/002_bot_characters.sql` karakterleri arena A konumunda ve `Zone = 71`'de zaten bu yolla kurar (ADR-0002).
- Bot çıkışında DB kaydı yapılır (AC-ARCH-05): konum, HP/MP/NP, envanter bir sonraki girişe taşınır. Önceki maçın ölümleri NP'yi düşürür (KI-013: NP 0 → `Regene` yok), pot tüketimi envanterde kalır, MP respawn'da dolmaz.
- Başlangıç noktaları arena merkezinden ±35 m; doğuştan yürümek 233 m (Karus) / 640 m (El Morad) sürer.
- CON-03: normal PK sırasında gizli teleport yok; eval modunda `TEST_TELEPORT` maçı geçersiz kılar.

## Karar

1. **Maç öncesi kurulum yerleşimi serbesttir; maç içi her şey normal mekanikle.** `MATCH_START` **öncesi** (Prepare), botun konumu, HP/MP/NP'si ve envanteri test kurulumu kapsamında belirlenir ve `TEST_TELEPORT` sayılmaz. `MATCH_START` **sonrasında** hiçbir DB yazımı, ışınlama veya durum değiştirme yoktur: hareket, ölüm, `WIZ_REGENE` (bind/`START_POSITION`), savaşa dönüş ve geri çekilme yalnızca gerçek oyun mekanikleriyle olur; maç içi ışınlama kurtarma teleportudur ve maçı geçersiz kılar.
2. **Kurulum yöntemi: bot satırına DB yazımı, yalnızca bot çevrimdışıyken (despawn tamamlanmış durumda).** Gerekçe aşağıdadır.
3. Her maçın başında `docs/15` §6a tablosundaki durumlar sıfırlanır ve **doğrulanır**; doğrulama başarısızsa maç `SETUP_FAIL` ile başlamaz ve geçersiz sayılır.
4. Sıfırlama yalnızca **bot karakter satırlarına** (`BOT_TABLE` üyeleri) yazar; kişisel veri tabloları okunmaz/yazılmaz (CLAUDE.md DB kuralı).

## Neden DB yazımı?

| Gereksinim | Neden normal oyun eylemi yetmez |
|---|---|
| Tam başlangıç konumu | Doğuştan yürümek tekrar başına 4–10 dk ve taraf asimetrisini başlangıca taşır (R-10 kapasite); `WIZ_WARP`/`+bot testtp` GM/test kurtarma aracıdır, eval'de maçı geçersiz kılar |
| NP ≥ 1000 | NP'yi normal oyunda yükseltmenin kısa yolu yok; KI-013 NP 0 botu doğurmaz |
| Pot/taş/scroll stoku (STK-01) | Envanter dolduran bir oyun eylemi yok (satıcı yok, `docs/11` STK-04); sunucu tarafı yardımcı gerekir |
| MP/HP tam | Respawn MP doldurmaz; potla doldurmak maç süresini kirletir |
| Önceki maç sızıntısı | Despawn kaydı bunların hepsini taşır; temizlenmezse maçlar bağımsız olmaz |

## Canlı `CUser` ile tutarlılık nasıl sağlanır?

1. **Canlı nesneye yazılmaz.** Kurulum, `CUser` yokken (oturum `DESPAWNED`: slot havuza dönmüş, `ReqUserLogOut`'un son ifadesi tamamlanmış, `m_deleted` temizlenmiş; `BotManager.cpp` despawn bekleme yolu) yalnızca **satıra** yazar. Canlı bir `CUser` varken DB satırı yazılmaz: despawn kaydı onu ezer ve bellek ile DB ayrışır.
2. **Sıra garantisi:** kurulum yazımı, çıkış kaydı ile `WIZ_SEL_CHAR` yüklemesiyle **aynı DB iş parçacığı kuyruğunda** (FIFO) çalışır: önce çıkış kaydı, sonra kurulum `UPDATE`'i, sonra giriş yüklemesi. (Elle/yan süreç SQL ile yazma, yalnızca bot `DESPAWNED` iken ve sunucu bot satırına başka bir yazar tutmuyorken kabul edilir: kayıt yolu (`ReqSaveCharacter` + `g_DBAgent.UpdateUser/UpdateWarehouseData`, örnek: `GameServer/GameServerDlg.cpp:2962-2964`) yalnızca girişli `CUser` için çalışır `[D]`.)
3. **Giriş yolu aynen:** bot, ardından normal giriş yoluyla açılır; bölge kaydı, AIServer bildirimi, ekipman/stat hesabı (`SetUserAbility`), party durumu insan girişiyle **aynı** kodla oluşur. Kurulum yalnızca satır verisini değiştirir, durum makinelerine dokunmaz.
4. **Kırpma kuralları:** `SetUserAbility` HP/MP'yi hesaplanan maksimuma kırpar (db/002: `Hp`/`Mp` 32000 yazılır). Doğrulama kırpılmış değerleri bekler.
5. **Doğrulama:** spawn sonrası botun kendi `SelfState`'i (`/bot snap`: konum, HP/MP/NP/stok) amaçlanan başlangıçla karşılaştırılır; konum toleransı ≤ 3 m, HP/MP = maksimum, NP ≥ 1000, stok = senaryo. Sapma → `SETUP_FAIL`.
6. **Yetki sınırı:** yazım yolu yalnızca `BOT_TABLE` adlarına izin verir (ad listesi dışı satırda reddeder); okunan/yazılan tablolar `USERDATA`/`WAREHOUSE` bot satırlarıdır.

## Değerlendirilen alternatifler

| Alternatif | Artılar | Eksiler | Neden seçilmedi |
|---|---|---|---|
| Doğuştan yürüyerek başla | Gizli teleport yok | Her tekrar 4–10 dk; asimetri başlangıca girer; NP/envanter yine sıfırlanmaz | Pratik değil |
| Canlı `CUser`'a sunucu içi warp (`ZoneChange`) | Bölge/AI tutarlılığı sunucu koduyla | NP/envanter/MP yine DB ister; canlı oturumda ışınlama eval'de ambiguity (CON-03) | Konum tek başına yetmez, ek yol açar |
| Maç içi `testtp` | Basit | Eval'de maçı geçersiz kılar | Kural çelişkisi |
| Bot çevrimdışıyken DB yazımı + doğrulama (seçilen) | Giriş yolu insanla aynı; tekrarlanabilir | Sıra/yetki denetimi gerekir | Seçildi |

## Sonuçlar

Olumlu: her maç aynı koşuldan başlar; durum sızıntısı yok; canlı nesneye müdahale yok. Olumsuz: `ScenarioRunner` kapsamı büyür (F8: DB yazımı, envanter doldurma, doğrulama, yetki listesi). Geri alma: `ScenarioReset` bayrağı kapalıysa mevcut davranış (yalnız despawn/spawn).

## Doğrulama

T-IGT-EVAL-01: başlangıç doğrulaması %100; art arda iki maçta `snap` değerleri eşit; `SETUP_FAIL` oranı raporlanır; DB'de bot dışı satır değişmedi (ad listesi denetimi, kişisel veri tabloları okunmadan).
