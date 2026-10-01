# 19 — Kaynaklar ve Kanıtlar

> Durum: Taslak v1.0 · Araştırma tarihi: 2026-10-01 · Referans commit: `0f520272ae1f11472623d62bff76fff98562e7b3`
> Bu doküman tüm kanıt kaynaklarının dizinidir. Dokümanlardaki `dosya:satır` referansları bu commit'e aittir ve GitHub bağlantısına çevrilmiştir. Bağlantı biçimi: `https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/<dosya>#L<satır>`.

---

## 1. Kanıt etiketleri

| Etiket | Anlam | Kullanıcının 5'li ayrımındaki karşılığı |
|---|---|---|
| `[D]` | Depodaki kodla doğrulandı (kod okuması; çalışma zamanında gözlenmedi) | 1 |
| `[V]` | Sürümün yerel veri paketiyle (DB, harita dosyası) doğrulandı | 1 (depo dışı ama sürümün veri paketi) |
| `[S]` | İlgili sürüm/dönem için dış kaynakla doğrulandı | 2 |
| `[B]` | Başka sürümden (ör. resmî 1.298, modern KO) veya doğrulanmamış | 3 |
| `[Ö]` | Tasarım önerisi | 4 |
| `[A]` | Açık konu / eksik veri / çalışma zamanı testi gerekli | 5 |
| `[I]` | Koddan veya veriden çıkarım/aritmetik (kanıtın kendisi değil) | — |

## 2. Depo

| Kimlik | Kaynak | Not |
|---|---|---|
| R-REPO | https://github.com/ko4life-net/Fire-Drake-Project-v1453 — `main` @ `0f52027` (2020-10-12) | 2026-10-01'de `git ls-remote` ile upstream'in aynı commit'te olduğu doğrulandı |
| R-PR10 | Açık PR #10 (`refs/pull/10/head` = `be26438`), 2021-04 | İncelenen kapsamın dışında; riskleri [18](18_RISKS_ASSUMPTIONS_AND_OPEN_QUESTIONS.md) K-10 |
| R-ISSUE7 | Issue #7: Moradon merdiveninden düşme | KI-005 |
| R-ISSUE8 | Issue #8: guard tower ölüm duyurusu yok | Tower'ların oyuncu öldürdüğünün dolaylı kanıtı |
| R-LIC | `LICENSE` (GPLv3); GPL SSS: https://www.gnu.org/licenses/gpl-faq.html | Hukuki görüş değildir |

### 2.1 Temel kod referansları (seçme)

| Konu | Referans |
|---|---|
| Sürüm sabiti | [`shared/version.h:3`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/version.h#L3) |
| Başlatma ve zamanlayıcılar | [`GameServer/GameServerDlg.cpp:76-208`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/GameServerDlg.cpp#L76-L208), [`GameServer/GameServerDlg.cpp:376-380`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/GameServerDlg.cpp#L376-L380), [`GameServer/GameServerDlg.cpp:728-758`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/GameServerDlg.cpp#L728-L758) |
| Tek IOCP worker | [`shared/SocketMgr.cpp:44-55`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/SocketMgr.cpp#L44-L55) |
| Oturum havuzu | [`shared/KOSocketMgr.h:52-61`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/KOSocketMgr.h#L52-L61), [`shared/KOSocketMgr.h:75-119`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/KOSocketMgr.h#L75-L119) |
| `Send` sanal | [`shared/KOSocket.h:33-34`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/KOSocket.h#L33-L34) |
| `CUser::Update` | [`GameServer/User.cpp:472-533`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L472-L533) |
| Normal saldırı | [`GameServer/AttackHandler.cpp:4-93`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/AttackHandler.cpp#L4-L93) |
| R kapısı | [`GameServer/Unit.cpp:926-947`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/Unit.cpp#L926-L947) |
| Skill recast ve tip kapısı | [`GameServer/MagicInstance.cpp:355-425`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L355-L425) |
| Skill sınıf/ağaç kontrolü | [`GameServer/MagicInstance.cpp:184-188`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L184-L188), [`GameServer/MagicInstance.cpp:951-958`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L951-L958) |
| Quest kapısı (Etc) | [`GameServer/MagicInstance.cpp:267-275`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L267-L275) |
| Type4 buff kuralları | [`GameServer/MagicInstance.cpp:1613-1891`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L1613-L1891), [`GameServer/MagicProcess.cpp:268-631`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicProcess.cpp#L268-L631) |
| Type8 summon | [`GameServer/MagicInstance.cpp:2229-2453`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L2229-L2453) |
| Hasar | [`GameServer/Unit.cpp:208-394`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/Unit.cpp#L208-L394), [`GameServer/MagicInstance.cpp:2558-2717`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L2558-L2717) |
| Hedef HP bildirimi | [`GameServer/User.cpp:2322-2353`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L2322-L2353), [`GameServer/User.cpp:1966-1982`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L1966-L1982) |
| Party HP yayını | [`GameServer/User.cpp:2023-2031`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L2023-L2031) |
| Ölüm/respawn | [`GameServer/User.cpp:4727-4949`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L4727-L4949), [`GameServer/AttackHandler.cpp:95-240`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/AttackHandler.cpp#L95-L240) |
| Ronark'ta blink yok | [`GameServer/User.cpp:4525-4535`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L4525-L4535) |
| Hareket | [`GameServer/CharacterMovementHandler.cpp:4-54`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/CharacterMovementHandler.cpp#L4-L54), [`GameServer/User.cpp:2862-2881`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L2862-L2881), [`GameServer/User.cpp:3581-3607`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L3581-L3607) |
| Zone giriş kuralları | [`GameServer/CharacterMovementHandler.cpp:174-293`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/CharacterMovementHandler.cpp#L174-L293) |
| Düşmanlık/güvenli alan | [`GameServer/Unit.cpp:1219-1256`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/Unit.cpp#L1219-L1256), [`GameServer/Unit.cpp:1311-1345`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/Unit.cpp#L1311-L1345) |
| Party | [`GameServer/PartyHandler.cpp:52-443`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/PartyHandler.cpp#L52-L443), [`shared/database/structs.h:291-312`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/database/structs.h#L291-L312) |
| Chat | [`GameServer/ChatHandler.cpp:89-317`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/ChatHandler.cpp#L89-L317), [`GameServer/ChatHandler.h:8-21`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/ChatHandler.h#L8-L21) |
| Stat/skill puanı | [`GameServer/User.cpp:1765-1826`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L1765-L1826), [`GameServer/User.cpp:2971-3000`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L2971-L3000), [`GameServer/User.cpp:3856-4158`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L3856-L4158) |
| Yetenek formülleri | [`GameServer/User.cpp:1036-1103`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L1036-L1103), [`GameServer/User.cpp:2082-2312`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L2082-L2312) |
| Ekipman kontrolü | [`GameServer/ItemHandler.cpp:527-539`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/ItemHandler.cpp#L527-L539) |
| SMD yükleyici | [`shared/SMDFile.cpp:75-206`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/SMDFile.cpp#L75-L206) |
| AIServer A* | [`AIServer/PathFind.cpp`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/AIServer/PathFind.cpp), [`AIServer/MAP.cpp:124-127`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/AIServer/MAP.cpp#L124-L127) |
| NPC görünüm paketi | [`GameServer/Npc.cpp:138-155`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/Npc.cpp#L138-L155) |
| Oyuncu görünüm paketi | [`GameServer/CharacterMovementHandler.cpp:99-161`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/CharacterMovementHandler.cpp#L99-L161) |

Ayrıntılı araştırma notları (tüm satır referanslarıyla): `appendix/research/A_core.md`, `B_combat.md`, `C_map_party_chat.md`, `D_stats_items.md`, `H_aiserver.md`.

## 3. Yerel sürüm verisi

| Kimlik | Kaynak | Kullanım | Erişim |
|---|---|---|---|
| L-DB | `FDP_kn_online` (SQL Server Express, yerel; `Database.7z` geri yüklemesi) | Skill, item, pot, katsayı, seviye, zone, NPC konumları, prosedür kaynakları | Yalnızca `SELECT`, Windows kimlik doğrulaması |
| L-DB-OK | Okunan tablolar: MAGIC, MAGIC_TYPE1..9, MAGIC_BAK_etc (yalnızca toplu karşılaştırma), ITEM, ITEM_UPGRADE (örnek), SET_ITEM, COEFFICIENT, LEVEL_UP, ZONE_INFO, K_NPC, K_MONSTER, K_NPCPOS, K_OBJECTPOS, K_OBJECTEVENT, K_WARPINFO, START_POSITION(_RANDOM), HOME, BEGINNER_ITEM, VERSION, SERVER_RESOURCE, sys.sql_modules (prosedür kaynağı), USERDATA **yalnızca şema ve varsayılanlar** | | |
| L-DB-NO | **Okunmayan** (kişisel/üçüncü taraf verisi): TB_USER, ACCOUNT_CHAR, USERDATA satırları, USER_*, WAREHOUSE*, MAIL_*, FRIEND_LIST, PUS_*, _SN_*, WEB_*, CURRENTUSER, CONCURRENT, KNIGHTS*, KING_*, w_* | R-12 | |
| L-MAP | `/mnt/c/dev/fdp/server/Map/` — `freezone_a_20050718.smd` (zone 71), `freezone_b_20050718.smd` (72), `*.aievt` | Navigasyon, arena analizi | Salt okunur ayrıştırıcı |
| L-START | `start.md` (kullanıcının kurulum notu, depoda izlenmeyen dosya) | Kurulum adımları, bilinen sorunlar | — |
| L-LOGS | Sunucu logları 2026-10-01 (giriş ve canavar öldürme) | Ortamın çalıştığının kanıtı | Kimlik bilgisi içeren satırlar alıntılanmadı |
| L-EXTRACT | `appendix/data/*.csv` (skill, pot, ekipman, katsayı, seviye, Ronark NPC) ve `appendix/A1–A3` | Doküman tablolarının kaynağı | Betikle üretildi |

## 4. Dış kaynaklar

Dönem etiketi: **E** = 2006–2008 RoFD dönemi (bu sürümle uyumlu) · **O** = 2005 ve öncesi / resmî 1.298 · **L** = sonraki/modern · **U** = tarihsiz. Hepsi 2026-10-01'de erişildi (erişilemeyenler belirtilmiştir).

### 4.1 Sürüm ve proje

| Kimlik | Kaynak | Dönem | Kullanım |
|---|---|---|---|
| W-01 | Kalais haber arşivi — http://ko.kalais.net/archive.php | E | RoFD Ağustos 2006, Forgotten Frontiers Ekim 2008 (seviye 83) |
| W-02 | Wikipedia "Knight Online" rev. 2009-12-27 — https://en.wikipedia.org/w/index.php?title=Knight_Online&oldid=334218821 | E/U | RoFD 3 Ağustos 2006, stat puanı kuralları |
| W-03 | DonanımHaber 2007-07-19 — https://forum.donanimhaber.com/colony-zone-neresidir--15960415 | E | Colony Zone → Ronark Land adı |
| W-04 | ko-event 2008 — http://ko-event.blogspot.com/2008/07/iksirler-potions.html | E | Pot adları ve değerleri |
| W-05 | ko-rehber 2007-01-24 — http://ko-rehber.blogspot.com/2007/01/alternatif-yama.html | E | Ocak 2007'de USKO yaması ~1470 |
| W-06 | Wikipedia rev. 2013 — https://en.wikipedia.org/w/index.php?title=Knight_Online&oldid=587919931 | U | Genişleme takvimi |
| W-07 | kocuce "[1.453] Reign Of The Fire Drake Server Files" — https://www.kocuce.com/konular/1-453-reign-of-the-fire-drake-server-files.902/ | U | Topluluk adlandırması |
| W-08 | kofans.cn (2010) — https://www.kofans.cn/bbs/thread-13994-1-1.html | U | 1453 Fire Drake dosyaları, snoxd soyu |
| W-09 | pvpkenti — https://pvpkenti.net/threads/yenilendi-1453-orjinal-server-files-fire-drake-server-files.19523/ | U | Orijinal 1453 istemcisiyle eşleşme iddiası |
| W-26 | ko4life konu 1258 — https://ko4life.net/topic/1258-ko4life-fire-drake-project-v1453/ | — | **Erişilemedi** (Cloudflare) |
| W-28 | snoxd aynası — https://github.com/twostarz/snoxd-koserver | O/U | Kod soyu (lisans beyanı yok) |
| W-29 | ko4life-net/ko — https://github.com/ko4life-net/ko | L | Farklı kod tabanı (karıştırılmamalı) |
| W-11 | Open-KO KnightOnline — https://github.com/Open-KO/KnightOnline | O (1.298) | Resmî 1.298 istemci/sunucu mantığı (R aralığı, cast kilidi) `[B]` |
| W-12 | OpenKO-db — https://github.com/Open-KO/OpenKO-db | O (1.298) | 1.298 skill/pot değerleri (karşılaştırma) `[B]` |
| W-62 | Softonic "1453 … 10/24/2006" arama özeti | U | **Sayfa doğrulanamadı** |

### 4.2 Ronark Land ve harita

| Kimlik | Kaynak | Dönem | Kullanım |
|---|---|---|---|
| W-10 | KnightOnlineWorld Colony Zone haritası (Wayback) — http://web.archive.org/web/20050326100836/http://www.knightonlineworld.com:80/maps/colony.asp ; görsel: http://web.archive.org/web/20051004003719im_/http://www.knightonlineworld.com:80/img/maps/map_colony_lrg.jpg | O | Sunucu SMD'siyle örtüşme; kapı amblemleri ±3 m |
| W-23 | Gamia arşivi — https://gamia-archive.fandom.com/wiki/Knight_Online | U (≤2014) | NP kuralları, genişlemeler |
| W-60 | Steam 2016 — https://steamcommunity.com/app/389430/discussions/0/364039785167170364/ | L | Ronark seviye şartı 55 (bu projede **kullanılmaz**) |
| W-61 | korehberi — https://wiki.korehberi.com/Ronark_Land | L | 70+ (kullanılmaz) |

### 4.3 Sınıf mekaniği, build ve ekipman

| Kimlik | Kaynak | Dönem | Kullanım |
|---|---|---|---|
| W-13 | Kalais warrior rehberi — http://ko.kalais.net/guide-warrior.php | O | Raptor, kombolar, takılar |
| W-14 | Kalais priest rehberleri — http://ko.kalais.net/guide-priest.php , http://ko.kalais.net/guide-priest2.php | O | Priest rol ayrımı, cure sırası, konum |
| W-15 | Kalais mage — http://ko.kalais.net/mage70damage.php , http://ko.kalais.net/guide-lrmage.php | O | "Yalnızca temel MP", mage build'leri |
| W-16 | Kalais item sayfaları — http://ko.kalais.net/item-armor-war.php (ve priest/mage/aksesuar sayfaları) | O | Zırh kademeleri; yerel DB ile spot kontrol eşleşti |
| W-17 | Kalais assassin rehberi — http://ko.kalais.net/guide-rogsin.php | O | "R sonrası yarım saniye" kombo zamanlaması `[B]` |
| W-18 | DonanımHaber 2008-09-14 — https://forum.donanimhaber.com/minor-cooldown--26413532-3 | E | "720'lik potu 2 sn'de"; level 80 warrior HP/AP |
| W-19 | turkmmo 2011 warrior kombo — https://forum.turkmmo.com/konu/1508388-warrior-item-combo-rehberi/ | L/U | Kombo tanımı |
| W-20 | ko-rehber "RoFD Yetenekleri" 2007 — http://ko-rehber.blogspot.com/2007/01/rofd-yetenekleri.html | E | RoFD skill eklemeleri |
| W-21 | ko-event "Fire Drake Yeni Skiller" 2008 — http://ko-event.blogspot.com/2008/07/fire-drake-yeni-skiller.html | E | Aynı |
| W-22 | turkmmo skill listeleri 2012 (warrior/mage/priest) | L/U | Karşılaştırma; çelişkiler [03] §1.3 |
| W-24 | Kalais master questleri — http://ko.kalais.net/master.php | O/E | Master quest yapısı |
| W-30 | ko4life-net/Knight-Offline `BotBalancer.cs` — https://github.com/ko4life-net/Knight-Offline/blob/master/Knight%20Offline/BotBalancer.cs | L (2023) | 8'lik kompozisyon örnekleri (tasarım fikri) |

### 4.4 Emülatör ve bot kaynakları

| Kimlik | Kaynak | Dönem | Kullanım |
|---|---|---|---|
| W-31 | co3moz/knightonline-js paket başlıkları — https://github.com/co3moz/knightonline-js/blob/master/docs/packet_headers.md | L | Opcode isimleri (yetkili kaynak depo [`shared/packets.h`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/packets.h)) |
| W-32 | AzerothCore mod-playerbots — https://github.com/mod-playerbots/mod-playerbots | L | Sunucu içi bot emsali (çekirdek kancaları gerekir) |
| W-33 | "PK Farm Botları v24xx" paylaşımları (pvpers.gg 2025, r10dev 2026) | L | Var oldukları; içerik doğrulanamadı |

### 4.5 Yapay zekâ, navigasyon ve istatistik

| Kimlik | Kaynak | Kullanım |
|---|---|---|
| W-40 | Mark & Dill, "Improving AI Decision Modeling Through Utility Theory", GDC 2010 — https://www.gdcvault.com/play/1012410/Improving-AI-Decision-Modeling-Through | Utility scoring |
| W-41 | Graham, "An Introduction to Utility Theory", Game AI Pro 1 ch.9 — http://www.gameaipro.com/GameAIPro/GameAIPro_Chapter09_An_Introduction_to_Utility_Theory.pdf | Utility |
| W-42 | Dill, "Dual-Utility Reasoning", Game AI Pro 2 ch.3 — http://www.gameaipro.com/GameAIPro2/GameAIPro2_Chapter03_Dual-Utility_Reasoning.pdf | [13](13_BOT_ARCHITECTURE_AND_DATA_MODEL.md) §7 |
| W-43 | Lewis, "Choosing Effective Utility-Based Considerations", Game AI Pro 3 ch.13 — http://www.gameaipro.com/GameAIPro3/GameAIPro3_Chapter13_Choosing_Effective_Utility-Based_Considerations.pdf | Considerations |
| W-44 | Colledanchise & Ögren, *Behavior Trees in Robotics and AI* — https://arxiv.org/abs/1709.00084 | BT |
| W-45 | Champandard & Dunstan, "The Behavior Tree Starter Kit" — http://www.gameaipro.com/GameAIPro/GameAIPro_Chapter06_The_Behavior_Tree_Starter_Kit.pdf | BT |
| W-46 | Orkin, "Three States and a Plan" (GOAP), GDC 2006 — https://gdcvault.com/play/1013282/Three-States-and-a-Plan | GOAP |
| W-47 | Mark, "Modular Tactical Influence Maps", Game AI Pro 2 ch.30 — http://www.gameaipro.com/GameAIPro2/GameAIPro2_Chapter30_Modular_Tactical_Influence_Maps.pdf | Tehlike haritası |
| W-48 | Li, Chu, Langford, Schapire, "A Contextual-Bandit Approach…" (LinUCB) — https://arxiv.org/abs/1003.0146 | L2 |
| W-49 | OpenAI Five — https://arxiv.org/abs/1912.06680 | RL maliyeti |
| W-50 | Vinyals et al., AlphaStar, Nature 575 (2019) — https://doi.org/10.1038/s41586-019-1724-z | RL maliyeti, lig yapısı |
| W-51 | Recast/Detour — https://github.com/recastnavigation/recastnavigation | Navmesh seçeneği |
| W-52 | Reynolds, "Steering Behaviors" GDC 1999 — https://www.red3d.com/cwr/steer/gdc99/ | Yerel yönlendirme |
| W-53 | Emerson, "Crowd Pathfinding and Steering Using Flow Field Tiles", Game AI Pro 1 ch.23 | Alternatif |
| W-54 | TrueSkill — https://www.microsoft.com/en-us/research/publication/trueskilltm-a-bayesian-skill-rating-system/ | Derecelendirme |
| W-55 | Binom oran güven aralıkları (Wilson) — https://en.wikipedia.org/wiki/Binomial_proportion_confidence_interval ; Wilson 1927 — https://doi.org/10.1080/01621459.1927.10502953 | İstatistik |
| W-56 | Efron 1979 (bootstrap) — https://doi.org/10.1214/aos/1176344552 | İstatistik |
| W-57 | Wald 1945 (SPRT) — https://doi.org/10.1214/aoms/1177731118 | İstatistik |
| W-58 | Fishtest — https://github.com/official-stockfish/fishtest ; matematik — https://github.com/official-stockfish/fishtest/wiki/Fishtest-mathematics | SPRT uygulaması (satranç motoru testleri) |
| W-59 | Chess Programming Wiki, SPRT — https://www.chessprogramming.org/Sequential_Probability_Ratio_Test | SPRT |

## 5. Çelişkiler ve uygulanan karar

| Konu | Çelişki | Karar | Doğrulama |
|---|---|---|---|
| Ronark minimum seviye | Kod 35; 2007 kaynağı 45; 2016: 55; modern: 70+ | Kod (35) | — |
| Seviye sınırı | Gamia: RoFD 80; Kalais: öncesinde 70; FF 83 | Kod (80) | — |
| Blink | 1.298: 10 sn, diriltmede de; yerel: 15 sn, Ronark'ta yok | Yerel kod | T-ENV-02 (Debug'da blink kapalı) |
| Heal değerleri | 1.298 (120/240/480), RoFD listeleri (240/360/720), yerel DB (Great 960) | Yerel DB | T-MECH-SKILL-P |
| Debuff güçleri | Malice −%25 (DB) / −%20 (2012+); Torment −%30 (DB) / −%50 (2016) | Yerel DB | T-MECH-BUF |
| Party boyutu | Kod 8; modern blog 6 | Kod | — |
| R hızı | 1.298 sunucusunda sunucu saati kontrolü yok; yerel kodda saniyede 1 | Yerel kod | T-MECH-CLIENT-01 |
| İtem adları | "Mirage Sword" vs DB "Mirage"; "Secret Silver" vs "Secret-Silver Earring" | Botlar yalnızca item ID kullanır | — |
| Ronark haritası | 2005 harita = yerel SMD; 2008 ve 2012'de değişiklik iddiaları | Yerel SMD | Q-20 |

## Değişiklik günlüğü

| Tarih | Sürüm | Değişiklik |
|---|---|---|
| 2026-10-01 | v1.0 | İlk sürüm |
