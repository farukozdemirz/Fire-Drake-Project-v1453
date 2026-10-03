---
name: plan-dogrula
description: DeepSeek'in (opencode) uyguladığı bir plans/ planını denetler. Plan kapsamını, kabul kriterlerini, derlemeyi, proje kurallarını ve kod kalitesini kontrol eder; plan dosyasına Doğrulama Raporu ve gerekirse DeepSeek'e verilecek düzeltme talimatı yazar.
argument-hint: "<plans/FAZ-NN-ad.md>"
disable-model-invocation: true
---

# Plan doğrula

Doğrulanacak plan: `$1`. Boşsa `plans/README.md`'de durumu `UYGULANDI` olan en eski planı seç ve kullanıcıya hangisini seçtiğini söyle.

Sen denetçisin. Amaç kodu "onaylamak" değil, planın gerçekten ve kurallara uygun biçimde yapıldığını **kanıtla** göstermek ya da eksikleri bulmaktır. Uygulayıcının raporundaki iddiaları doğrulamadan kabul etme.

## 1. Hazırlık

1. Plan dosyasını tamamen oku: kabul kriterleri, dokunulabilecek dosyalar, kapsam dışı, Uygulayıcı Raporu'nun son turu.
2. Planın durumu `UYGULANDI` değilse dur ve kullanıcıya bildir.
3. Branch ve farkları çıkar:
   - Taban: planın Branch alanındaki `(taban: X)` (yoksa `main`; gece modunda genellikle `$AUTO_INTEGRATION_BRANCH`). Aşağıda `<taban>` bu daldır.
   - `git log --oneline <taban>..bot/<FAZ>-<NN>`
   - `git diff --stat <taban>...bot/<FAZ>-<NN>`
   - `git diff <taban>...bot/<FAZ>-<NN>`
   - Branch yoksa veya commit yoksa bu bir bulgudur.
4. Çalışma ağacında commit edilmemiş değişiklik var mı bak (`git status`). Commit edilmemiş iş doğrulanmaz.

## 1b. Ön kanıt (otonom döngüde, varsa)

Döngü (`tools/auto-loop.sh` `collect_evidence`) sen başlamadan önce iki dosya üretmiş olabilir: `plans/_logs/evidence/<plan-adı>-generic.md` (betik `tools/verify-evidence.sh`: fark özeti, kodlama, yasak desen taraması, Release + Debug derleme ve test sonuçları, LLM yok) ve `<plan-adı>-plan.md` (kısıtlı DeepSeek V4 Pro "kanıt ajanı": plandaki doğrulama komutları ve çalışma zamanı adımları, ham çıktılarla). Bu dosyalar **ipucudur, kanıt değildir**: ajan uygulayıcıyla aynı model ailesindendir ve yanılabilir. Dosya yoksa ya da `head:` satırı `git rev-parse HEAD` ile aynı değilse yok say ve §2'yi tam yap. Varsa:

1. Derleme ve test sayıları (`generic.md`) HEAD aynıysa yeniden derlemeden kabul edilebilir; yine de sayıları kendi koşunla (`./tools/run-tests.sh Release --no-build`, saniyeler sürer) bir kez doğrula.
2. Kabul kriterlerinin **en az üçünü kendin yeniden doğrula** (en riskli olanlar: davranış, sunucu, eşik). Testler için tek test adıyla `--no-build` koş; çalışma zamanı kriterinde ajanın bıraktığı ham günlükleri (`Logs/`, `Logs/bots/`, `plans/_logs/evidence/`) kendin oku ve alıntılanan satırları kaynakla karşılaştır. Çalışma zamanı kanıtı yoksa, `ENGEL`/`BELİRSİZ` içeriyorsa ya da değer planla uyuşmuyorsa o kriteri tamamen sen yürüt.
3. **Kodu (`git diff`) her zaman kendin oku.** Kapsam, yasak desen, kod kalitesi, "testi geçirmek için sabitleme/gevşetme" kararı yalnızca sana aittir; kritik testlerde negatif kontrol (testi bilerek bozup kırmızıya döndür, geri al) yap.
4. Yerel ortamdan etkilenen kanıtlarda (ini md5 öncesi/sonrası, sunucu kapalı mı) ajanın yazdığına güvenme: `tools/run-servers.sh status` ve md5'i kendin bak.
5. Raporda her kriter için kanıtın kaynağını belirt: `kanıt: ön kanıt + yeniden doğrulandı (<ne>)` ya da `kanıt: kendi koşum`.

## 2. Denetim

Ayrıntılı liste: [kontrol-listesi.md](kontrol-listesi.md). Her maddeyi uygula. Özetle:

1. **Kapsam:** Değişen her dosya izin listesinde mi? Kapsam dışı iş, gereksiz yeniden düzenleme, silinen kod var mı?
2. **Kabul kriterleri:** Her kriteri **kendin** doğrula; uygulayıcının ✔ işaretine güvenme. Doğrulama komutla, kodun ilgili satırını okuyarak (dosya:satır) veya ölçümle yapılır.
3. **Derleme:** Yapı oturumu geçici branch'e geçirmeyi gerektiriyorsa önce kullanıcıya söyle.
   - `git switch bot/<FAZ>-<NN>`
   - `./tools/build.sh Release`, plan isterse `Debug` da
   - Sonra eski branch'e dön.
   - Hatalar ve **yeni** uyarılar not edilir.
4. **Proje kuralları** (`AGENTS.md` §2–3, `docs/03` §13, `docs/13` §3):
   - mekanik değişikliği
   - bot avantajı
   - thread kuralı
   - DB kuralı
   - dosya kodlaması / satır sonu / BOM
   - vcxproj kaydı
   - bot sistemi varsayılan kapalı mı
5. **Kod kalitesi:**
   - doğruluk hataları (sınır koşulları, null, kilit, sızıntı)
   - çevredeki koda uyum
   - gereksiz karmaşıklık
6. **Dürüstlük:** Rapordaki iddialar (derleme çıktısı, "test geçti") gerçekle uyuşuyor mu?

## 3. Karar

| Karar | Ne zaman |
|---|---|
| `DOĞRULANDI` | Tüm kabul kriterleri kanıtla karşılandı, derleme temiz, kural ihlali yok. Küçük üslup notları engel değildir; rapora "not" olarak yazılır. |
| `DÜZELTME GEREKLİ` | En az bir kriter karşılanmadı, hata veya kural ihlali var, ya da kanıt eksik. |
| `REDDEDİLDİ` | İş planla ilgisiz veya baştan yapılması gerekiyor. Gerekçeyle kullanıcıya danış. |

## 4. Raporu yaz

Plan dosyasının "Doğrulama Raporu" bölümüne yeni `### Tur N — <tarih>` ekle; önceki turları silme. İçerik:

- karar
- incelenen commit (`git rev-parse --short bot/<FAZ>-<NN>`)
- kriter tablosu (sonuç + kanıt)
- önem sırasına göre bulgular (her biri `dosya:satır` ile)
- `DÜZELTME GEREKLİ` ise **düzeltme talimatı:** DeepSeek'e aynen verilecek, numaralı, her madde tek ve somut iş

Düzeltme talimatı şu satırla başlar:

```
plans/<FAZ>-<NN>-<ad>.md — Doğrulama Turu N düzeltmeleri. Aynı branch'te yalnızca şunları yap, sonra raporuna "Tur N+1" ekle:
```

Planın `Durum` satırını kararla güncelle: satırda **yalnızca durum sözcüğü** olsun (`DOĞRULANDI`, `DÜZELTME GEREKLİ`, `REDDEDİLDİ`); tarih, parantez veya açıklama ekleme (döngü bu satırı olduğu gibi okur; ayrıntı Doğrulama Raporu'ndadır). **Kodu kendin düzeltme.** Uygulama DeepSeek'in işidir. Kullanıcı açıkça isterse istisna olur.

## 5. Kayıtları güncelle

1. `plans/README.md` listesinde durumu güncelle.
2. `docs/STATUS.md`:
   - "Son doğrulamalar" tablosu
   - blokajlar
   - sıradaki adımlar
3. Yeni bilinen sorun bulduysan `docs/KNOWN_ISSUES.md`'ye ekle (`docs/21` §4.4).
4. Doğrulama bir mekanik bilgiyi kesinleştirdiyse ilgili dokümanı güncelle. Etiket yükseltme kuralı `docs/21` §5'tedir.
5. Faz kapısı: fazın tüm planları `DOĞRULANDI` ise kullanıcıya faz sonuç raporu (`docs/templates/PHASE_REPORT.md`) hazırlamayı öner. Faz `KABUL_EDILDI` ancak kullanıcı onayıyla yazılır.

6. **Commit:** Plan dosyası, `plans/README.md`, `docs/STATUS.md` (ve değiştirdiysen `docs/KNOWN_ISSUES.md`, ilgili doküman) değişikliklerini **plan branch'ine** commit et: mesaj `[<FAZ>-<NN>] Doğrulama raporu: <karar>`. Yalnızca bu dosyaları `git add` ile ekle (`git add -A`/`.` yok). Commit edilmemiş rapor, sıradaki planın dalı açılırken kaybolur/çakışır.
7. **Otonom mod:** `echo "${AUTO_LOOP:-0}"` `1` ise kullanıcıya soru sorulamaz. Karar `DÜZELTME GEREKLİ` ise düzeltme talimatını mutlaka plan dosyasındaki "Düzeltme talimatı" bloğuna **kod çitiyle** yaz (döngü oradan okur). `REDDEDİLDİ` kararını yalnızca gerçekten baştan yapılması gerekiyorsa ver; belirsizlikte `DÜZELTME GEREKLİ` tercih et. `plans/.aktif-plan`'a dokunma.
8. **Gece modu** (`AUTO_INTEGRATION_BRANCH` dolu): birleştirmeyi döngü betiği yapar; sen **birleştirme ve push yapma**. Derlemeden önce sunucular açıksa kapat (`tools/run-servers.sh status` → `[UP]` varsa `tools/run-servers.sh stop`; açık exe bağlayıcıyı kilitler). Çalışma zamanı testi için sunucu açtıysan bitince kapat. **İstemci (GUI) gerektiren kontroller** bu modda yapılmaz: plan bunları kapsam dışı/insan testi olarak listelemişse karar vermeyi engellemez; `docs/STATUS.md` **`## Proje sahibi testleri (bekleyen)`** bölümüne ekle. Planın kendi kabul kriteri insan testi gerektiriyorsa bunu `DÜZELTME GEREKLİ` sebebi sayma; kriteri "sabah testine ertelendi" diye işaretle ve diğer kriterlere göre karar ver. Doğrulama bitince `git switch $AUTO_INTEGRATION_BRANCH` ile entegrasyon dalına dön ve çalışma ağacını temiz bırak.
9. **Paralel hat** (`AUTO_TRACK` dolu, ör. `nav`): ikinci döngü ayrı worktree'de çalışır. **Sunuculara dokunma** (`tools/run-servers.sh` çağırma; ana hat sunucu kullanıyor olabilir). Doğrulama: `./tools/build.sh Release` (GameServer çözümü derlenmeli), `./tools/run-tests.sh`, kapsam denetimi: değişen dosyalar yalnızca `BotCore/`, `BotCoreTests`, `tools/`, `docs/`, `plans/` altında olmalı (`GameServer/`, `AIServer/`, `shared/` değişikliği = `DÜZELTME GEREKLİ`). Çalışma zamanı/sunucu kriteri bu hatta yoktur; plan koymuşsa kapsam dışı say. Kriter ve kalite eşiği ana hatla aynıdır; hız için gevşetme.
10. **Başsız modda (`AUTO_LOOP=1`) turu arka plan işine bırakarak bitirme:** `claude -p` oturumu son metin mesajınla biter; arka plan (`run_in_background`) komutunun bitiş bildirimi seni **geri çağırmaz**. Uzun komutları (derleme, test, çalışma zamanı betiği) **ön planda** çalıştır (`timeout` en çok 600000 ms; daha uzunsa betiği bölüp ardışık ön plan çağrılarıyla yürüt, durumu dosyaya yazıp `sleep` + okuma ile sorgula). "Bitince günlüğü okuyacağım" diyerek tur bitirme: karar yazılmadan biten tur, doğrulamanın boşa gitmesi demektir (F5-74 iter26 böyle kaybedildi).

## 6. Kullanıcıya özet

Kararı, kriter sonuçlarını (kaç ✔ / ✘) ve en önemli 3 bulguyu yaz.

- `DÜZELTME GEREKLİ` ise düzeltme talimatını kopyalanabilir blok olarak tekrar ver.
- `DOĞRULANDI` ise (kalıcı izin, 2026-10-02): **etkileşimli modda** (`AUTO_LOOP` 1 değilse) ayrıca sormadan plan branch'ini birleştir ve push'la, sonra planı `KAPANDI` yap ve kayıtları güncelle:

```bash
git switch main && git merge --no-ff bot/<FAZ>-<NN>   # çalışma ağacı temiz olmalı
git push origin main                                   # --force yok; yalnızca main
```

  `AUTO_LOOP=1` ise birleştirme/push **yapılmaz** (yalnızca komutu not et). Birleştirme çakışırsa dur ve kullanıcıya bildir. Faz `KABUL_EDILDI` yine yalnızca kullanıcı onayıyla yazılır. Kullanıcıya birleştirme commit'ini ve push sonucunu bildir.
