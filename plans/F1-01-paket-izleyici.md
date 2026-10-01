# F1-01: Paket izleyici (derleme bayrağıyla kapalı) ve özet betiği

| Alan | Değer |
|---|---|
| Durum | HAZIR |
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

- Durum: —
- Branch / commit'ler: —
- Değişen dosyalar ve neden: —
- Derleme sonucu: —
- Kabul kriterleri öz-değerlendirme: —
- Plandan sapmalar ve gerekçeleri: —
- Açık sorular: —

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(henüz yok)
