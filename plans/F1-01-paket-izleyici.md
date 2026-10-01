# F1-01: Paket izleyici (derleme bayrağıyla kapalı) ve özet betiği

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F1 — Veri ve mekanik doğrulama (`docs/17` §2) |
| Branch | `bot/F1-01` (taban: `bot/F0-02`) |
| Bağımlı olduğu planlar | F0-02 (DOĞRULANDI; `main`'e henüz birleşmedi, bu yüzden dal zincirli) |
| İlgili gereksinim / kabul | T-MECH-CLIENT-01..04 (`docs/15` §4.1), Q-01, Q-02, Q-18 (`docs/18` §3) |
| Tahmini büyüklük | S (5 dosya yeni/değişen + 1 betik) |
| Hazırlayan / tarih | Claude / 2026-10-01 |

---

## 1. Amaç

GameServer'a, **yalnızca özel bir derleme bayrağıyla** (`FDP_PACKET_TRACE`) açılan bir paket izleyici eklenmek: oyundaki bir oyuncunun istemciden gelen paketlerinin opcode'u, milisaniye zamanı ve ham baytları bir log dosyasına yazılır. Ayrıca bu logdan ölçüm tablosu (opcode başına sayı, ardışık paket aralıkları) üreten bir Python betiği eklenir. Bayrak kapalıyken sunucu **bit düzeyinde aynı davranır** (kod derlenmez bile).

Amaç sonraki planda (F1-02) insan istemcisiyle yapılacak zamanlama kaydının (T-MECH-CLIENT-01..04) ölçüm altyapısını hazırlamaktır. Bu planda istemciyle oyuna girilmez.

## 2. Bağlam (okunması zorunlu)

- `docs/17` §2 "F1" — kapsam: "Debug-only paket izleme loglayıcısı… (derleme bayrağıyla)". Dikkat: "Debug-only" burada "geliştirici derlemesi" demektir. Bot zamanlama testleri Release gerektirdiği için (`docs/02` §2.1) bayrak `DEBUG`'dan **bağımsız** olacak.
- `docs/02` §2.1 — Debug ve Release farkları. Debug'da zaman aşımları/kapılar kapalıdır; zamanlama ölçümü Release'te yapılmalı.
- `docs/03` §14 — paket yük (payload) düzenleri (WIZ_MOVE, WIZ_ATTACK, WIZ_MAGIC_PROCESS, WIZ_TARGET_HP). Bu planda yük **çözülmez**, ham hex yazılır; çözme betiğin işidir.
- `docs/15` §4.1 satır 103–106 — T-MECH-CLIENT-01..04 ne ölçülecek.
- `docs/18` Q-01, Q-02, Q-18 — bu planın altyapısıyla cevaplanacak sorular.
- İlgili kod (dosya:satır, 2026-10-01'de doğrulandı):
  - `GameServer/User.cpp:217` `bool CUser::HandlePacket(Packet & pkt)`; `:219` `uint8 command = pkt.GetOpcode();`.
  - `GameServer/User.cpp:274` yorum `// Otherwise, assume we're authed & in-game.`, `:275` `switch (command)`. Giriş öncesi (login/karakter seçimi) dallar bu satırın **üstünde** `return true;` ile biter (`:272`). Kancayı **274'ün hemen üstüne**, yani yalnızca oyunda olan oyuncular için koyacaksın. Böylece login/şifre paketleri kapsam dışı kalır.
  - `GameServer/stdafx.h:5-7` `DEBUG` ise `DISABLE_PLAYER_BLINKING` tanımlıyor (örnek: bayrakların stdafx'te nasıl yazıldığı).
  - `GameServer/proj-GameServer.vcxproj:62` (Debug) ve `:100` (Release) `PreprocessorDefinitions` satırları (`ClCompile` ana `ItemDefinitionGroup` içinde); `:42` genel `PropertyGroup` (`_ProjectFileVersion`); `:188` `PartyHandler.cpp` (yeni `ClCompile` buraya komşu eklenebilir); `:276` `Npc.h` (yeni `ClInclude` buraya komşu).
  - `GameServer/GameServerDlg.cpp:161-191` mevcut log dosyaları `./Logs/...` altına `fopen(..., "a")` ile açılıyor (kalıp için bak).
  - `shared/packets.h`: `WIZ_MOVE 0x06`, `WIZ_ATTACK 0x08`, `WIZ_ROTATE 0x09`, `WIZ_CHAT 0x10`, `WIZ_TARGET_HP 0x22`, `WIZ_STATE_CHANGE 0x29`, `WIZ_PARTY 0x2F`, `WIZ_MAGIC_PROCESS 0x31`, `WIZ_SPEEDHACK_CHECK 0x41`. Bu dosya ISO-8859 olabilir: `grep -a` kullan; **düzenleme**.
  - `shared/Packet.h` / `shared/ByteBuffer.h`: `pkt.GetOpcode()`, `pkt.size()` (yük boyutu, opcode hariç), `pkt.contents()` (yük baytları; `size()==0` iken `contents()` çağırma), `pkt.rpos()`. Uygulayıcı önce bu başlıkları açıp imzaları doğrulasın.

## 3. Kapsam

**Yapılacaklar**

- Derleme bayrağı `FDP_PACKET_TRACE`: yalnızca MSBuild özelliği `FdpPacketTrace=1` verildiğinde tanımlanır. Varsayılan: kapalı (Debug ve Release'te).
- `GameServer/PacketTrace.h` ve `GameServer/PacketTrace.cpp`: izleyici sınıfı/işlevleri. Tüm içerik `#ifdef FDP_PACKET_TRACE` içinde (bayrak kapalıyken iki dosya boş çeviri birimi).
- `GameServer/User.cpp`'ye tek bir `#ifdef FDP_PACKET_TRACE … #endif` bloğu (kanca).
- `tools/build.sh`: isteğe bağlı ikinci argüman `--packet-trace` → MSBuild'e `/p:FdpPacketTrace=1`. Argümansız davranış **aynen** kalır.
- `tools/packet-trace-summary.py`: log dosyasını okuyup özet üretir; `--selftest` ile kendi kendini test eder (sunucu/istemci gerekmez).

**Kapsam dışı (yapılmayacak)**

- İstemciyle oyuna giriş, gerçek paket kaydı toplama, ölçüm sonuçlarını `docs/03`/`docs/18`'e işleme (F1-02, proje sahibi + Claude).
- Paket yükünü sunucu içinde çözmek/yorumlamak. Yalnızca ham hex.
- Giden (sunucudan istemciye) paketleri izlemek. Yalnızca **gelen** paketler.
- Bot kodu, `BotFairnessGuard`, yeni opcode, mekanik değişiklik.
- `shared/`, `AIServer/`, `LoginServer/`, SQL, `docs/**`, `AGENTS.md`, `opencode.json` değişikliği.
- Varsayılan derlemenin çıktısını değiştirecek her türlü "iyileştirme" (yeniden biçimlendirme, başka dosyalara dokunma).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `GameServer/PacketTrace.h` | yeni | UTF-8 **BOM'lu**, CRLF (komşu dosyalarla aynı) |
| `GameServer/PacketTrace.cpp` | yeni | UTF-8 BOM'lu, CRLF; ilk satır `#include "stdafx.h"` |
| `GameServer/User.cpp` | değiştir | yalnızca kanca bloğu + `#include "PacketTrace.h"` (ISO/UTF-8 BOM'u ve CRLF'i **koru**) |
| `GameServer/proj-GameServer.vcxproj` | değiştir | yeni `ClCompile`/`ClInclude` satırları + `FdpPacketTrace` özelliği (BOM+CRLF koru) |
| `GameServer/proj-GameServer.vcxproj.filters` | değiştir | yeni dosyalar için filtre girdileri (BOM+CRLF koru) |
| `tools/build.sh` | değiştir | `--packet-trace` seçeneği |
| `tools/packet-trace-summary.py` | yeni | `chmod +x` değil, `python3 tools/...` ile çalışır; ASCII |

`tools/*` düzenlemesi opencode'da `ask` ister; onay verilmezse durup Uygulayıcı Raporu'na yaz. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** raporda soru olarak yaz.

## 5. Uygulama adımları

1. **Başlık/imzaları doğrula.** `shared/Packet.h`, `shared/ByteBuffer.h` içinde `GetOpcode`, `size`, `contents`'ın gerçek imzalarını oku. `User.cpp:217-275` aralığını oku; kancanın gireceği yerin `// Otherwise, assume we're authed & in-game.` yorumunun hemen üstü olduğunu doğrula (`return true;` ile biten giriş öncesi dalların **altında**).

2. **Derleme bayrağı (vcxproj).** `proj-GameServer.vcxproj` `:42` civarındaki mevcut `<PropertyGroup>` içine (yeni bir `PropertyGroup` açmadan) şunu ekle:
   ```xml
   <FdpTraceDefs Condition="'$(FdpPacketTrace)'=='1'">FDP_PACKET_TRACE;</FdpTraceDefs>
   ```
   Debug (`:62`) ve Release (`:100`) `ClCompile` `PreprocessorDefinitions` satırlarının **başına** `$(FdpTraceDefs)` ekle (ör. `$(FdpTraceDefs)WIN32;GAMESERVER;…`). Özellik verilmezse `$(FdpTraceDefs)` boştur ve satır eski halinin aynısı olur. Aşağıdaki iki yardımcı satıra (`:55`, `:72`, `:92`, `:113` vb. farklı `ItemDefinitionGroup` alt öğeleri — örn. ResourceCompile/Midl) **dokunma**; yalnızca `:62` ve `:100` `ClCompile` bloklarında değişiklik yap. Hangi satırın `ClCompile` bloğunda olduğunu satırın bulunduğu `<ClCompile>` elemanına bakarak doğrula.

3. **`PacketTrace.h`.** Her şey `#ifdef FDP_PACKET_TRACE` içinde. İskelet (imzalar bağlayıcı, gövde senin):
   ```cpp
   #pragma once
   #ifdef FDP_PACKET_TRACE
   class Packet;
   namespace PacketTrace
   {
   	// Oyundaki bir oyuncudan gelen paketi izleme dosyasına yazar. HandlePacket (IOCP is parcacigi) icinden cagrilir.
   	void LogIncoming(uint16 sid, const char * charName, uint8 zone, const Packet & pkt);
   }
   #endif
   ```
   `uint16`/`uint8` türleri `stdafx.h` zincirinden gelir; başlığı `.cpp`'de `stdafx.h`'den **sonra** dahil et. Başlığın kendisi `User.cpp`'de `#include` edilirken `stdafx.h` zaten yüklü olduğundan sorun çıkmaz; yine de `Packet` için ileri bildirim yeterli olmazsa `#include "../shared/Packet.h"` ekleyebilirsin.

4. **`PacketTrace.cpp`.** `#include "stdafx.h"` ilk satır (PCH), ardından `#include "PacketTrace.h"`, geri kalan her şey `#ifdef FDP_PACKET_TRACE` içinde. İçerik:
   - **Opcode izin listesi** (bunun dışındaki hiçbir paket **yazılmaz**; özellikle `WIZ_LOGIN`, karakter seçimi, `WIZ_CHAT` içeriği, `WIZ_EXCHANGE`, `WIZ_ITEM_MOVE`, mağaza, mail asla loglanmaz): `WIZ_MOVE`, `WIZ_ROTATE`, `WIZ_ATTACK`, `WIZ_MAGIC_PROCESS`, `WIZ_TARGET_HP`, `WIZ_STATE_CHANGE`, `WIZ_PARTY`, `WIZ_SPEEDHACK_CHECK`. (`WIZ_PARTY` alt opcode ve id'leri içerir; kişisel metin içermez. Doğrula: `PartyHandler.cpp`'de metin/isim alanı varsa `WIZ_PARTY`'yi listeden **çıkar** ve raporda belirt.)
   - Zaman: `std::chrono::steady_clock` ile, ilk çağrıdan itibaren geçen **milisaniye** (`int64`); ayrıca satırın başına duvar saati `UNIXTIME` (saniye) yazılabilir. `UNIXTIME` 1 sn çözünürlüklüdür; ölçüm için **steady_clock ms** esastır.
   - Çıktı dosyası: `./Logs/PacketTrace_<gün>_<ay>_<yıl>.log`, `fopen("a")`, ilk çağrıda tembel (lazy) açılır (`GameServerDlg.cpp:161-191`'deki `CreateDirectory("Logs",NULL)` zaten klasörü oluşturur; yine de dosya açılamazsa sessizce izlemeyi kapat, **çökme**). Dosya yazma, tek bir `std::mutex` ile korunur (paketler tek IOCP iş parçacığından gelse de zararsız ve ucuz). Her satırdan sonra `fflush`.
   - Satır biçimi (sekme ayraçlı, tek satır, değişmez — betik buna güveniyor):
     ```
     t_ms<TAB>sid<TAB>name<TAB>zone<TAB>opcode_hex<TAB>len<TAB>payload_hex
     ```
     örn. `1523	12	TestChar	71	06	11	0f2c...` (`opcode_hex` iki hane küçük harf; `len` = `pkt.size()`; `payload_hex` küçük harf bitişik hex, `len==0` ise `-`). `name` içinde sekme/boşluk varsa `_` ile değiştir. En fazla ilk 64 yük baytını yaz (`len` gerçek boyutu söyler).
   - Mekanik veya oyun durumu **değiştirme**; paketin okuma imlecini (`rpos`) **bozma**: `contents()` ile baytları oku, `pkt` üzerinde `>>` kullanma.

5. **Kanca (`User.cpp`).** Dosyanın başındaki mevcut `#include` bloğuna `#include "PacketTrace.h"` ekle (blok içindeki mevcut sıraya uy). `// Otherwise, assume we're authed & in-game.` yorumunun **hemen üstüne**:
   ```cpp
   #ifdef FDP_PACKET_TRACE
   	PacketTrace::LogIncoming(GetSocketID(), GetName().c_str(), GetZoneID(), pkt);
   #endif
   ```
   `GetSocketID()` dönüş türü ve `GetName()` (`User.h:116`) kullanımlarını doğrula; yoksa en yakın eşdeğeri bul ve raporla. Başka hiçbir yerde `User.cpp`'ye dokunma.

6. **vcxproj + filters.** `PacketTrace.cpp` için `<ClCompile Include="PacketTrace.cpp" />` (`PartyHandler.cpp` satırı komşuluğunda), `PacketTrace.h` için `<ClInclude Include="PacketTrace.h" />` (`Npc.h` komşuluğunda). `.filters` dosyasında komşu dosyalarla aynı filtre (`Source Files` / `Header Files`; `Npc.h`/`Npc.cpp` satırlarının filtresine bak). Dosyaların kodlaması: BOM+CRLF **korunur**. Yeni dosyalara da BOM ve CRLF ver. (ISO-8859 dosyaları UTF-8'e çevirme.)

7. **`tools/build.sh`.** İkinci pozisyonel argüman `--packet-trace`:
   ```bash
   EXTRA=()
   [ "${2:-}" = "--packet-trace" ] && EXTRA+=("/p:FdpPacketTrace=1")
   ```
   MSBuild çağrısının sonuna `"${EXTRA[@]}"` ekle (`set -u` ile boş dizi sorun çıkarırsa `${EXTRA[@]+"${EXTRA[@]}"}` kalıbını kullan). Başlık yorumundaki `Usage:` satırını güncelle. Argümansız çağrının komutu **harfi harfine** eskisi gibi kalmalı.

8. **`tools/packet-trace-summary.py`.** Python 3, yalnızca standart kütüphane, ASCII. Kullanım: `python3 tools/packet-trace-summary.py <log> [--sid N] [--name X]` ve `--selftest`.
   - Satırları 4. adımdaki biçime göre ayrıştır; hatalı satırı atla ve sayısını yaz.
   - Çıktı (düz metin, Türkçe başlıklarsız ASCII kolon adları olabilir): opcode başına `adet, ort/medyan/min/maks ardışık aralık (ms)`; `WIZ_ATTACK (08)` için ardışık paket aralıkları ayrıca; `WIZ_MAGIC_PROCESS (31)` için ilk yük baytı (alt opcode) başına adet; `WIZ_MOVE (06)` için saniyedeki paket sayısı; `WIZ_TARGET_HP (22)` için aralıklar.
   - `--selftest`: bellekte sentetik birkaç satır üretip beklenen sayıları `assert` ile doğrular, `selftest OK` basar, çıkış kodu 0. Sunucu/log dosyası gerekmez.

9. **Derle ve doğrula** (aşağıdaki kriterler). Önce bayraksız `Release`, sonra `--packet-trace` ile `Release`. İkincisinde üretilen `GameServer.exe`'yi **çalıştırma** (istemci ve sunucu gerektirir; çalışma zamanı doğrulaması F1-02'dedir). Derleme çıktısını (`build/bin/…`) git'e ekleme.

10. Raporu `plans/F1-01-paket-izleyici.md` "Uygulayıcı Raporu"na yaz; her adımın çıktısını yapıştır.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` hatasız biter, **yeni uyarı yok** (derleme çıktısının son 10 satırını rapora yapıştır).
- [ ] K2: `./tools/build.sh Release --packet-trace` hatasız biter, yeni uyarı yok. Bayrağın gerçekten devrede olduğu kanıtlanır: derleme günlüğünde veya `grep -c FDP_PACKET_TRACE` ile `GameServer/PacketTrace.cpp` içindeki kod bloğunun derlendiğini gösteren bir kanıt (ör. `PacketTrace.cpp` için `cl` komut satırında `/D FDP_PACKET_TRACE`; bulamazsan `/v:detailed` ile bir kez derle ve ilgili satırı yapıştır).
- [ ] K3: `./tools/build.sh Debug` hatasız biter (bayraksız).
- [ ] K4: Bayraksız derlemede davranış değişmedi: `git diff main...bot/F1-01 -- GameServer/` yalnızca `PacketTrace.*`, `User.cpp` (yalnızca `#include "PacketTrace.h"` + `#ifdef FDP_PACKET_TRACE` bloğu), vcxproj ve filters dosyalarını içerir; `User.cpp` farkı toplam ≤ 8 satır ekleme, 0 silme.
- [ ] K5: Kanca yalnızca oyundaki oyuncular içindir: `git diff` içinde kancanın `// Otherwise, assume we're authed & in-game.` yorumunun hemen üstünde ve `return true;` ile biten giriş öncesi dalların altında olduğu görülür; `WIZ_LOGIN`/karakter seçimi paketleri izleyiciye ulaşmaz.
- [ ] K6: İzin listesi dışında hiçbir opcode yazılmaz: `PacketTrace.cpp`'de izin listesi tek bir yerde tanımlı ve `WIZ_CHAT`, `WIZ_LOGIN`, `WIZ_EXCHANGE`, `WIZ_ITEM_MOVE` o listede **yok** (`grep -n` çıktısını yapıştır).
- [ ] K7: `python3 tools/packet-trace-summary.py --selftest` `selftest OK` basar, çıkış kodu 0.
- [ ] K8: Betik sentetik bir log ile çalışır: rapora 6–10 satırlık elle yazılmış örnek log (dosya olarak değil, `printf` ile `/tmp` veya `$TMPDIR` altında üretilip silinen geçici dosya) ve betiğin çıktısı yapıştırılır. Geçici dosya depoya eklenmez.
- [ ] K9: Kodlama: `file GameServer/PacketTrace.h GameServer/PacketTrace.cpp GameServer/User.cpp` hepsi "UTF-8 (with BOM) … CRLF" (User.cpp'nin önceki kodlamasıyla aynı); `file tools/build.sh tools/packet-trace-summary.py` CRLF içermez. Çıktıyı yapıştır.
- [ ] K10: `git status --short` boş (yalnızca izinli dosyalar commit'li; `build/` çıktısı ve `Logs/` depoda değil).
- [ ] K11: `./tools/build.sh Release` argümansız çağrısının MSBuild komutu eskisiyle aynı (yalnızca `EXTRA` boş): `git diff main...bot/F1-01 -- tools/build.sh` farkını yapıştır ve boş-dizi durumunun `set -u` altında hata vermediğini bir kez `bash -c 'set -u; EXTRA=(); echo ${EXTRA[@]+"${EXTRA[@]}"}'` ile göster.

## 7. Doğrulama komutları

```bash
./tools/build.sh Release
./tools/build.sh Release --packet-trace
./tools/build.sh Debug
python3 tools/packet-trace-summary.py --selftest
git diff --stat main...bot/F1-01
git diff main...bot/F1-01 -- GameServer/User.cpp
file GameServer/PacketTrace.h GameServer/PacketTrace.cpp GameServer/User.cpp tools/build.sh tools/packet-trace-summary.py
grep -n "WIZ_" GameServer/PacketTrace.cpp
git status --short
```

Not: `--packet-trace` derlemesinden sonra bayraksız bir derleme yapıldığında MSBuild komut satırı değişimini algılayıp ilgili dosyaları yeniden derler; bayraklı `GameServer.exe`'nin **dağıtım dizinine** (`C:\dev\fdp\server`) kopyalanması bu planın işi **değil**.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §Kod kuralları. Yeni `.cpp/.h`: UTF-8 BOM + CRLF. vcxproj/filters: BOM + CRLF korunur.
- **Gizlilik:** İzleyici yalnızca izin listesindeki opcode'ları yazar; sohbet, login, takas, depo, mail içeriği **asla**. Karakter adı yalnızca yerel geliştirme logunda geçer; `Logs/` git'e girmez. Log örneğini rapora koyarken **gerçek oyuncu adlarını maskele** (bu plan zaten gerçek log üretmez).
- Thread: `HandlePacket` IOCP iş parçacığında çalışır; izleyici bloklayıcı iş yapmamalı (dosya yazma + `fflush` kabul; ağ/DB/uyku yok).
- Hiçbir paketi **tüketme veya değiştirme**: kanca salt-okur; `pkt`'in okuma imleci bozulmamalı (`MEC` kuralı: bayrak açıkken bile oyun davranışı aynı kalır).
- Bu plan CLI-01..12 adalet tablosunu **değiştirmez**; yalnızca ölçüm altyapısıdır. Ölçülmüş değerler F1-02'den sonra `docs/03` §13'e işlenecek.
- Riskler: (1) `FdpTraceDefs` özelliğinin yanlış satıra eklenip varsayılan derlemeyi değiştirmesi — K1/K4 bunu yakalar; (2) PCH ile bayrak uyumsuzluğu — bayrak değişince tüm proje yeniden derlenir, ilk bayraklı derleme normalden uzun sürebilir, bu beklenen; (3) `std::chrono` başlıkları PCH zincirinde yoksa `PacketTrace.cpp`'de `<chrono>` dahil et.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F1-01` (taban: `bot/F0-02` @ `dc7cac3`)
  - `baf8008` — `[F1-01] Paket izleyici ve kanca eklendi` (`PacketTrace.h`, `PacketTrace.cpp`, `User.cpp`, `proj-GameServer.vcxproj`, `proj-GameServer.vcxproj.filters`)
  - `a5343c4` — `[F1-01] build.sh --packet-trace secenegi` (`tools/build.sh`)
  - `bda4f7b` — `[F1-01] Ozet betigi ve selftest` (`tools/packet-trace-summary.py`)
  - Bu rapor ve `Durum: UYGULANDI` ayrı commit'lenir.
- Değişen dosyalar ve nedenleri:
  - `GameServer/PacketTrace.h` (yeni): `PacketTrace::LogIncoming` bildirimi; tüm içerik `#ifdef FDP_PACKET_TRACE` içinde.
  - `GameServer/PacketTrace.cpp` (yeni): izleyici; izin listesi (7 opcode), `steady_clock` ms, `./Logs/PacketTrace_<gün>_<ay>_<yıl>.log` (tembel `fopen("a")`, `fflush`, `std::mutex`), isim temizleme, ilk 64 bayt hex, `len==0` iken `-`.
  - `GameServer/User.cpp` (değişti): yalnızca `#include "PacketTrace.h"` + `#ifdef FDP_PACKET_TRACE` kancası (5 ekleme, 0 silme).
  - `GameServer/proj-GameServer.vcxproj` (değişti): `FdpTraceDefs` özelliği; Debug/Release `ClCompile` tanım satırlarının başına `$(FdpTraceDefs)`; `PacketTrace.cpp/h` proje girdileri.
  - `GameServer/proj-GameServer.vcxproj.filters` (değişti): yeni dosyalar için `Source Files` / `Header Files` filtreleri.
  - `tools/build.sh` (değişti): ikinci argüman `--packet-trace` → `/p:FdpPacketTrace=1`.
  - `tools/packet-trace-summary.py` (yeni): log özeti + `--selftest`.
  - `plans/F1-01-paket-izleyici.md`: yalnızca `Durum` satırı ve bu rapor.
- Bayrak kapalıyken sunucu davranışı değişmez: `FDP_PACKET_TRACE` hiçbir yerde tanımlı değildir; izleyici kodunun tamamı `#ifdef` içindedir (K4: User.cpp 5 ekleme/0 silme; `PacketTrace.cpp` bayraksız derlemede boş çeviri birimi olarak derlenir).

**Adım 1 — imza ve kanca yeri doğrulaması (koddan okundu)**
```
pkt.GetOpcode()  -> uint8              shared/Packet.h (GetOpcode)
pkt.size()       -> size_t             shared/ByteBuffer.h:128
pkt.contents()   -> const uint8 *      shared/ByteBuffer.h:127
pkt.rpos()       -> size_t             shared/ByteBuffer.h:98
GetSocketID()    -> uint16             shared/KOSocket.h:24
GetName()        -> std::string &      GameServer/User.h:116
GetZoneID()      -> uint8              GameServer/Unit.h:55
```
Kanca, `User.cpp` içinde `return true;` ile biten üç giriş öncesi dalın (crypto/hesap/karakter seçimi) **altında**, `// Otherwise, assume we're authed & in-game.` yorumunun hemen üstündedir; login/karakter paketleri izleyiciye ulaşmaz (K5, fark aşağıda).

**Adım 2/6 — vcxproj + filters farkı (tam)**
```
$ git diff main...bot/F1-01 -- GameServer/proj-GameServer.vcxproj
diff --git a/GameServer/proj-GameServer.vcxproj b/GameServer/proj-GameServer.vcxproj
index a7cdd7a..cd2d7a5 100644
--- a/GameServer/proj-GameServer.vcxproj
+++ b/GameServer/proj-GameServer.vcxproj
@@ -41,6 +41,7 @@
   <PropertyGroup Label="UserMacros" />
   <PropertyGroup>
     <_ProjectFileVersion>10.0.40219.1</_ProjectFileVersion>
+    <FdpTraceDefs Condition="'$(FdpPacketTrace)'=='1'">FDP_PACKET_TRACE;</FdpTraceDefs>
     <LinkIncremental Condition="'$(Configuration)|$(Platform)'=='Debug|Win32'">true</LinkIncremental>
     <OutDir Condition="'$(Configuration)|$(Platform)'=='Release|Win32'">$(SolutionDir)build\bin\$(PlatformTarget)-$(Configuration)\Server\</OutDir>
     <IntDir Condition="'$(Configuration)|$(Platform)'=='Release|Win32'">$(SolutionDir)build\obj\$(PlatformTarget)-$(Configuration)\Server\$(ProjectName)\</IntDir>
@@ -59,7 +60,7 @@
     </Midl>
     <ClCompile>
       <Optimization>Disabled</Optimization>
-      <PreprocessorDefinitions>WIN32;GAMESERVER;_WINSOCK_DEPRECATED_NO_WARNINGS;_DEBUG;_WINDOWS;_3DSERVER;_CRT_SECURE_NO_WARNINGS;%(PreprocessorDefinitions)</PreprocessorDefinitions>
+      <PreprocessorDefinitions>$(FdpTraceDefs)WIN32;GAMESERVER;_WINSOCK_DEPRECATED_NO_WARNINGS;_DEBUG;_WINDOWS;_3DSERVER;_CRT_SECURE_NO_WARNINGS;%(PreprocessorDefinitions)</PreprocessorDefinitions>
       <RuntimeLibrary>MultiThreadedDebug</RuntimeLibrary>
       <WarningLevel>Level3</WarningLevel>
       <SuppressStartupBanner>true</SuppressStartupBanner>
@@ -97,7 +98,7 @@
     <ClCompile>
       <Optimization>MaxSpeed</Optimization>
       <InlineFunctionExpansion>AnySuitable</InlineFunctionExpansion>
-      <PreprocessorDefinitions>WIN32;GAMESERVER;_WINSOCK_DEPRECATED_NO_WARNINGS;NDEBUG;_WINDOWS;_3DSERVER;_CRT_SECURE_NO_WARNINGS;%(PreprocessorDefinitions)</PreprocessorDefinitions>
+      <PreprocessorDefinitions>$(FdpTraceDefs)WIN32;GAMESERVER;_WINSOCK_DEPRECATED_NO_WARNINGS;NDEBUG;_WINDOWS;_3DSERVER;_CRT_SECURE_NO_WARNINGS;%(PreprocessorDefinitions)</PreprocessorDefinitions>
       <StringPooling>true</StringPooling>
       <RuntimeLibrary>MultiThreaded</RuntimeLibrary>
       <FunctionLevelLinking>true</FunctionLevelLinking>
@@ -186,6 +187,7 @@
     <ClCompile Include="NPCHandler.cpp" />
     <ClCompile Include="CharacterHandler.cpp" />
     <ClCompile Include="PartyHandler.cpp" />
+    <ClCompile Include="PacketTrace.cpp" />
     <ClCompile Include="QuestHandler.cpp" />
     <ClCompile Include="Region.cpp" />
     <ClCompile Include="RentalHandler.cpp" />
@@ -274,6 +276,7 @@
     <ClInclude Include="MagicProcess.h" />
     <ClInclude Include="Map.h" />
     <ClInclude Include="Npc.h" />
+    <ClInclude Include="PacketTrace.h" />
     <ClInclude Include="Region.h" />
     <ClInclude Include="StdAfx.h" />
     <ClInclude Include="Unit.h" />

$ git diff main...bot/F1-01 -- GameServer/proj-GameServer.vcxproj.filters
diff --git a/GameServer/proj-GameServer.vcxproj.filters b/GameServer/proj-GameServer.vcxproj.filters
index facc210..f32585f 100644
--- a/GameServer/proj-GameServer.vcxproj.filters
+++ b/GameServer/proj-GameServer.vcxproj.filters
@@ -75,6 +75,9 @@
     <ClCompile Include="PartyHandler.cpp">
       <Filter>Source Files\Handlers</Filter>
     </ClCompile>
+    <ClCompile Include="PacketTrace.cpp">
+      <Filter>Source Files</Filter>
+    </ClCompile>
     <ClCompile Include="FriendHandler.cpp">
       <Filter>Source Files\Handlers</Filter>
     </ClCompile>
@@ -182,6 +185,9 @@
     <ClInclude Include="Npc.h">
       <Filter>Header Files</Filter>
     </ClInclude>
+    <ClInclude Include="PacketTrace.h">
+      <Filter>Header Files</Filter>
+    </ClInclude>
     <ClInclude Include="Region.h">
       <Filter>Header Files</Filter>
     </ClInclude>
```

**Adım 7 — `tools/build.sh` farkı ve boş-dizi testi (K11)**
```
$ git diff main...bot/F1-01 -- tools/build.sh
diff --git a/tools/build.sh b/tools/build.sh
index afe4299..af3809a 100755
--- a/tools/build.sh
+++ b/tools/build.sh
@@ -1,9 +1,13 @@
 #!/usr/bin/env bash
 # Builds KnightOnlineServer.sln (Win32, MSVC v143) from WSL.
-# Usage: tools/build.sh [Release|Debug]
+# Usage: tools/build.sh [Release|Debug] [--packet-trace]
 set -euo pipefail
 
 CONFIG="${1:-Release}"
+EXTRA=()
+if [ "${2:-}" = "--packet-trace" ]; then
+	EXTRA+=("/p:FdpPacketTrace=1")
+fi
 ROOT="$(cd "$(dirname "$0")/.." && pwd)"
 MSBUILD="${MSBUILD:-/mnt/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe}"
 
@@ -13,4 +17,4 @@ if [ ! -f "$MSBUILD" ]; then
 fi
 
 SLN="$(wslpath -w "$ROOT/KnightOnlineServer.sln")"
-"$MSBUILD" "$SLN" /p:Configuration="$CONFIG" /p:Platform=Win32 /p:PlatformToolset=v143 /m /nologo /v:minimal
+"$MSBUILD" "$SLN" /p:Configuration="$CONFIG" /p:Platform=Win32 /p:PlatformToolset=v143 /m /nologo /v:minimal "${EXTRA[@]+"${EXTRA[@]}"}"

$ bash -c 'set -u; EXTRA=(); echo ${EXTRA[@]+"${EXTRA[@]}"}'
bos_dizi_exit=0
```
Argümansız çağrıda `EXTRA` boş olduğundan ek argüman eklenmez.

**Adım 8 — özet betiği (K7, K8)**
```
$ python3 tools/packet-trace-summary.py --selftest
selftest OK
selftest_exit=0

$ printf '0\t12\tTestChar\t71\t06\t11\t0f2c00000000000000000000000000\n100\t12\tTestChar\t71\t06\t11\t0f2c00010000000000000000000000\n250\t12\tTestChar\t71\t08\t15\t0000000a0000\n400\t12\tTestChar\t71\t08\t15\t0000000a0000\n450\t12\tTestChar\t71\t31\t10\t020001000000000000\n550\t12\tTestChar\t71\t31\t10\t030001000000000000\n600\t12\tTestChar\t71\t22\t02\t0102\n700\t12\tTestChar\t71\t22\t02\t0103\n' > /tmp/opencode/f1-01-sample.log
$ python3 tools/packet-trace-summary.py /tmp/opencode/f1-01-sample.log
parsed_lines: 8
skipped_lines: 0
filtered_out_records: 0
total_records: 8
opcode summary:
  WIZ_MOVE (06): count=2 interval_ms avg=100.0 median=100.0 min=100 max=100
  WIZ_ATTACK (08): count=2 interval_ms avg=150.0 median=150.0 min=150 max=150
  WIZ_TARGET_HP (22): count=2 interval_ms avg=100.0 median=100.0 min=100 max=100
  WIZ_MAGIC_PROCESS (31): count=2 interval_ms avg=100.0 median=100.0 min=100 max=100
WIZ_ATTACK (08) intervals_ms: [150]
WIZ_TARGET_HP (22) intervals_ms: [100]
WIZ_MAGIC_PROCESS (31) sub_opcode counts:
  02: 1
  03: 1
WIZ_MOVE (06) packets_per_second: min=2 avg=2.0 max=2 seconds=1
sample_exit=0
$ rm -f /tmp/opencode/f1-01-sample.log
```
Geçici log depoya eklenmedi.

**Adım 9 — derlemeler ve bayrak kanıtı (K1, K2, K3)**

K1 — `./tools/build.sh Release` (bayraksız), çıkış kodu `0`, son 10 satır:
```
C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\GameServer\User.cpp(2734,34): warning C4834: [[nodiscard]] özniteliğine sahip işlevin dönüş değerinin atılması [C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\GameServer\proj-GameServer.vcxproj]
  ('/User.cpp' kaynak dosyası derleniyor)
  Kod üretiliyor
  1 of 13016 functions (<0.1%) were compiled, the rest were copied from previous compilation.
    0 functions were new in current compilation
    61 functions had inline decision re-evaluated but remain unchanged
  Kodun üretilmesi tamamlandı
  proj-GameServer.vcxproj -> C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\bin\x86-Release\Server\GameServer.exe
```
→ Buradaki `C4834` uyarıları **önceden vardı**; kanıt için değişiklikler geçici olarak `git stash` ile geri alınıp aynı derleme yapıldı:
```
$ git stash push -- GameServer/User.cpp GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters tools/build.sh
$ ./tools/build.sh Release
baseline_exit=0
      2 warning C4834
C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\GameServer\User.cpp(2714,35): warning C4834: [[nodiscard]] özniteliğine sahip işlevin dönüş değerinin atılması [C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\GameServer\proj-GameServer.vcxproj]
C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\GameServer\User.cpp(2729,34): warning C4834: [[nodiscard]] özniteliğine sahip işlevin dönüş değerinin atılması [C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\GameServer\proj-GameServer.vcxproj]
$ git stash pop
```
→ Taban sürümde de aynı iki uyarı (benim eklediğim satırlar yüzünden 2719/2734'e kaydı). `PacketTrace.cpp` sıfır uyarı. **Yeni uyarı yok.**

K2 — `./tools/build.sh Release --packet-trace`, çıkış kodu `0`, son 10 satır:
```
C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\GameServer\User.cpp(2734,34): warning C4834: [[nodiscard]] özniteliğine sahip işlevin dönüş değerinin atılması [C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\GameServer\proj-GameServer.vcxproj]
  ('/User.cpp' kaynak dosyası derleniyor)
  Kod üretiliyor
  25 of 13035 functions ( 0.2%) were compiled, the rest were copied from previous compilation.
    9 functions were new in current compilation
    116 functions had inline decision re-evaluated but remain unchanged
  Kodun üretilmesi tamamlandı
  proj-GameServer.vcxproj -> C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\bin\x86-Release\Server\GameServer.exe
```
Bayrak değişince GameServer projesi tümüyle yeniden derlendiği için dosyalardaki **mevcut** uyarılar görünür; dosya bazında dağılım:
```
$ grep "warning C" /tmp/opencode/f1-release-ptrace.log | sed -E 's/.*[\\/]([A-Za-z0-9_]+\.(cpp|c|h))\(.*warning (C[0-9]+).*/\1 \3/' | sort | uniq -c
      3 AISocket.cpp C4834
      2 User.cpp C4834
      2 GameServerDlg.cpp C4267
      1 Map.cpp C4834
      1 MagicProcess.cpp C4834
      1 MagicInstance.cpp C4838
      1 MagicInstance.cpp C4834
      1 LoginHandler.cpp C4267
      1 GameServerDlg.cpp C4834
      1 EventHandler.cpp C4267
      1 DBAgent.cpp C4834
      1 DBAgent.cpp C4267
```
→ Hiçbiri `PacketTrace.cpp`'de değil; tümü değiştirmediğim dosyalarda. Bayrağın gerçekten devrede olduğunun kanıtı — `cl` komut satırı (`/v:detailed` ile bir kez derlendi):
```
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\HostX86\x86\CL.exe /c /I"C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\../src/scripting/Lua/src" /Zi /nologo /W3 /WX- /diagnostics:column /O2 /Ob2 /Ot /Oy- /GL /D FDP_PACKET_TRACE /D WIN32 /D GAMESERVER /D _WINSOCK_DEPRECATED_NO_WARNINGS /D NDEBUG /D _WINDOWS /D _3DSERVER /D _CRT_SECURE_NO_WARNINGS /D _VC80_UPGRADE=0x0600 /D _MBCS /GF /Gm- /EHsc /MT /GS /Gy /fp:precise /Zc:wchar_t /Zc:forScope /Zc:inline /std:c++17 /Yu"stdafx.h" /Fp"C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\obj\x86-Release\Server\GameServer\GameServer.pch" /Fo"C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\obj\x86-Release\Server\GameServer\\" /Fd"C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\obj\x86-Release\Server\GameServer\vc143.pdb" /external:W3 /Gd /TP /analyze- /FC /errorReport:queue PacketTrace.cpp User.cpp
```
ve bayraklı `GameServer.exe` içinde log biçim dizgesi; bayraksız exe'de yok:
```
$ grep -a -c "PacketTrace_" build/bin/x86-Release/Server/GameServer.exe   # bayrakli derleme sonrasi
1
$ grep -a -o "./Logs/PacketTrace_[^\"]*" build/bin/x86-Release/Server/GameServer.exe | head -2
./Logs/PacketTrace_%d_%d_%d.log - %lld	%u	%s	%u	%02x	%zu	%s
```
(Önceki bayraksız derlemede aynı grep `0` döndü.)

K3 — `./tools/build.sh Debug` (bayraksız), çıkış kodu `0`, uyarı yok, son 10 satır:
```
  Lua.vcxproj -> C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\bin\x86-Debug\libs\Lua.lib
  shared.vcxproj -> C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\bin\x86-Debug\libs\shared.lib
  proj-LogInServer.vcxproj -> C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\bin\x86-Debug\Server\LogInServer.exe
  PacketTrace.cpp
  User.cpp
  proj-AIServer.vcxproj -> C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\bin\x86-Debug\Server\AIServer.exe
  Kod Üretiliyor...
  proj-GameServer.vcxproj -> C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\bin\x86-Debug\Server\GameServer.exe
```

**K4 / K5 — `GameServer/` farkı**
```
$ git diff --numstat main...bot/F1-01 -- GameServer/
126	0	GameServer/PacketTrace.cpp
14	0	GameServer/PacketTrace.h
5	0	GameServer/User.cpp
5	2	GameServer/proj-GameServer.vcxproj
6	0	GameServer/proj-GameServer.vcxproj.filters

$ git diff main...bot/F1-01 -- GameServer/User.cpp
@@ -3,6 +3,7 @@
 #include "MagicInstance.h"
+#include "PacketTrace.h"
 #include "DBAgent.h"
@@ -271,6 +272,10 @@ bool CUser::HandlePacket(Packet & pkt)
 		return true;
 	}
 
+#ifdef FDP_PACKET_TRACE
+	PacketTrace::LogIncoming(GetSocketID(), GetName().c_str(), GetZoneID(), pkt);
+#endif
+
 	// Otherwise, assume we're authed & in-game.
 	switch (command)
```
`User.cpp`: 5 ekleme, 0 silme (K4 üst sınırı 8).

**K6 — izin listesi**
```
$ grep -n "WIZ_" GameServer/PacketTrace.cpp
26:			case WIZ_MOVE:
27:			case WIZ_ROTATE:
28:			case WIZ_ATTACK:
29:			case WIZ_MAGIC_PROCESS:
30:			case WIZ_TARGET_HP:
31:			case WIZ_STATE_CHANGE:
32:			case WIZ_SPEEDHACK_CHECK:
$ grep -n "WIZ_CHAT\|WIZ_LOGIN\|WIZ_EXCHANGE\|WIZ_ITEM_MOVE\|WIZ_PARTY" GameServer/PacketTrace.cpp
grep_exit=1
```
→ `WIZ_PARTY` de listede değil; gerekçe "Plandan sapmalar" 1. maddede.

**K9 — kodlama**
```
$ file GameServer/PacketTrace.h GameServer/PacketTrace.cpp GameServer/User.cpp tools/build.sh tools/packet-trace-summary.py
GameServer/PacketTrace.h:      C++ source, Unicode text, UTF-8 (with BOM) text, with CRLF line terminators
GameServer/PacketTrace.cpp:    C source, Unicode text, UTF-8 (with BOM) text, with CRLF line terminators
GameServer/User.cpp:           C++ source, Unicode text, UTF-8 (with BOM) text, with CRLF line terminators
tools/build.sh:                Bourne-Again shell script, ASCII text executable
tools/packet-trace-summary.py: Python script, ASCII text executable

$ grep -c $'\r' tools/build.sh tools/packet-trace-summary.py
tools/build.sh:0
tools/packet-trace-summary.py:0
```
(`file`, `PacketTrace.cpp`'yi içerik sezgisiyle "C source" sayıyor; kodlama satırı UTF-8 BOM + CRLF.)

**K10 — çalışma ağacı (rapor commit'i öncesi)**
```
$ git status --short
 M plans/F1-01-paket-izleyici.md
?? start.md
```
`start.md` depoda önceden beri izlenmeyen altyapı dosyasıdır (plana göre dokunulmadı); rapor commit'inden sonra yalnızca o kalır. `build/` ve `Logs/` depoda değil.

**Kabul kriterleri öz-değerlendirmesi**
- K1 ✔ `./tools/build.sh Release` çıkış 0; yeni uyarı yok (taban stash testiyle kanıtlandı; `PacketTrace.cpp` 0 uyarı).
- K2 ✔ `./tools/build.sh Release --packet-trace` çıkış 0; `cl` komut satırında `/D FDP_PACKET_TRACE` ve `PacketTrace.cpp`; bayraklı exe'de `PacketTrace_` dizgesi var, bayraksızda yok.
- K3 ✔ `./tools/build.sh Debug` çıkış 0, uyarı yok.
- K4 ✔ `GameServer/` farkı yalnızca izinli 5 dosya; `User.cpp` 5 ekleme/0 silme.
- K5 ✔ Kanca giriş öncesi dalların altında, yorumun hemen üstünde; login/karakter seçimi paketleri kapsam dışı.
- K6 ✔ İzin listesi tek yerde (`IsTracedOpcode`); `WIZ_CHAT`, `WIZ_LOGIN`, `WIZ_EXCHANGE`, `WIZ_ITEM_MOVE` yok; `WIZ_PARTY` de bilinçli olarak yok.
- K7 ✔ `--selftest` → `selftest OK`, çıkış 0.
- K8 ✔ Sentetik 8 satırlık log ile özet üretildi; geçici dosya `/tmp/opencode` altındaydı ve silindi.
- K9 ✔ Yukarıdaki `file` çıktısı; `.sh`/`.py` CR içermiyor.
- K10 ✔ İzinli dosyalar dışında değişiklik yok; tek kalan `?? start.md` (önceden var, dokunulmadı).
- K11 ✔ `build.sh` farkı yalnızca `EXTRA` ekliyor; boş dizi testi `set -u` altında hatasız.

**Plandan sapmalar ve gerekçeleri**
1. **`WIZ_PARTY` izin listesinden çıkarıldı.** Plan adım 4 bunu şarta bağlamıştı: `GameServer/PartyHandler.cpp:14-16` gelen `WIZ_PARTY` (`PARTY_CREATE`/`PARTY_INSERT`) paketinden karakter adı okuyor (`pkt >> strUserID;`). Kişisel metin loglanmaması için 7 opcode kaldı: `WIZ_MOVE`, `WIZ_ROTATE`, `WIZ_ATTACK`, `WIZ_MAGIC_PROCESS`, `WIZ_TARGET_HP`, `WIZ_STATE_CHANGE`, `WIZ_SPEEDHACK_CHECK`.
2. **Ek standart başlıklar:** `PacketTrace.cpp`'ye `<chrono>`, `<cstdio>`, `<ctime>`, `<mutex>` eklendi (plan risk 3'ün önerisi).
3. **Bayrak kanıtı için ek doğrulama:** Plan K2 "derleme günlüğünde `cl` satırı; bulamazsan `/v:detailed`" diyordu; `/v:detailed` ile bir kez derlenip `cl` satırı ve üretilen exe'deki dizge birlikte gösterildi. Ek derleme varsayılan `build.sh` davranışını değiştirmedi.
4. **`tools/packet-trace-summary.py` LF:** Çalışma ağacında LF ve commit'lenen blob LF (`git show HEAD:... | grep -c $'\r'` → 0). Ancak `.gitattributes` yalnızca `*.sh`'yi LF'e sabitliyor; taze bir Windows checkout'unda `.py` CRLF'e dönüşebilir. `.gitattributes` opencode izin listesinde olmadığı için dokunulmadı; ayrıntı "Açık sorular"da.
5. **Özet betiği çıktısı ASCII/İngilizce** kolon adlarıyla yazıldı (plan ASCII ve Türkçe başlıksız çıktıya izin veriyor).

**Açık sorular / bulgular**
1. **`.py` satır sonu kalıcılığı:** `tools/packet-trace-summary.py` depoda LF olarak saklanıyor (blob LF), fakat `.gitattributes` `*.py text eol=lf` içermediğinden `core.autocrlf=true` olan bir Windows/WSL checkout'unda CRLF'e dönüşebilir. Kalıcı çözüm `.gitattributes`'a bir satır eklemektir; bu dosya izin listesinde olmadığı için değiştirilmedi (planner/doğrulayıcı kararı).
2. **F1-02 için ölçüm notu:** `WIZ_PARTY` paketleri parti zamanlaması için artık loglanmayacak; gerekiyorsa yalnızca `t/opcode/len` (isim alanı olmadan) yazan ayrı bir güvenli yol ayrı planda düşünülmeli.
3. **AIServer'da `PacketTrace.cpp` yok:** İzleyici yalnızca GameServer'a eklendi (plan kapsamı); AIServer/LogInServer'da değişiklik yok.
4. **Bayraklı derleme zamanlaması:** `--packet-trace` ilk kez verildiğinde GameServer projesi tümüyle yeniden derlenir (tanım değişikliği); bu beklenen davranıştır (plan risk 2).

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(henüz yok)
