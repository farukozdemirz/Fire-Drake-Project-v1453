# F4-54: Gözlem tablosunda tek yönlü görüş (KI-016): sayaçlı teşhis ve kök neden düzeltmesi

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2; kapı G4) |
| Branch | `bot/F4-54 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | F4-12, F4-13, F4-14, F4-15 (görünür oyuncu/NPC tabloları ve bölge değişimi istekleri) — `KAPANDI` |
| İlgili gereksinim / kabul | KI-016, Q-27; `docs/03` §16 (3×3 bölge bilgisi); `docs/reports/degerlendirme-2026-10-02.md` DEG-04 |
| Tahmini büyüklük | S–M (teşhis sayaçları + koşullu düzeltme; 4 kod dosyası) |
| Hazırlayan / tarih | Claude / 2026-10-02 (değerlendirme) |

---

## 1. Amaç

F4-18 Tur 2 doğrulamasında (plan dosyası, "Bulgular" madde 3) görüş tablosu (`ObsTable`) bazı bot çiftlerinde **tek yönlü** çıktı: `BotMF_K` aynı bölgede 5 m'deki `BotWP_K`'yi görmüyor, `BotWP_K` `BotMF_K`'yi görüyordu (`member … out_of_view` bu yüzdendi). Neden teşhis edilmedi ve kayıt altına alınmadı. Karar katmanı ve takım görünümü (hedef seçimi, `TeamView.inView`, peel) bu tabloya dayanır; sessiz bir görünmezlik hatası F6'da "bot neden saldırmıyor" sorusuna dönüşür. Bu plan (1) hata ayıklama **sayaçları** ekler, (2) tekrar üretir, (3) kök nedeni bulur ve **yalnızca** `BotCore/Perception.h` / `BotSession` içindeyse düzeltir; sunucu kodunda ise düzeltmez, `KNOWN_ISSUES` kaydına yazar (üretim sunucusu davranışı bu planın kapsamı dışındadır).

## 2. Bağlam (okunması zorunlu)

- `plans/F4-18-algi-takim-gorunumu.md` Doğrulama Tur 2, Bulgular madde 3 (gözlem) ve S1–S5 çalışma zamanı kaydı.
- `GameServer/Bot/BotSession.cpp:195-262` (`WIZ_USER_INOUT`/`WIZ_REQ_USERIN`/`WIZ_REGIONCHANGE`/`WIZ_MOVE`/`WIZ_DEAD` blokları): **ayrıştırma başarısız olursa paket sessizce atılır, sayaç yok**; `WIZ_REGIONCHANGE` dalı `m_obs.Retain(ids, n, 0xFFFF)` ile listede olmayan her birimi **siler**.
- `BotCore/Perception.h` `ParseUserInOut`, `ParseUserInfo` (kayıt alanları: ad `u8` uzunluklu, klan bloğu, ekipman, ...), `ObsTable::Retain` (~satır 330).
- `GameServer/Bot/ActionExecutor.cpp` `TickUserIn` (F4-13, CLI-19: ≥ 1,0 sn aralık, istek başına ≤ 32 kimlik, `m_selfSid` hariç).
- Sunucu: `GameServer/GameServerDlg.cpp:1340-1358` `RegionUserInOutForMe`, `GameServer/User.cpp:1230-1253` `RequestUserIn`, bölge yayını `Send_Region` (kimlere gittiği ve `isInGame()` süzgeci).
- Olası nedenler (hipotezler, sırayla ele al):
  - **H1** `WIZ_REGIONCHANGE` `Retain`'i, kısa süre önce `WIZ_USER_INOUT(IN)` ile eklenen birimi listede olmadığı için siliyor (sıra/yarış: liste sunucuda eski anlık görüntüden, IN daha yeni).
  - **H2** Yeni doğan botun `USER_INOUT(IN/RESPAWN)` yayını, o sırada henüz `isInGame()` olmayan botlara gitmiyor (doğuş zamanlaması); sonradan `REGIONCHANGE`/`REQ_USERIN` yolu da tetiklenmiyor.
  - **H3** `ParseUserInOut`/`ParseUserInfo` bazı kayıtları (ör. klan/ekipman/rütbe alan değişkenliği) reddediyor: paket sessizce düşüyor.
  - **H4** `TickUserIn` istekleri CLI-19 aralığı veya `PendingIds::Peek` (tabloda "biliniyor" sandığı kimlikleri atlama) yüzünden hiç yollanmıyor.

## 3. Kapsam

**Yapılacaklar**

1. **Sayaçlar** (`BotSession`, atomik/`m_obsLock` altında; `/bot see` çıktısına eklenir): `inout_in`, `inout_out`, `inout_parse_fail`, `reqUserIn_recv`, `reqUserIn_units`, `reqUserIn_parse_stop` (liste `declared` > ayrıştırılan), `region_recv`, `region_ids_last`, `region_dropped_total` (`Retain`'in sildiği birim sayısı toplamı) ve `region_dropped_last_ids` (son silinen en çok 8 kimlik), `move_unknown` (tabloda olmayan kimlik için gelen `WIZ_MOVE`), `userin_req_sent`, `userin_req_skipped_gap`. Her sayaç yalnızca alıcı yolunda artar; yeni paket yok.
2. **Tekrar üretme betikleri** (yalnızca `bots/config/` altında örnek betik/senaryo; sunucuya kod eklemez): 3 bot (`BotWP_K`, `BotMF_K`, `BotPHD_K`) için **altı doğuş sırası** ve **iki aralık** (art arda ≤ 100 ms / aralarda 3 sn) ile `spawn` + 6 sn bekleme + her bot için `see` (`bots/config/see_symmetry_<n>.txt` veya tek betik `script_see_symmetry.txt`; betik fiilleri `docs/13`/F4-19 izinli listesinde: `spawn` yasak, bu yüzden doğuşu senaryo dosyasıyla veya `BotCommands.txt` ile elle yap; betik yalnızca `see`/`snap` basar). Beklenen görünüm kümesi sunucu `list` ile (test teşhis komutu, `G` sınıfı) çapraz kontrol edilir.
3. **Kök neden raporu**: Uygulayıcı Raporu'nda her koşunun (12 koşu) tek satırlık simetri sonucu ve sayaç dökümü; hipotezlerin hangisi doğrulandı/çürütüldü.
4. **Düzeltme (yalnızca kök neden `BotCore/Perception.h` veya `BotSession`'daysa):** en küçük değişiklik (örn. H1 için `Retain` yalnızca `REGIONCHANGE` anında listede olmayan **ve** o andan önce alınmış birimleri siler: her birime `lastSeenMs` ile karşılaştırma; H3 için ayrıştırıcı düzeltmesi + gerçek bayt dizili birim testi; H4 için istek/aralık mantığı). Düzeltme için regresyon birim testi **zorunlu**. Kök neden **sunucu tarafındaysa** (H2): düzeltme **yapılmaz**; bulgu `KNOWN_ISSUES` önerisi olarak rapora yazılır, bot tarafı geçici çözümü (örn. doğuş sonrası bir kez `WIZ_REQ_USERIN` ile bölge listesini isteme) yalnızca rapor önerisi olarak kalır.

**Kapsam dışı**

- Sunucu (`GameServer/*.cpp` bot dışı, `shared/`) değişikliği; yeni komut/ini anahtarı; karar katmanı; NPC tablosu (`npcs` aynı yöntemle **yalnızca gözlenir**, bulgu varsa rapora yazılır).
- `docs/`, `KNOWN_ISSUES.md` değişikliği (Claude yapar; KI-016 zaten açık).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `GameServer/Bot/BotSession.h` | değiştir | sayaç üyeleri |
| `GameServer/Bot/BotSession.cpp` | değiştir | `OnPacket` sayaç artırımları (davranış değişmez); düzeltme gerekirse yalnızca kök neden satırı |
| `GameServer/Bot/BotManager.cpp` | değiştir | yalnızca `CommandSee` çıktısı |
| `BotCore/Perception.h` | değiştir | yalnızca kök neden `BotCore`'daysa |
| `Tests/BotCoreTests/PerceptionTests.cpp` | değiştir | düzeltme yapılırsa regresyon testi |
| `bots/config/script_see_symmetry.txt` | yeni | `see`/`snap` betiği |

Listede olmayan dosyaya dokunmak gerekirse **durup** raporda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F4-54 gece/2026-10-02`; `Durum` → `UYGULANIYOR`; açık sunucuları durdur (`./tools/run-servers.sh stop`).
2. Sayaçlar ve `/bot see` satırı; derle (§7); `ENABLED=1` (`GameServer.ini` **elle değiştirilirse geri al**: md5 öncesi/sonrası raporda).
3. 12 koşu (3 bot × 6 doğuş sırası × 2 aralık): her koşuda üç botun `see` çıktısı + `list` konumları; asimetri bulununca sayaç dökümünü kaydet. **Sunucuyu ve istemciyi bu plan uygulanırken DeepSeek açmaz:** çalışma zamanı koşularını Claude yapar (§6 K8); DeepSeek'in işi sayaçlar, betik ve (kök neden koddaysa) düzeltmedir. Uygulayıcı Raporu'na "çalışma zamanı koşusu Claude'da" yaz.
4. Doğrulama sonrası Claude, kök neden bulgusuna göre ikinci tur (düzeltme talimatı) yazar; bu plan **iki turlu** tasarlandı.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, değişen dosyalar `touch` edilince yeni uyarı yok
- [ ] K2: `./tools/build.sh Debug` rc=0, uyarı yok
- [ ] K3: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; düzeltme yapıldıysa en az 1 yeni regresyon testi, yapılmadıysa test sayısı değişmez
- [ ] K4: sayaçlar yalnızca alıcı yolunda artar; mevcut `OnPacket` davranışı (tablo güncellemeleri, eko kayıtları) **aynıdır** (`git diff` yalnızca ek satır; `-` satırı yok ya da kök neden düzeltmesi)
- [ ] K5: yeni satırlarda `g_pMain|GetUserPtr|_PARTY_GROUP|m_pUser->` yok; sayaçlara kilit dışı erişim yok
- [ ] K6: yeni ini anahtarı/komut/thread/paket yok; `ENABLED=0` davranışı değişmez; ASCII + CRLF; `git diff --check` boş
- [ ] K7: `bots/config/script_see_symmetry.txt` `./Scripts/` ayrıştırıcısından geçer (F4-19 izinli fiiller: yalnızca `see`/`snap`/`list`), 100 adım sınırı içinde
- [ ] K8 (çalışma zamanı, Claude yapar): 12 koşunun tamamında üç botun `see`'si **her yönde simetrik** (aynı bölgedeki her çift birbirini gösteriyor) **veya** asimetri bulunduysa kök neden sayaçla belgeli ve (koddaysa) düzeltmeden sonra 12 koşu simetrik; sonuç tablosu plan dosyasına ve KI-016'ya işlenir

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release && ./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-54
grep -n "region_dropped\|inout_parse_fail" GameServer/Bot/BotSession.cpp GameServer/Bot/BotManager.cpp
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3 (ASCII, CRLF, tab, Allman, İngilizce yorum). Sunucu davranışını değiştirme; yalnızca bot alıcı yolu ve görüntü.
- Teşhis için sunucu nesnelerini bota **okutma**: karşılaştırma `list` (test teşhis komutu) ile Claude'un çalışma zamanı koşusunda yapılır; sayaçlar yalnızca alınan paketleri sayar.
- Sayaçlar `docs/13` §5.2a "G" sınıfı değildir (alınan paket sayısı `O`'dur), ama karar kodunun kullanacağı alanlar değildir: yalnızca `/bot see` çıktısı.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum:
- Branch / commit'ler:
- Değişen dosyalar ve neden:
- Derleme sonucu:
- Kabul kriterleri öz-değerlendirme:
- Plandan sapmalar ve gerekçeleri:
- Açık sorular:

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)
