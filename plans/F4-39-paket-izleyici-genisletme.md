# F4-39: Paket izleyici genişletme — gelen/giden yeni opcode'lar, ad sansürü ve `--cli` bölümleri (ADR-0018 Ek 2 / Ek 15)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2) |
| Branch | `bot/F4-39` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F1-01 (`FDP_PACKET_TRACE`, `PacketTrace.cpp`, `tools/packet-trace-summary.py`) — `KAPANDI`; F1-02 (`tools/trace-session.sh`, `--cli`) — `KAPANDI`; F4-38 — `KAPANDI` (gece dalında) |
| İlgili gereksinim / kabul | ADR-0018 Ek 2 (madde 11) ve Ek 15, CLI-14..CLI-20 (`docs/03` §14), T-REGENE-01, T-PARTY-01..03, T-PERC-01 (`docs/STATUS.md` "Proje sahibi testleri") |
| Tahmini büyüklük | S–M (4 dosya; yeni dosya yok, proje dosyaları değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-03 (otonom gece döngüsü) |

---

## 1. Amaç

Paket izleyici bugün yalnızca **gelen** hareket/saldırı/büyü/hedef HP/durum/speedhack paketlerini kaydediyor. İnsan istemcisiyle yapılacak CLI-14..CLI-20 zamanlama ölçümleri (yeniden doğ düğmesi, party davet/kabul/ret/ayrılma/devir/atma, sohbet, bölge değişiminde kullanıcı/NPC isteği) bu yüzden **ölçülemiyor** (`docs/STATUS.md` T-PARTY-01 satırı: ölçülemedi). Bu plan izleyiciye (a) yeni **gelen** opcode'ları (`WIZ_PARTY`, `WIZ_REGENE`, `WIZ_REQ_USERIN`, `WIZ_REQ_NPCIN`, `WIZ_CHAT`), (b) küçük bir **giden** (sunucu → oyuncu) kayıt listesi (`WIZ_DEAD`, `WIZ_REGIONCHANGE`, `WIZ_NPC_REGION`, `WIZ_REGENE`, `WIZ_PARTY`), (c) kişisel veri sansürü (chat metni ve party adları **asla** yazılmaz) ve (d) `tools/packet-trace-summary.py --cli` için CLI-14..CLI-20 bölümlerini ekler. Her şey `FDP_PACKET_TRACE` bayrağının arkasındadır; bayrak kapalıyken sunucu bit düzeyinde aynıdır.

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0018-f4-kapsam-genisletme-aksiyon-destegi.md` Ek 2 (madde 11, 2026-10-03) ve **Ek 15** (bu plan; Claude yazdı) — kapsam ve sansür kuralı.
- `docs/03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md` §14 (paket yükleri; bu planla genişletildi) ve CLI-14..CLI-20 satırları (`:391-397`; hangi sürenin ölçüleceği).
- `plans/F1-01-paket-izleyici.md` — izleyicinin ilk tasarımı (kayıt biçimi, derleme bayrağı, `tools/build.sh --packet-trace`).
- İlgili kod (dosya:satır, 2026-10-03'te doğrulandı):
  - `GameServer/PacketTrace.h:1-14` (tek fonksiyon `LogIncoming`); `GameServer/PacketTrace.cpp:19-36` `IsTracedOpcode` (7 opcode; yorum `:19-21`: "Chat, login, character selection, trade, item moves, mail and party names are never written to the log"), `:80-124` `LogIncoming` (satır biçimi `t_ms\tsid\tname\tzone\topcode_hex\tlen\tpayload_hex`, yük en çok 64 bayt).
  - `GameServer/User.cpp:300-302` gelen kanca (yalnızca oyundaki oyuncular: giriş öncesi dallar `:272`'de `return true` ile biter); `GameServer/User.cpp:19-39` `CUser::Send`/`CUser::SendCompressed` (bot alıcısı `m_botSink != nullptr` ise paket `m_botSink->OnPacket()`'e gider ve `KOSocket::Send` **çağrılmaz**).
  - `shared/KOSocket.cpp:195-198` `KOSocket::SendCompressed`: `pkt->size() < 500` ise sanal `Send(pkt)` çağırır (yani `CUser::Send` yakalar); ≥ 500 ise `WIZ_COMPRESS_PACKET` sarmalıyla `Send` çağırır (özgün opcode'u `CUser::Send` görmez).
  - `GameServer/User.h:114` `CUser::GetID()` = `GetSocketID()` (oyuncu kimliği = `sid`); `GameServer/User.h:312` `isInGame()`.
  - `shared/packets.h` (ISO-8859 olabilir; **düzenleme**, `grep -a`): `WIZ_CHAT 0x10`, `WIZ_DEAD 0x11`, `WIZ_REGENE 0x12`, `WIZ_REGIONCHANGE 0x15`, `WIZ_REQ_USERIN 0x16`, `WIZ_NPC_REGION 0x1C`, `WIZ_REQ_NPCIN 0x1D`, `WIZ_PARTY 0x2F`; party alt opcode'ları `PARTY_CREATE 1`, `PARTY_PERMIT 2`, `PARTY_INSERT 3`, `PARTY_REMOVE 4`, `PARTY_DELETE 5`, `PARTY_PROMOTE 0x1C`.
  - Yük biçimleri (okundu): `WIZ_REGENE` gelen `u8 tip` (`GameServer/User.cpp:334-336`); giden `WIZ_REGENE` `u16 x, z, y` (`AttackHandler.cpp:196-198`); `WIZ_DEAD` giden `u16 ölenKimlik` (`Unit.cpp:959-961`); `WIZ_REGIONCHANGE` giden `u16 sayı` + kimlikler (`GameServerDlg.cpp:1345-1357`, `SendCompressed`); `WIZ_NPC_REGION` giden `u16 sayı` + kimlikler (`GameServerDlg.cpp:1546-1557`); gelen `WIZ_REQ_USERIN`/`WIZ_REQ_NPCIN` `u16 sayı` + `u16` kimlikler (`User.cpp:1232-1234`); gelen `WIZ_PARTY`: `PARTY_CREATE`/`PARTY_INSERT` `u8 alt` + **ad (string)**, `PARTY_PERMIT` `u8 alt, u8 kabul`, `PARTY_PROMOTE`/`PARTY_REMOVE` `u8 alt, u16 kimlik`, `PARTY_DELETE` `u8 alt` (`PartyHandler.cpp:6-50`); giden `PARTY_PERMIT` `u8 alt, u16 davetEdenSid, string ad` (`PartyHandler.cpp:158-159` civarı, `PartyRequest`), giden `PARTY_INSERT` ad içerir; gelen `WIZ_CHAT` `u8 tip` + metin (`ChatHandler.cpp:92-96`).

## 3. Kapsam

**Yapılacaklar**

- `GameServer/PacketTrace.h/.cpp`: gelen kayıt listesine `WIZ_PARTY`, `WIZ_REGENE`, `WIZ_REQ_USERIN`, `WIZ_REQ_NPCIN`, `WIZ_CHAT` eklenir; yeni `LogOutgoing` ve giden kayıt listesi (`WIZ_DEAD`, `WIZ_REGIONCHANGE`, `WIZ_NPC_REGION`, `WIZ_REGENE`, `WIZ_PARTY`); **yük sansürü** (§5.1): chat metni ve party adları dosyaya hiç yazılmaz.
- Satır biçimi: gelen satırlar **bayt bayt eskisi gibi** (7 sütun); giden satırlar sonuna **8. sütun** `out` alır.
- `GameServer/User.cpp`: `CUser::Send` ve `CUser::SendCompressed`'a `#ifdef FDP_PACKET_TRACE` kancaları (giden kayıt). Bot alıcısı olan oturumların aldığı paketler de kaydedilir (çalışma zamanı doğrulaması botla yapılabilsin diye).
- `tools/packet-trace-summary.py`: 7 veya 8 sütunlu satırları okur; yeni opcode adları; `--cli` çıktısına CLI-14..CLI-20 bölümleri; `--selftest` vakaları.

**Kapsam dışı (yapılmayacak)**

- Chat **metnini**, party/karakter **adlarını** ya da giriş/şifre/çanta/ticaret/mail paketlerini kaydetmek. Sansür §5.1'deki beyaz listedir; beyaz listede olmayan bayt yazılmaz.
- Sunucu mantığını değiştirmek: `Send`/`SendCompressed` dönüş değerleri ve davranışı aynı kalır (kanca yalnızca gözlemler).
- `WIZ_USER_INOUT`, `WIZ_NPC_INOUT`, `WIZ_MAGIC_PROCESS` giden yönü, `WIZ_ITEM_MOVE` ve diğer opcode'lar; paket sıkıştırması ≥ 500 bayt dışındaki "toplu" giden yollar (`Send_PartyMember`, `SendToRegion` zaten `CUser::Send`'e iner; ayrıca dokunulmaz).
- `tools/trace-session.sh`, `tools/build.sh`, `.vcxproj` dosyaları, `BotCore/`, `GameServer/Bot/`, `docs/`, ADR (Claude yazdı).
- Ölçüm sonuçlarını `docs/03`'e işlemek (proje sahibi testi sonrası, Claude).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `GameServer/PacketTrace.h` | değiştir | `LogOutgoing` bildirimi; UTF-8 **BOM'lu**, CRLF — koru |
| `GameServer/PacketTrace.cpp` | değiştir | Opcode listeleri, sansür, ortak yazıcı, `LogOutgoing`; UTF-8 **BOM'lu**, CRLF — koru |
| `GameServer/User.cpp` | değiştir | Yalnızca `Send`/`SendCompressed` kancaları + dosya-statik yardımcı; UTF-8 **BOM'lu**, CRLF — koru |
| `tools/packet-trace-summary.py` | değiştir | ASCII; mevcut `--selftest` assert'leri değişmez, yalnızca eklenir |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. (`proj-GameServer.vcxproj*` değişmez: yeni dosya yok.)

## 5. Uygulama adımları

### 5.1 `GameServer/PacketTrace.cpp` ve `.h`

1. Branch: `git switch -c bot/F4-39 gece/2026-10-02`. Plan `Durum` satırını `UYGULANIYOR` yap. `git switch` öncesi `./tools/run-servers.sh status` `[UP]` ise `./tools/run-servers.sh stop`.
2. `PacketTrace.h`: `LogIncoming`'in yanına
   ```cpp
   // Writes a packet the server sends to an in-game player (or bot) to the trace file.
   // Called from CUser::Send / CUser::SendCompressed, which run on arbitrary threads.
   void LogOutgoing(uint16 sid, const char * charName, uint8 zone, const Packet & pkt);
   ```
3. `PacketTrace.cpp`:
   - `IsTracedOpcode` → `IsTracedIncoming`: mevcut yedi opcode **artı** `WIZ_PARTY`, `WIZ_REGENE`, `WIZ_REQ_USERIN`, `WIZ_REQ_NPCIN`, `WIZ_CHAT`. Yeni `IsTracedOutgoing`: `WIZ_DEAD`, `WIZ_REGIONCHANGE`, `WIZ_NPC_REGION`, `WIZ_REGENE`, `WIZ_PARTY`. Dosya başındaki yorumu güncelle (kayıt listeleri + sansür kuralı; İngilizce).
   - **Sansür:** `size_t KeptPayloadBytes(bool outgoing, uint8 opcode, const uint8 * data, size_t len)` (dosya-statik) yazılabilecek **en çok** yük baytı sayısını döndürür:
     - gelen `WIZ_CHAT`: `min(len, 1)` (yalnızca sohbet tipi; metin yok);
     - giden `WIZ_PARTY`: `min(len, 1)` (yalnızca alt opcode; `PARTY_PERMIT`/`PARTY_INSERT` ad taşır);
     - gelen `WIZ_PARTY`, `len > 0` iken alt opcode `data[0]`: `PARTY_PERMIT` → `min(len, 2)`; `PARTY_PROMOTE` (`0x1C`) veya `PARTY_REMOVE` (4) → `min(len, 3)`; diğerleri (`PARTY_CREATE`/`PARTY_INSERT` ad taşır, `PARTY_DELETE`) → `min(len, 1)`;
     - diğer tüm opcode'lar: `len`.
     Yazılan yük = ilk `min(KeptPayloadBytes, 64)` bayt (hex). `len` sütunu **gerçek** yük boyutunu yazmaya devam eder (araç sohbet/ad uzunluğunu buradan görebilir). Yük boşsa `-` (mevcut davranış).
   - `LogIncoming` ve `LogOutgoing` aynı dosya-statik yazıcıyı (`WriteRecord(sid, charName, zone, pkt, outgoing)`) çağırır: aynı mutex, aynı lazy dosya, aynı `t_ms` saati, aynı `SanitizeName`. `outgoing == true` iken satırın sonuna `\tout` eklenir (7. sütundan sonra, `\n`'den önce); gelen satırın baytları **değişmez**.
   - Opcode süzgeci kilitten **önce** çalışmalı (izlenmeyen paket kilide girmez; `LogOutgoing` her `Send`'de çağrılacağı için sıcak yol).
4. Yeni `printf`/konsol çıktısı yok.

### 5.2 `GameServer/User.cpp`

1. Dosya-statik, `#ifdef FDP_PACKET_TRACE` içinde, `CUser::Send`'den **önce**:
   ```cpp
   static void TraceOutgoing(CUser * pUser, Packet * pkt)
   {
   	if (pUser->isInGame())
   		PacketTrace::LogOutgoing(pUser->GetSocketID(), pUser->GetName().c_str(), pUser->GetZoneID(), *pkt);
   }
   ```
   (`isInGame()`, `GetSocketID()`, `GetName()`, `GetZoneID()` `CUser`'ın dışarıdan erişilebilir üyeleri mi kontrol et; değilse `friend`/üye yapma, **dur ve sor**.)
2. `CUser::Send`: bot dalında `m_botSink->OnPacket(*pkt)` **çağrısından önce** `TraceOutgoing(this, pkt)`; gerçek istemci yolunda `bool ok = KOSocket::Send(pkt);` sonrası `if (ok) TraceOutgoing(this, pkt);` ve `return ok;`. Bayrak kapalıyken fonksiyonun derlenmiş hâli **eskisiyle aynı** olmalı (`#ifdef` ile tüm eklemeler çıkar; `bool ok` değişkeni de `#ifdef` içinde ya da `return KOSocket::Send(pkt)` biçimi korunacak şekilde yaz).
3. `CUser::SendCompressed`: bot dalında `m_botSink->OnPacket(*pkt)` öncesi `TraceOutgoing(this, pkt)`; gerçek istemci yolunda **yalnızca `pkt->size() >= 500` iken** `KOSocket::SendCompressed(pkt)` başarılıysa `TraceOutgoing(this, pkt)` (500'den küçük paketleri `KOSocket::SendCompressed` zaten `CUser::Send`'e indirir; çift kayıt olmasın). Eşiğin kaynağını yorumla belirt (`shared/KOSocket.cpp` `SendCompressed`, İngilizce).
4. Bu ikisi dışında `User.cpp`'ye dokunma. Gelen kanca (`:300-302`) olduğu gibi kalır.

### 5.3 `tools/packet-trace-summary.py`

1. `OPCODE_NAMES`'e ekle: `0x10 WIZ_CHAT`, `0x11 WIZ_DEAD`, `0x12 WIZ_REGENE`, `0x15 WIZ_REGIONCHANGE`, `0x16 WIZ_REQ_USERIN`, `0x1C WIZ_NPC_REGION`, `0x1D WIZ_REQ_NPCIN`, `0x2F WIZ_PARTY`. Başlık yorumunu (7 sütun + isteğe bağlı `out`) ve `USAGE`'ı güncelle.
2. `parse_line`: 7 sütun ⇒ `"dir": "in"`; 8 sütun ve 8. sütun tam olarak `out` ⇒ `"dir": "out"`; başka her şey (6 veya 9 sütun, 8. sütun `out` değilse) `None`. Mevcut doğrulamalar (int/hex/`len >= 0`) aynen kalır.
3. Mevcut (7 opcode) bölümler yalnızca `dir == "in"` satırlarıyla çalışır: `analyze`, `write_report` ve `parse_cli_records` girdisinde giden satırlar süzülür (mevcut çıktı eski kayıtlarda **birebir aynı** kalmalı: eski kayıtların hepsi gelen). `write_report` sonuna, giden satır varsa, `outgoing summary:` başlığı ve opcode başına `count=` satırı eklenir; yoksa hiçbir şey yazılmaz.
4. `parse_cli_records`: yeni gelen opcode'lar için kayıt üret (kısa/geçersiz yükler `bad_len` sayar, mevcut sözleşme):
   - `0x12` gelen: `u8 tip` (en az 1 bayt) → `{"op": 0x12, "dir": "in", "type": tip}`; giden `0x12`: kayıt `{"op": 0x12, "dir": "out"}` (yük çözülmez).
   - `0x11` giden: `u16` kimlik (tam 2 bayt) → `{"op": 0x11, "dir": "out", "id": id}`.
   - `0x15`/`0x1C` giden: `u16` sayı (en az 2 bayt; sansürsüz ilk 64 bayt yeter) → `{"op", "dir": "out", "count": sayı}`.
   - `0x16`/`0x1D` gelen: `u16` sayı (en az 2 bayt) → `{"op", "dir": "in", "count": sayı}`.
   - `0x2F`: ilk bayt alt opcode `sub`; gelen `PARTY_PERMIT` (2) ikinci bayt `accept` (0/1; 2 bayttan kısaysa `bad_len`); gelen `PARTY_PROMOTE`/`PARTY_REMOVE` `u16` hedef kimlik `target` (3 bayttan kısaysa `bad_len`) → `{"op": 0x2F, "dir", "sub", "accept"?, "target"?}`.
   - `0x10` gelen: `u8 tip` (en az 1 bayt) ve gerçek yük boyutu `len` (satırın `len` sütunu; `parse_cli_records` için `row["len"]` kullan) → `{"op": 0x10, "dir": "in", "type": tip, "len": len}`.
   Mevcut opcode'ların (0x06, 0x08, 0x22, 0x31, 0x41) çözümü aynı kalır; kayıtlara `"dir": "in"` eklenebilir.
5. İki yardımcı:
   - `first_after_each(a_times, b_times)`: sıralı `a_times`'ın her `a` öğesi için, `a`'dan **sonra** ve bir sonraki `a`'dan **önce** (eşit zaman sonraki `a`'ya ait değil: `b > a` ve `b < next_a`) ilk `b`'yi bulur; `[b - a, ...]` döner (eşleşmeyen `a` listeye girmez; eşleşmeyen sayısı çağıranda `len(a_times) - len(sonuç)`).
   - `last_before_each(b_times, a_times)`: sıralı `b_times`'ın her `b` öğesi için `a <= b` olan **son** `a`'yı bulur; `[b - a, ...]` döner (önceki `a` yoksa o `b` atlanır; sayısı çağıranda `len(b_times) - len(sonuç)`).
6. `write_cli_sections`'a (mevcut bölümlerden **sonra**, mevcut satırlara dokunmadan), hedef `sid` = `rows[0]["sid"]` olmak üzere şu bölümleri ekle. **Her bölüm başlığı ve satır önekleri aşağıdaki gibi sabittir** (selftest ve doğrulama bunlara bakar); `stats` = mevcut `format_stats_short` (`n=… p5=… p50=… p95=… min=… max=…`):
   - `== CLI-14 yeniden dogus (WIZ_DEAD -> WIZ_REGENE) ==`: `DEAD own count=<n>` (giden `0x11`, `id == sid`), `REGENE in count=<n>`, `REGENE type top5: <top5>`, `DEAD->REGENE gap_ms <stats>` (`first_after_each(kendi ölümleri, gelen 0x12 zamanları)`), `DEAD->REGENE unmatched=<k>`.
   - `== CLI-15/16/17 party (WIZ_PARTY) ==`: `PARTY in count=<n> out count=<m>`; `PARTY in sub top5: <top5 (alt opcode sayısal)>`; `PARTY create/insert interval_ms <stats>` (gelen alt opcode 1 ve 3 zamanlarının ardışık farkları); `PARTY permit-in->accept gap_ms <stats>` (giden alt opcode 2 zamanlarından gelen `PERMIT accept=1` zamanlarına, `last_before_each`); `PARTY permit-in->decline gap_ms <stats>` (aynı, `accept=0`); `PARTY manage interval_ms <stats>` (gelen `PROMOTE` ve `target != sid` `REMOVE` zamanlarının ardışık farkları); `PARTY leave kind: self_remove=<a> other_remove=<b> delete=<c> promote=<d>` (gelen alt opcode 4 `target == sid`, 4 `target != sid`, 5, `0x1C`); `PARTY insert-out->leave gap_ms <stats>` (giden alt opcode 3 zamanlarından, gelen `REMOVE target == sid` ve `DELETE` zamanlarına, `last_before_each`).
   - `== CLI-18 chat (WIZ_CHAT) ==`: `CHAT count=<n>`; `CHAT interval_ms <stats>`; `CHAT type top5: <top5>`; `CHAT len top5: <top5 (satırın len sütunu)>`. Metin hiçbir yerde yazılmaz.
   - `== CLI-19 bolge degisimi kullanici (WIZ_REGIONCHANGE -> WIZ_REQ_USERIN) ==`: `REGIONCHANGE out count=<n>`; `REGIONCHANGE ids top5: <top5 (count)>`; `REQ_USERIN in count=<n>`; `REQ_USERIN interval_ms <stats>`; `REQ_USERIN ids per request top5: <top5 (count)>`; `REQ_USERIN ids max=<en büyük count veya n/a>`; `REGIONCHANGE->REQ_USERIN gap_ms <stats>` (`last_before_each(istekler, bildirimler)`); `REQ_USERIN unmatched=<k>`.
   - `== CLI-20 bolge degisimi npc (WIZ_NPC_REGION -> WIZ_REQ_NPCIN) ==`: CLI-19 ile aynı satırlar, `0x1C`/`0x1D` için (`NPC_REGION`, `REQ_NPCIN` öneki).
   Giden satır yoksa ilgili sayılar `0`, istatistikler `n=0 p5=n/a …` çıkar (mevcut `format_stats_short` davranışı); bölümler her zaman yazılır.
7. `--selftest`'e (mevcut assert'lere dokunmadan) şunları ekle (yeni veri satırları ayrı liste):
   - `parse_line`: 7 sütun `dir == "in"`; 8 sütun `...\tout` `dir == "out"`; 8. sütun `xyz` ⇒ `None`; 6 sütun ⇒ `None`; 9 sütun ⇒ `None`.
   - Eski 7 opcode'lu mevcut `lines` listesi için `write_report` çıktısı **değişmedi** (selftest'te çıktıyı `outgoing summary:` içermediğini assert et).
   - Giden satırlı bir `write_report`'ta `outgoing summary:` ve `WIZ_REGIONCHANGE (15): count=` görünür.
   - CLI-14: sid 5, `t=1000` giden `0x11` `id=5`, `t=1100` giden `0x11` `id=9` (başkasının ölümü, sayılmaz), `t=4200` gelen `0x12` tip 1 ⇒ `DEAD own count=1`, `DEAD->REGENE gap_ms` `n=1 ... p50=3200`; ikinci ölüm `t=9000` ve cevapsız ⇒ `unmatched=1`.
   - CLI-15/16/17: gelen `PARTY_CREATE` (`t=0`, yük `01`, `len` 9) ve `PARTY_INSERT` (`t=1500`, yük `03`) ⇒ `create/insert interval_ms` `n=1 ... p50=1500`; giden `PARTY_PERMIT` `t=2000` + gelen `PERMIT accept=1` `t=3400` ⇒ `permit-in->accept` `n=1 ... p50=1400`; giden `PARTY_PERMIT` `t=5000` + gelen `PERMIT accept=0` `t=6300` ⇒ `permit-in->decline` `p50=1300`; gelen `PARTY_REMOVE` yük `04 0500` (`target = 5 == sid`) ⇒ `self_remove=1`; gelen `PARTY_REMOVE` `04 0900` ⇒ `other_remove=1`; kısa yük (`PARTY_PERMIT` tek bayt) ⇒ `bad_len`.
   - CLI-18: üç gelen `WIZ_CHAT` (`t=0, 4100, 8300`; yük `01`; `len` 12, 12, 30) ⇒ `CHAT count=3`, `CHAT interval_ms` `p50=4100`, `CHAT len top5: 12:2, 30:1`.
   - CLI-19: giden `0x15` yük `0300` (`count=3`) `t=100`; gelen `0x16` yük `0200 0100 0200` `t=450`, `t=1500` (ikinci istek aynı bildirimden) ⇒ `REQ_USERIN in count=2`, `REGIONCHANGE->REQ_USERIN gap_ms` `n=2` `min=350 max=1400`, `REQ_USERIN ids max=2`, `REQ_USERIN interval_ms` `p50=1050`.
   - CLI-20 için bir eş durum (`0x1C`/`0x1D`).
   - Sondaki `selftest OK` çıktısı korunur; çıkış 0.

### 5.4 Derle ve sına

`./tools/build.sh Release` (bayraksız), `./tools/build.sh Release --packet-trace` (izleyicili; `build/` çıktısını commit etme; **izleyicili derlemeyi bırakma**: işin sonunda `./tools/build.sh Release` ile normal derlemeye dön), `python3 tools/packet-trace-summary.py --selftest`. **Sunucuyu çalıştırma** (çalışma zamanı doğrulamasını Claude yapar, bkz. §7).

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` hatasız biter (yeni uyarı yok); `./tools/build.sh Release --packet-trace` hatasız biter (yeni uyarı yok).
- [ ] K2: `python3 tools/packet-trace-summary.py --selftest` çıkış kodu 0 ve son satır `selftest OK`; selftest §5.3 madde 7'deki tüm vakaları içerir (kod okumasıyla).
- [ ] K3: Eski kayıtlarla gerilemesizlik: `plans/_logs/trace/` altında bir `*.log` varsa (yoksa selftest verisi) `python3 tools/packet-trace-summary.py <log>` ve `... <log> --cli` çıktısının **mevcut satırları** değişmeden durur; `--cli` çıktısına yalnızca yeni bölümler eklenir (`git stash` ile eski betikle karşılaştırma ya da selftest eşdeğeri; raporda yöntemi yaz).
- [ ] K4: Sansür: `grep -n "KeptPayloadBytes" GameServer/PacketTrace.cpp` hem `LogIncoming` hem `LogOutgoing` yolunun kullandığı ortak yazıcıda tek çağrı gösterir; `WIZ_CHAT`, `WIZ_PARTY` kolları §5.1'deki baytları (1 / 1 / 2 / 3) verir (kod okumasıyla); yazıcıda `contents()` ile okunan bayt sayısı `KeptPayloadBytes` ve 64'ün küçüğünü aşmaz.
- [ ] K5: Gelen satır biçimi değişmedi: `LogIncoming` çıktısının `fprintf` biçim dizesi gelen için 7 sütunlu eski haliyle birebir aynı; giden satırın sonunda tek `\tout` var.
- [ ] K6: Bayrak kapalıyken davranış aynı: `git diff gece/2026-10-02...bot/F4-39 -- GameServer/User.cpp` içindeki **tüm** eklenen satırlar `#ifdef FDP_PACKET_TRACE … #endif` içindedir (kaldırılan satır yok ya da yalnızca bayrak kapalıyken eşdeğer biçime çevrilen `return` satırları); `GameServer/PacketTrace.h/.cpp` zaten bütünüyle `#ifdef FDP_PACKET_TRACE` içinde.
- [ ] K7: `CUser::SendCompressed`'de çift kayıt yok: gerçek istemci yolunda kanca yalnızca `pkt->size() >= 500` koşulu altında (kod okumasıyla); `CUser::Send` yolunda yalnızca `KOSocket::Send` başarılıyken.
- [ ] K8: `git diff --stat gece/2026-10-02...bot/F4-39` yalnızca §4'teki 4 dosyayı (ve plan dosyasını) gösterir; `GameServer/proj-GameServer.vcxproj*` değişmemiş; `tools/trace-session.sh`, `tools/build.sh`, `GameServer/Bot/`, `BotCore/`, `docs/` değişmemiş.
- [ ] K9: Kodlama/satır sonu korunmuş: `file GameServer/PacketTrace.h GameServer/PacketTrace.cpp GameServer/User.cpp` çıktısı değişiklik öncesiyle aynı (UTF-8 BOM + CRLF), `file tools/packet-trace-summary.py` ASCII; `git diff --check` boş.
- [ ] K10: `GameServer/PacketTrace.cpp` içinde `printf(` (yalnızca `fprintf`/`snprintf` olabilir), `Sleep`, `rand(` yok.
- [ ] K11 (Claude, `/plan-dogrula` çalışma zamanı): §7 senaryoları S1–S4 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Release --packet-trace
python3 tools/packet-trace-summary.py --selftest
git diff --stat gece/2026-10-02...bot/F4-39
git diff gece/2026-10-02...bot/F4-39 -- GameServer/User.cpp
git diff gece/2026-10-02...bot/F4-39 -- GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters tools/trace-session.sh tools/build.sh
grep -an "KeptPayloadBytes\|IsTracedIncoming\|IsTracedOutgoing" GameServer/PacketTrace.cpp
grep -n "printf(\|Sleep\|rand(" GameServer/PacketTrace.cpp
file GameServer/PacketTrace.h GameServer/PacketTrace.cpp GameServer/User.cpp tools/packet-trace-summary.py
git diff --check gece/2026-10-02...bot/F4-39
./tools/build.sh Release             # son adım: izleyicisiz normal derlemeye dön
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** izleyicili derleme (`./tools/build.sh Release --packet-trace`), `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop` ve **izleyicisiz** `./tools/build.sh Release`. Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`). Botlar: Karus `BotWP_K`, `BotMI_K`, `BotMF_K`; zone 71. Kayıt dosyası `…/server/Logs/PacketTrace_<g>_<a>_<y>.log` (karakter adı içerir: **kopyalanmaz, commit edilmez, rapora ad yazılmaz**; sayılar ve örnek satırların yalnızca `opcode/len/payload` sütunları rapora girer).

1. **S1 (gelen, bot):** `spawn BotWP_K,BotMI_K` → `pinvite BotWP_K BotMI_K` → 2 sn sonra `paccept BotMI_K` → `pchat BotWP_K <metin>` (≥ 4 sn arayla iki kez, metin `selamlar-test-1`, `-2`) → `pleave BotMI_K`. Beklenen: dosyada `2f` (`WIZ_PARTY`) gelen satırları (yani 8. sütunu olmayan): `PARTY_CREATE` satırının payload sütunu **tam `01`**, `len` > 1; `PARTY_PERMIT` satırı `0201`; `PARTY_REMOVE` satırı `04` + 2 bayt; iki `10` (`WIZ_CHAT`) satırı payload sütunu **tam `01`** (tip) ve `len` metin uzunluğundan büyük; dosyada `selamlar`, `BotMI_K`, `BotWP_K` karakter dizisi **yalnızca 3. sütunda** (oturum adı) görünür, hiçbir payload sütununda görünmez (`awk -F'\t' '{print $7}' | grep -ac "73656c616d6c6172"` vb. hex ve düz arama, sonuç 0).
2. **S2 (gelen, yeniden doğuş ve bölge isteği):** `spawn BotWP_K` → bir bot ölü iken (`attack` ya da kule yakınında) `regene` (≥ 3 sn sonra); ayrıca bot iki bölge arası `move`: gelen `12` (`WIZ_REGENE`) satırı payload `01`; bot gözlem akışı `REQ_USERIN`/`REQ_NPCIN` (otomatik, `TickUserIn`/`TickNpcIn`) üretirse `16`/`1d` gelen satırları `u16 sayı` + kimlikler biçiminde ve sayı ≤ 32.
3. **S3 (giden, bot alıcısı):** aynı koşuda botun **aldığı** paketler 8. sütunu `out` olan satırlar olarak görünür: ölüm için `11` (`WIZ_DEAD`, `len` 2, payload = ölenin `sid`'i little-endian), bölge değişiminde `15`/`1c` (`len` ≥ 2), regene sonrası giden `12`, party daveti için giden `2f` `02` satırı (**payload tam `02`**, ad yok) ve `PARTY_INSERT` giden satırı (**payload tam `03`**). Gelen satırların 7 sütunlu biçimi değişmemiş (`awk -F'\t' 'NF==7'` ve `NF==8` sayıları toplamı satır sayısı; `NF==8` satırlarının hepsinin 8. sütunu `out`).
4. **S4 (özet aracı):** `python3 tools/packet-trace-summary.py <log> --cli --sid <botSid>` (bot oturum kimliği `Logs/Bot_*.log`'dan): çıktıda `== CLI-14 …`, `== CLI-15/16/17 …`, `== CLI-18 …`, `== CLI-19 …`, `== CLI-20 …` başlıkları var; `DEAD own count`, `REGENE in count`, `PARTY in count`, `CHAT count` S1/S2'deki işlem sayılarıyla uyuşur; `bad_len: 0`; çıktıda sohbet metni ve ad yok.
5. **S5 (gerilemesiz, izleyicisiz derleme):** `./tools/build.sh Release` (bayraksız) sunucu açılır, `[BOT] ENABLED=0` ile ve `ENABLED=1` ile `spawn`/`list`/`despawn` çalışır; `Logs/PacketTrace_*.log` **oluşmaz** (bayrak kapalı).

Beklenmeyen/ölçülemeyen (ör. S2'de botun otomatik `REQ_USERIN` üretmemesi) bu planın hatası değil, **sonuç olarak raporlanır**. Gerçek insan istemcisiyle ölçüm (T-REGENE-01, T-PARTY-01..03, T-PERC-01) bu plana dahil değildir; plan bunları **yapılabilir** kılar (`docs/STATUS.md` "Proje sahibi testleri").

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §Kod kuralları. Üç `GameServer/*.cpp/.h` dosyası **UTF-8 BOM'lu + CRLF**'dir (BOM'u ve CRLF'yi koru, yeni satırlar CRLF; `PacketTrace.cpp`'nin ilk baytı BOM'dur). `tools/packet-trace-summary.py` ASCII + LF (mevcut haliyle kalsın; `file` çıktısı değişmesin). Kod yorumları İngilizce.
- **Kişisel veri (kesin kural):** izleyici dosyasına chat metni, party/karakter adı ya da çanta/giriş verisi **hiçbir koşulda** yazılmaz. Sansür beyaz listedir: `KeptPayloadBytes`'ta açıkça sayılan baytlar dışında hiçbir bayt yazılmaz; emin olmadığın opcode için `0` bayt (`-`) yaz ve raporda sor. Karakter adı **3. sütunda zaten** vardır (F1-01 tasarımı; `SanitizeName`); `tools/trace-session.sh` ve `docs/` bunu uyarıyor. Kayıt dosyaları commit edilmez ve rapora kopyalanmaz.
- Thread kuralı: `LogOutgoing` her thread'den (IOCP worker'lar, zamanlayıcılar) çağrılabilir; izlenmeyen opcode kilide girmez, izlenenlerde `g_traceMutex` altında yazılır (mevcut kalıp). `CUser::Send`/`SendCompressed` içinde başka kilit alma, `Send`'in içinden yeniden `Send` çağırma yok.
- Sunucu davranışı değişmez: `Send`/`SendCompressed` dönüş değeri ve yan etkileri aynı; kanca paketi değiştirmez. Bot sistemi kapalıyken (varsayılan) ve izleyici bayrağı kapalıyken hiçbir şey değişmez.
- Bot alıcılı oturumların aldığı paketlerin kaydı yalnızca **doğrulama kolaylığı** içindir (S3); izleyici ancak özel derlemede vardır. Bu, bot kodunun paketlere bakma biçimini (AC-LRN-03) etkilemez: `GameServer/Bot/` değişmez.
- Risk: `KOSocket::SendCompressed` eşiği (500) başka yerde değişirse çift/eksik kayıt oluşur; yorumla bağlanır. Risk 2: giden `WIZ_PARTY` alt opcode 3'ü (`PARTY_INSERT`) hem katılana hem mevcut üyelere gider (hata kodu biçimi de var); araç bunu "party üyelik değişimi" olarak sayar, `insert-out->leave` ölçüsü `[A]` kalır.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum:
- Branch / commit'ler:
- Değişen dosyalar ve neden:
- Derleme sonucu (`tools/build.sh Release` ve `--packet-trace` son 10 satır):
- Kabul kriterleri öz-değerlendirme:
- Plandan sapmalar ve gerekçeleri:
- Açık sorular:

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)
