# F1-02: Zamanlama oturumu araçları (`trace-session.sh` ve `--cli` özeti)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F1 — Veri ve mekanik doğrulama (`docs/17` §2) |
| Branch | `bot/F1-02` (taban: `main`) |
| Bağımlı olduğu planlar | F1-01 (KAPANDI, `main`'de) |
| İlgili gereksinim / kabul | T-MECH-CLIENT-01..04 (`docs/15` §4.2 ve §4.2.1), Q-01, Q-02, Q-18; CLI-01..06, CLI-11, CLI-12 (`docs/03` §13) |
| Tahmini büyüklük | S (2 dosya: 1 yeni betik, 1 değişen betik) |
| Hazırlayan / tarih | Claude / 2026-10-02 |

---

## 1. Amaç

F1-01'in paket izleyicisiyle **insan istemcisinden** alınacak zamanlama kayıtlarını (proje sahibi oynar) tek komutlarla toplamak ve CLI tablosuna dönüşecek ölçümlere çevirmek:

1. `tools/trace-session.sh`: izleyicili sunucuyu hazırlar, her senaryonun kaydını ayrı dosyaya ayırır, oturumu kapatıp normal derlemeye döner.
2. `tools/packet-trace-summary.py --cli`: kayıt dosyasını `docs/03` §14'teki paket düzenlerine göre **çözer** ve CLI-01..06, CLI-11, CLI-12, Q-18 için sayı üretir (yüzdelik aralıklar, kombinasyon boşlukları, hız).

Bu planda oyuna girilmez ve gerçek kayıt alınmaz. Oturumu proje sahibi `docs/15` §4.2.1'deki protokolle yapar; çıkan sayıları Claude `docs/03`'e işler.

## 2. Bağlam (okunması zorunlu)

- `docs/03` §14 (paket düzenleri) ve §4.1 (`MAGIC_CASTING 1`, `MAGIC_FLYING 2`, `MAGIC_EFFECTING 3`, `MAGIC_FAIL 4`, `MAGIC_DURATION_EXPIRED 5`, `MAGIC_CANCEL 6`), §13 (CLI-01..12 neyi ölçmeyi gerektiriyor).
- `docs/15` §4.2.1: insan oturum protokolü (bu planın araçlarını hangi sırayla kullandığı). Komut adları ve alt komutlar burada **aynen** geçer.
- `plans/F1-01-paket-izleyici.md`: log biçimi (7 sütun, sekme ayraçlı): `t_ms  sid  name  zone  opcode_hex  len  payload_hex`. `t_ms` sunucu sürecinin ilk izlenen paketinden itibaren `steady_clock` milisaniyesidir (süreç yeniden başlarsa sıfırlanır). `payload_hex` en çok ilk 64 bayt, `len==0` ise `-`. Çok baytlı alanlar **little-endian** (x86 ham bellek). Mevcut `tools/packet-trace-summary.py` (223 satır) bu biçimi ayrıştırıyor; yeni mod onun **üstüne** eklenir.
- `tools/run-servers.sh`: `start --config Release` sunucuları `build/bin/x86-Release/Server/` içindeki exe'lerle, çalışma dizini `C:\dev\fdp\server` olacak şekilde açar (satır 402-411, 475-477). Bu yüzden izleyicili exe'yi başka yere kopyalamak gerekmez; log `C:\dev\fdp\server\Logs\PacketTrace_<gün>_<ay>_<yıl>.log` olur (`GameServer/PacketTrace.cpp`'de `./Logs/`). `stop` açık istemci varsa reddeder (`--force` ister).
- `tools/build.sh Release --packet-trace`: izleyicili derleme. Argümansız `Release` derleme izleyiciyi **kaldırır** (aynı `build/bin/x86-Release` klasörünü ezer).
- `tools/check-env.sh`, `tools/run-servers.sh`: Türkçe çıktı dizgeleri `$'\u00e7'` kaçışıyla yazılıyor, betik ASCII kalıyor. Aynı kalıbı kullan (`AGENTS.md` kodlama kuralları).

## 3. Kapsam

**Yapılacaklar**

- `tools/trace-session.sh` (yeni): alt komutlar `prepare`, `collect <etiket>`, `status`, `finish [--force]`.
- `tools/packet-trace-summary.py` (değişiklik): yeni `--cli` modu + `--selftest` genişletmesi. `--cli` verilmezse çıktı **bire bir şimdiki gibi** kalır.

**Kapsam dışı (yapılmayacak)**

- Oyuna girmek, gerçek kayıt almak, ölçülen değerleri `docs/**`'e yazmak.
- `GameServer/`, `AIServer/`, `shared/`, vcxproj, `docs/**`, `.gitattributes`, `AGENTS.md`, `opencode.json` değişikliği.
- Sunucu exe'lerini `C:\dev\fdp\server` içine kopyalamak veya oradaki hiçbir dosyayı değiştirmek/silmek (yalnızca `Logs/` altındaki `PacketTrace_*.log` dosyaları **okunur**).
- Ham logları git'e eklemek (içlerinde karakter adı var). Çıktı klasörü `plans/_logs/trace/` zaten `plans/.gitignore`'daki `_logs/` ile dışarıda; ayrıca bir şey ekleme.
- Paket yükü dışında bir şeyi (sohbet, isim listesi vb.) ayrıştırmak; izleyici zaten onları yazmıyor.
- Bot kodu, adalet değerlerini (`docs/03` §13) değiştirmek.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `tools/trace-session.sh` | yeni | ASCII, LF, shebang `#!/usr/bin/env bash`, `set -euo pipefail`; git modu 100755 |
| `tools/packet-trace-summary.py` | değiştir | ASCII, LF; yalnızca standart kütüphane |

`tools/*` düzenlemesi opencode'da `ask` ister. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** raporda soru olarak yaz.

## 5. Uygulama adımları

### 5.1 `tools/packet-trace-summary.py --cli`

1. Önce dosyayı oku; mevcut ayrıştırıcıyı (satır → kayıt) ve `--sid`/`--name` filtrelerini yeniden kullan. Yeni mod: `python3 tools/packet-trace-summary.py <log> --cli [--sid N] [--name X]`. `--sid`/`--name` yoksa **en çok kaydı olan sid** seçilir ve çıktının ilk satırı `cli_target: sid=<N> name=<isim> records=<n>` olur. Kayıt yoksa `cli_target: none` ve çıkış kodu 1.
2. Yardımcı: `percentiles(values)` → **nearest-rank** yöntemi: sıralı listede `p` yüzdelik için `idx = max(ceil(p/100*n) - 1, 0)`. `p5, p25, p50, p75, p95`, `min`, `max` hesapla; `n == 0` ise `n/a`. Ardışık aralık listesi için en az 2 kayıt gerekir. `top5(values)` → en sık 5 değer `değer:adet` (adet azalan, eşitlikte değer artan).
3. Her paketi, ilgili opcode'a göre `struct.unpack` ile çöz (little-endian, `payload_hex` önce `bytes.fromhex`; `-` boş yüktür). **Beklenen uzunluk tutmuyorsa** kaydı atla ve `bad_len` sayacına ekle (çıktının sonunda `bad_len: <n>`):
   - `0x08` WIZ_ATTACK: `<BBhhh` (u8 type, u8 result, i16 tid, i16 delaytime, i16 distance) = 8 bayt.
   - `0x31` WIZ_MAGIC_PROCESS: `<BIhh7h` (u8 opcode, u32 skill, i16 caster, i16 target, 7×i16 data) = 23 bayt. Daha uzun yük (>23) de kabul et (yalnızca ilk 23 bayt çözülür).
   - `0x06` WIZ_MOVE: `<HHHhB` (u16 x·10, u16 z·10, u16 y·10, i16 speed, u8 echo) = 9 bayt; konum = değer/10.0.
   - `0x22` WIZ_TARGET_HP: `<HB` (u16 uid, u8 echo) = 3 bayt.
   - `0x41` WIZ_SPEEDHACK_CHECK: yük çözülmez, yalnızca zamanı kullanılır.
4. Çıktı bölümleri (başlıklar ASCII; bu satır biçimleri `--selftest` ile doğrulanır, değerler değişebilir):
   ```
   cli_target: sid=12 name=TestChar records=134
   == CLI-01 normal saldiri (WIZ_ATTACK) ==
   ATTACK count=40
   ATTACK interval_ms p5=… p25=… p50=… p75=… p95=… min=… max=…
   ATTACK delaytime top5: 1010:35, …
   ATTACK distance top5: …
   ATTACK type top5: … result top5: …
   == CLI-02 skill ile R arasi ==
   MAGIC->ATTACK gap_ms (skill sonrasi ilk R): n=… p5=… p50=… p95=… min=… max=…
   ATTACK->MAGIC gap_ms (R sonrasi ilk skill): n=… p5=… …
   == CLI-03 cast suresi (CASTING -> EFFECTING) ==
   CAST gap_ms per skill:
     skill=<id> n=… p5=… p50=… p95=… min=… max=…
   MAGIC cancel (opcode 6) count=…
   == CLI-04 skill tekrar ==
   skill=<id> count=… interval_ms p5=… p50=… p95=… min=… max=…   (yalnizca opcode 3 EFFECTING paketleri)
   MAGIC opcode counts: 1:… 2:… 3:… 4:… 5:… 6:…
   == CLI-06 pot (skill >= 490000, yalnizca EFFECTING) ==
   POT count=…  interval_ms p5=… p50=… p95=… min=… max=…   (tum potlar birlikte)
   POT per skill: <id>:<adet>, …
   == CLI-05 hareket (WIZ_MOVE) ==
   MOVE count=…
   MOVE interval_ms p5=… p50=… p95=… min=… max=…
   MOVE speed top5: …   (i16 speed alani)
   MOVE echo top5: …
   MOVE packets_per_second min=… avg=… max=…
   MOVE run_speed_mps median=… p95=… n=…   (konum degisimi/zaman; yalnizca speed != 0 ve ardisik iki move arasi dt <= 2000 ms olan ciftler)
   == CLI-11 aksiyon hizi ==
   ACTIONS (ATTACK+MAGIC opcode 1 ve 3) per_second max=… p95=…
   == CLI-12 / Q-02 speedhack check ==
   SPEEDHACK count=… interval_ms p5=… p50=… p95=… min=… max=…
   == Q-18 hedef HP istegi ==
   TARGETHP count=… interval_ms p5=… p50=… p95=… min=… max=…
   bad_len: 0
   ```
   Ayrıntılar:
   - **MAGIC->ATTACK:** `opcode == 3` (EFFECTING) bir magic paketinden sonra gelen **ilk** WIZ_ATTACK ile aradaki ms. Aynı magic paketi için bir kez sayılır. **ATTACK->MAGIC:** bir WIZ_ATTACK'tan sonra gelen ilk EFFECTING.
   - **CAST gap:** aynı `skill` için bir `opcode==1` (CASTING) paketinden sonra gelen ilk `opcode==3` paketine kadar süre; en çok 10 000 ms; eşleşmeyen CASTING sayılmaz. Skill kimliği başına ayrı satır.
   - **Hız:** ardışık iki WIZ_MOVE arasında `dist = hypot(dx, dz)` (metre, değer/10), `dt` saniye; `speed != 0` ve `dt <= 2.0` ise `mps = dist/dt`. `n` bu çift sayısı. `dt <= 0` ise atla.
   - **packets_per_second:** WIZ_MOVE'lar `t_ms // 1000` kovalarına bölünür; yalnızca en az bir paket olan saniyeler sayılır (mevcut `packets_per_second` kodunu yeniden kullanabilirsin).
   - **ACTIONS:** WIZ_ATTACK ve WIZ_MAGIC_PROCESS (`opcode` 1 veya 3) paketleri `t_ms // 1000` kovalarında sayılır; en çok ve p95 değeri.
   - Boş bölümde (`n/a`) satır yine yazılır; hata verme.
5. `--selftest` genişlet: bellekte sentetik bir kayıt dizisi üret (WIZ_ATTACK, WIZ_MAGIC_PROCESS 1/3/6, bir pot skill'i 490001, WIZ_MOVE'lar, TARGET_HP, SPEEDHACK; en az bir kasıtlı yanlış uzunluklu paket) ve `--cli` çıktısında şunları `assert` ile doğrula: `ATTACK count`, bir `ATTACK interval_ms p50`, `ATTACK delaytime top5`, bir `CAST gap_ms` p50 (bilinen 300 ms), `POT count`, `MOVE run_speed_mps median` (bilinen konum ve süreden hesaplanmış), `bad_len: 1`. Sonunda `selftest OK`, çıkış 0. Eski selftest iddiaları **silinmez**.
6. `--cli` olmadan çıktı değişmemeli: eski sentetik örnek (plan F1-01 raporundaki 8 satırlık log) ile önceki çıktının aynısını ver.

### 5.2 `tools/trace-session.sh`

Genel: `set -euo pipefail`; `ROOT="$(cd "$(dirname "$0")/.." && pwd)"`; `FDP_RUNTIME_DIR="${FDP_RUNTIME_DIR:-/mnt/c/dev/fdp}"`; `TRACE_OUT_DIR="${TRACE_OUT_DIR:-$ROOT/plans/_logs/trace}"`; log klasörü `$FDP_RUNTIME_DIR/server/Logs`. Türkçe mesajlar `$'\u..'` kaçışıyla (dosya ASCII). Çıkış kodları: 0 başarı, 1 başarısız/reddedildi, 2 kullanım hatası. Üstte `run-servers.sh` tarzı kısa kullanım yorumu ve `usage()`; `-h/--help` kullanımı yazar, çıkış 0; bilinmeyen alt komut → kullanım + çıkış 2.

Durum dosyası: `$TRACE_OUT_DIR/.session`, her satır `<log-dosya-adi><TAB><bayt-boyutu>`. Yalnızca `PacketTrace_*.log` dosyaları.

1. **`prepare`**
   1. `"$ROOT/tools/run-servers.sh" stop` çalıştır. Çıkış kodu 0 değilse (ör. açık istemci nedeniyle reddedildi) hata mesajı yazıp **çıkış 1**. (`--force` geçirme.)
   2. `"$ROOT/tools/build.sh" Release --packet-trace`; başarısızsa çıkış 1.
   3. `"$ROOT/tools/run-servers.sh" start --config Release`; başarısızsa çıkış 1.
   4. Oturum anlık görüntüsü: `mkdir -p "$TRACE_OUT_DIR"`; log klasöründeki her `PacketTrace_*.log` için `ad<TAB>boyut` satırını `.session`'a yaz (dosya yoksa boş `.session`). Böylece önceki günlerin/oturumların satırları sonraki `collect`'e karışmaz.
   5. Kısa talimat yaz: izleyicili sunucular ayakta; şimdi istemciyle oyuna girin; her senaryodan sonra `tools/trace-session.sh collect <etiket>`; bitince `tools/trace-session.sh finish`. Uyarı: `build/bin/x86-Release/Server/` şu an izleyicili sürüm; `finish` normal sürüme döndürür.
2. **`collect <etiket>`** — sunucu gerektirmez (yalnızca dosya okur):
   1. `<etiket>` `^[A-Za-z0-9_-]{1,40}$` değilse çıkış 2. `.session` yoksa "önce prepare" deyip çıkış 1. `$TRACE_OUT_DIR/<etiket>.log` zaten varsa üzerine yazma, çıkış 1.
   2. Log klasöründeki `PacketTrace_*.log` dosyalarını **değiştirilme zamanına göre eskiden yeniye** sırala. Her biri için `.session`'daki ofseti oku (kayıtlı değilse 0); dosya boyutu ofsetten büyükse `tail -c +$((ofset+1))` ile yalnızca yeni baytları `<etiket>.log` dosyasına **ekle**. Dosya ofsetten küçükse (kırpılmış) 0'dan al.
   3. Toplanan bayt 0 ise `<etiket>.log`'u sil, "yeni kayit yok" yaz, çıkış 1.
   4. `.session`'ı güncel boyutlarla **yeniden yaz** (yeni dosyalar dahil). Böylece sıradaki `collect` yalnızca sonraki kaydı alır.
   5. `python3 "$ROOT/tools/packet-trace-summary.py" "$TRACE_OUT_DIR/<etiket>.log" --cli` çıktısını hem ekrana yaz hem `<etiket>.summary.txt` dosyasına kaydet (`tee`). Python hatasında çıkış 1.
3. **`status`**: `.session` varsa her satırı ve log klasöründeki güncel boyutu yaz (kaydedilmemiş yeni bayt sayısı), `$TRACE_OUT_DIR` altındaki `*.log` dosyalarını boyutlarıyla listele; yoksa "oturum yok" yaz. Çıkış 0. (`run-servers.sh status` çıktısını da en sonda çağırıp ekrana ekle; onun çıkış kodunu yok say.)
4. **`finish [--force]`**:
   1. `run-servers.sh stop` (`--force` verildiyse geçir). Başarısızsa çıkış 1 (derleme yapma).
   2. `"$ROOT/tools/build.sh" Release` (izleyicisiz). Başarısızsa çıkış 1.
   3. `.session`'ı sil. `$TRACE_OUT_DIR` altındaki kayıtların yerini ve "bu dosyalarda karakter adı var, paylaşmayın/git'e eklemeyin" uyarısını yaz.
   4. Normal sunucuları açmak için `tools/run-servers.sh start` komutunu hatırlat (kendin başlatma).
5. Betiği `chmod +x` yap ve git modunu ayarla: `git update-index --add --chmod=+x tools/trace-session.sh` (commit'ten önce).

## 6. Kabul kriterleri

- [ ] K1: `python3 tools/packet-trace-summary.py --selftest` → `selftest OK`, çıkış 0 (genişletilmiş iddialar dahil).
- [ ] K2: Elle yazılmış 12–20 satırlık sentetik logla (`printf` ile geçici dosyada, depoya eklenmez) `--cli` çıktısı rapora yapıştırılır; çıktıda §5.1 adım 4'teki tüm bölüm başlıkları ve `cli_target`, `bad_len` satırları var; bildiğin değerler (ör. bir CAST aralığı, bir pot aralığı, hareket hızı) elle hesapla ve çıktıyla karşılaştır; hesabı raporda göster.
- [ ] K3: `--cli` **olmadan** F1-01 raporundaki 8 satırlık örnek logda çıktı bire bir önceki gibi (rapora iki çıktıyı yapıştır: eski `git stash` ile değil, `git show main:tools/packet-trace-summary.py > /tmp/...` ile alınan eski betikle karşılaştır; `diff` boş).
- [ ] K4: Bozuk girdide çökmez: boş log → `cli_target: none`, çıkış 1; yalnızca bozuk satırlar içeren log → çökme yok, `bad_len`/atlanan satır sayısı görünür.
- [ ] K5: `bash -n tools/trace-session.sh` hatasız; `tools/trace-session.sh --help` çıkış 0; `tools/trace-session.sh bilinmeyen` çıkış 2; `file tools/trace-session.sh` → ASCII, CR yok; `git ls-files -s tools/trace-session.sh` → `100755`.
- [ ] K6: **Çevrimdışı `collect` testi** (sunucu çalıştırmadan): geçici bir `FDP_RUNTIME_DIR` (`$TMP/fdp/server/Logs/`), geçici `TRACE_OUT_DIR` ve elle yazılmış bir `PacketTrace_1_1_2026.log` ile: (a) `.session` yokken `collect x` → çıkış 1; (b) boş `.session` oluştur, `collect a1` → log'un tamamı `a1.log` olur, özet basılır, `a1.summary.txt` var; (c) log'a satır ekle, `collect a2` → yalnızca yeni satırlar `a2.log`'da; (d) hemen tekrar `collect a3` → "yeni kayit yok", çıkış 1, `a3.log` yok; (e) `collect a1` tekrar → çıkış 1 (üzerine yazmaz); (f) `collect "kotu etiket!"` → çıkış 2. Her adımın çıkış kodu ve çıktısı rapora yapıştırılır.
- [ ] K7: `prepare` ve `finish` **çalıştırılmaz** (sunucu/derleme gerektirir; çalışma zamanı doğrulamasını Claude yapar). Bunun yerine kodu satır satır savun: raporda `prepare` ve `finish`'in hangi komutları hangi sırayla çağırdığını (dosya:satır) listele ve `run-servers.sh stop` reddinde (`exit 1`) derlemeye geçilmediğini göster.
- [ ] K8: Kapsam: `git diff --stat main...bot/F1-02` yalnızca `tools/trace-session.sh`, `tools/packet-trace-summary.py` ve bu plan dosyası; `GameServer/`, `docs/`, `shared/` yok. `git status --short` boş (geçici dosyalar `/tmp` altında ve silindi).

## 7. Doğrulama komutları

```bash
python3 tools/packet-trace-summary.py --selftest
bash -n tools/trace-session.sh && tools/trace-session.sh --help; echo "exit=$?"
file tools/trace-session.sh tools/packet-trace-summary.py
git ls-files -s tools/trace-session.sh
git diff --stat main...bot/F1-02
git status --short
```

(`./tools/build.sh` bu planda gerekmez; `GameServer/` değişmiyor.)

## 8. Kısıtlar ve uyarılar

- Kodlama: `tools/*.sh` ve `.py` ASCII + LF. Türkçe metinleri `$'\u..'` ile yaz (bash) veya Python'da sade ASCII İngilizce/Türkçe-karaktersiz bırak.
- **Gizlilik:** `collect` ham log kopyalar; içinde karakter adı olabilir. Çıktı klasörü `plans/_logs/trace/` (git dışı). Raporuna gerçek bir log satırı yapıştırma; yalnızca kendi sentetik verini yapıştır.
- `prepare`/`finish` sunucuları durdurup derleme yapar; bunları plan sırasında **çalıştırma** (K7). Kodu yazarken `stop` ve `build` çağrılarının başarısız çıkışlarında devam edilmemesine dikkat et (`set -e` tek başına yetmeyebilir: komut `if`/`||` içindeyse açık kontrol yaz).
- Yeni karar gerekirse (ör. başka bir log yeri) **durup** Uygulayıcı Raporu'nda sor; `docs/` veya `GameServer/` değiştirme.
- Bu plan CLI değerlerini **değiştirmez**; sayıları üretir. Ölçüm yorumu (ör. "R aralığı 1,0 s mi?") Claude'un işidir.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: —
- Branch / commit'ler: —
- Değişen dosyalar ve neden: —
- Kabul kriterleri öz-değerlendirme: —
- Plandan sapmalar ve gerekçeleri: —
- Açık sorular: —

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(henüz yok)
